/* vtwin: one terminal bound to one window; see vtwin.h. Moved out of the
 * XCON: handler (plan 2026-09-30-console-device.md, DX1/DX2): the same code,
 * with the owner's policy behind vtwin_host. */
#include "vtwin.h"
#include <string.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/inputevent.h>
#include <devices/console.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/console.h>
#include "../handler/clip.h"
#include "fontpair.h"
#include <graphics/displayinfo.h>

extern struct GfxBase *GfxBase;
/* RawKeyConvert is a macro that calls into the console device at
 * CONSOLE_BASE_NAME (ConsoleDevice by default), so it needs the base here.
 * Both programs that link vtwin.c define it: the handler from its console
 * device packet, upcon_device.c from the ROM device it fronts. */
extern struct Device *ConsoleDevice;

#define FRAME_MICROS 50000 /* 20 frames per second */
#define SYNC_FRAMES 3       /* the longest a ?2026 frame is waited for */

static void frame_start(vtwin *w);

/* ---- engine callbacks ------------------------------------------------------- */

static void cb_damage(void *u, int x0, int y0, int x1, int y1)
{
    vr_damage(&((vtwin *)u)->r, x0, y0, x1, y1);
}

static void cb_scroll(void *u, int top, int bot, int n)
{
    vr_scroll(&((vtwin *)u)->r, top, bot, n);
}

static void cb_reply(void *u, const vt_u8 *b, long n)
{
    vtwin *w = (vtwin *)u;
    w->host->reply(w->user, b, n);
}

/* The profile's bell: 1 beeps, 2 reverses the screen for one frame (50 ms;
 * vtwin_tick ends the flash on the next frame, in the renderer only --
 * the program's own reverse-video mode is not touched). */
static void cb_bell(void *u)
{
    vtwin *w = (vtwin *)u;
    if (w->bell == 1)
        DisplayBeep(w->win ? w->win->WScreen : 0);
    else if (w->bell == 2 && w->t) {
        vr_bell_flash(&w->r);
        w->render_pending = 1;
        frame_start(w);
    }
}

/* The title where it shows: the window's title bar, or -- a borderless
 * full-screen window has none -- the screen's (a title given to such a
 * window makes Intuition draw a title bar over the text). */
void vtwin_show_title(vtwin *w)
{
    if (!w->win)
        return;
    if (w->title_on_screen)
        SetWindowTitles(w->win, (UBYTE *)~0, (UBYTE *)w->title);
    else
        SetWindowTitles(w->win, (UBYTE *)w->title, (UBYTE *)~0);
}

static void cb_title(void *u, const char *s)
{
    vtwin *w = (vtwin *)u;
    int i;
    /* the title arrives as UTF-8; Intuition shows Latin-1 */
    for (i = 0; *s && i < (int)sizeof(w->title) - 1; s++) {
        unsigned char b = (unsigned char)*s;
        if (b < 0x80) {
            w->title[i++] = (char)b;
        } else if ((b & 0xE0) == 0xC0 && s[1]) {
            unsigned cp = ((b & 0x1F) << 6) | (s[1] & 0x3F);
            w->title[i++] = (char)(cp < 0x100 ? cp : '?');
            s++;
        } else if ((b & 0xC0) != 0x80) {
            w->title[i++] = '?';
        }
    }
    w->title[i] = 0;
    if (w->win && !w->r.off)
        vtwin_show_title(w);
    if (w->host->titled)
        w->host->titled(w->user);
}

/* The engine's idea of the default colours: what the pens show. */
static void report_defaults(vtwin *w)
{
    ULONG fg = vr_pen_rgb(&w->r, w->r.pen_default_fg), bg = vr_pen_rgb(&w->r, w->r.pen_default_bg);
    vt_set_default_colors(w->t, fg, bg, fg);
}

/* A program changed the palette or the default colours (OSC 4, 10-12):
 * new pens, then the whole window redrawn (the engine marked it). With a
 * profile cursor colour, OSC 12's new cursor colour follows through. */
static void cb_colors(void *u)
{
    vtwin *w = (vtwin *)u;
    ULONG fg = vt_default_color(w->t, 0), bg = vt_default_color(w->t, 1);
    vr_palette_changed(&w->r);
    vr_set_defaults(&w->r, fg, bg);
    if (w->cursor_rgb != VR_KEEP)
        vr_set_cursor_color(&w->r, vt_default_color(w->t, 2));
}

/* Amiga page length, line length and offsets (CSI t / u / x / y): the text
 * area changes, as the console recomputes it (-1: back to automatic). The
 * resize happens after the write that asked for it (see vtwin_write()). */
static void cb_layout(void *u, int which, int value)
{
    vtwin *w = (vtwin *)u;
    switch (which) {
    case VT_LAYOUT_PAGE_LENGTH: w->r.lay_rows = (WORD)value; break;
    case VT_LAYOUT_LINE_LENGTH: w->r.lay_cols = (WORD)value; break;
    case VT_LAYOUT_LEFT_OFFSET: w->r.lay_x = (WORD)value; break;
    case VT_LAYOUT_TOP_OFFSET: w->r.lay_y = (WORD)value; break;
    case VT_LAYOUT_COLUMNS: w->want_cols = (WORD)value; break;
    default: return;
    }
    w->layout_dirty = 1;
}

/* ---- lifetime ----------------------------------------------------------------- */

void vtwin_profile_defaults(vtwin *w)
{
    int i;
    w->bell = 1;          /* beep; a profile may choose none or a flash */
    w->bold_bright = 1;   /* xterm SGR 1 takes the bright 8-15 */
    w->wheel_scroll = 1;  /* the wheel moves through the scrollback */
    w->cursor_rgb = VR_KEEP;
    w->sel_fg_rgb = w->sel_bg_rgb = VR_KEEP;
    /* every other profile field too: a window switching profiles must not
     * keep the last one's cursor or palette */
    w->cursor_style = 0;
    w->cursor_blink = 0;
    w->meta_alt = 0;
    w->copy_on_select = 0;
    for (i = 0; i < 16; i++)
        w->pal16[i] = 0;
    w->fallback[0] = 0;
}

/* The outline font follows fallback: opened, changed or closed so the
 * renderer has the one the spec names (0 when it cannot be opened: the
 * cells look as without one). 1 when it changed. */
static int outline_sync(vtwin *w)
{
    if (w->outline && !strcmp(vo_name(w->outline), w->fallback))
        return 0;
    if (!w->outline && !w->fallback[0])
        return 0;
    vr_set_outline(&w->r, 0);
    if (w->outline)
        vo_close(w->outline);
    w->outline = w->fallback[0] && w->font
                     ? vo_open(w->fallback, w->font->tf_XSize, w->font->tf_YSize, w->font->tf_Baseline)
                     : 0;
    vr_set_outline(&w->r, w->outline);
    return 1;
}

void vtwin_init(vtwin *w, const vtwin_host *host, void *user)
{
    w->host = host;
    w->user = user;
    w->find_next = VT_ROW_NONE;
    vti_mouse_reset(&w->mouse);
    /* every owner starts from the historical look: a console.device unit
     * (MEMF_CLEAR'd) had a silent bell, a black cursor and black-on-black
     * selection (2026-10-02 review) */
    vtwin_profile_defaults(w);
    w->frame_port = CreateMsgPort();
    if (w->frame_port) {
        w->frame = (struct timerequest *)CreateIORequest(w->frame_port, sizeof(struct timerequest));
        if (w->frame && !OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)w->frame, 0))
            w->frame_open = 1;
    }
}

static void frame_stop(vtwin *w)
{
    if (w->frame_busy) {
        AbortIO((struct IORequest *)w->frame);
        WaitIO((struct IORequest *)w->frame);
        w->frame_busy = 0;
    }
}

void vtwin_cleanup(vtwin *w)
{
    frame_stop(w);
    if (w->frame_open)
        CloseDevice((struct IORequest *)w->frame);
    w->frame_open = 0;
    if (w->frame)
        DeleteIORequest((struct IORequest *)w->frame);
    w->frame = 0;
    if (w->frame_port)
        DeleteMsgPort(w->frame_port);
    w->frame_port = 0;
}

ULONG vtwin_sigmask(const vtwin *w)
{
    return w->frame_port ? 1UL << w->frame_port->mp_SigBit : 0;
}

static void frame_start(vtwin *w)
{
    if (!w->frame_open || w->frame_busy)
        return;
    w->frame->tr_node.io_Command = TR_ADDREQUEST;
    w->frame->tr_time.tv_secs = 0;
    w->frame->tr_time.tv_micro = FRAME_MICROS;
    SendIO((struct IORequest *)w->frame);
    w->frame_busy = 1;
}

void vtwin_render(vtwin *w)
{
    if (!w->render_pending || !w->t)
        return;
    if ((vt_modes(w->t) & VT_MODE_SYNC) && w->sync_held < SYNC_FRAMES) {
        /* synchronized output (?2026): the program is in the middle of a
         * frame. Nothing is drawn until it says the frame is whole -- or
         * three frames have passed, should it never say so. (A frame sent
         * in several writes showed its top new and its bottom old.) */
        w->sync_held++;
        frame_start(w);
        return;
    }
    w->sync_held = 0;
    w->render_pending = 0;
    vr_mask_begin(&w->r); /* planar screens: only the planes in use (S1) */
    vr_cursor_off(&w->r);
    vt_flush(w->t);
    vr_cursor_on(&w->r);
    vr_mask_end(&w->r);
    if (w->r.has_blink || vr_cursor_blinks(&w->r))
        frame_start(w); /* blinking cells or cursor: the frames keep coming */
}

static void drag_to(vtwin *w, WORD mx, WORD my);
static void motion_to(vtwin *w, WORD mx, WORD my);
static void pointer_sync(vtwin *w);

void vtwin_tick(vtwin *w)
{
    if (w->frame_busy && CheckIO((struct IORequest *)w->frame)) {
        WaitIO((struct IORequest *)w->frame);
        w->frame_busy = 0;
        vtwin_render(w); /* the frame is due */
        if (w->t && (vr_flash_tick(&w->r) || vr_blink_tick(&w->r)))
            frame_start(w); /* that frame ended a flash or a blink phase */
    }
    if (!w->frame_open) {
        vtwin_render(w); /* no frame clock: draw at once */
        if (w->t && vr_flash_tick(&w->r))
            vtwin_render(w); /* the flash's frame is over */
    }
    if (w->dragging && w->win) {
        /* follow the pointer on the frame clock (after the frame's own
         * bookkeeping, or frame_start finds it busy and the polling stops):
         * moves may never reach us as events -- Intuition keeps absolute
         * pointer moves to itself */
        drag_to(w, w->win->MouseX, w->win->MouseY);
        frame_start(w);
    } else if (w->mouse.held && w->win && w->t && vti_wants_motion(&w->mouse, w->t)) {
        motion_to(w, w->win->MouseX, w->win->MouseY); /* the same for a program's ?1002 drag */
        frame_start(w);
    }
}

/* A fixed-width font by name ("topaz" or "topaz.font") and size; 0 if it
 * cannot be opened or is proportional. */
/* OpenDiskFont in a process of its own: diskfont reads FONTS: (DOS calls),
 * and a handler's own DOS call shares its process port with the packets
 * DOS sends it, so the reply goes astray -- the call failed and every font
 * came back as the nearest one in memory (rig 2026-10-03: /font topaz 11
 * and View > Bigger font drew topaz 8). The worker opens the font and
 * ends; the font is a system object, the caller closes it. */
struct font_job {
    struct Message msg;
    struct TextAttr ta;
    char name[48];
    struct TextFont *font;
};

static void font_worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct font_job *j;
    struct Library *DiskfontBase;
    WaitPort(&me->pr_MsgPort);
    j = (struct font_job *)GetMsg(&me->pr_MsgPort);
    me->pr_WindowPtr = (APTR)-1; /* a missing font is an answer, not a requester */
    if ((DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 36)) != 0) {
        j->font = OpenDiskFont(&j->ta);
        CloseLibrary(DiskfontBase);
    }
    Forbid(); /* the caller frees j: end before it can run on */
    ReplyMsg(&j->msg);
}

static struct TextFont *disk_font(const struct TextAttr *ta)
{
    struct font_job j;
    struct MsgPort *reply = CreateMsgPort();
    struct Process *p;
    if (!reply)
        return 0;
    memset(&j, 0, sizeof(j));
    strncpy(j.name, (const char *)ta->ta_Name, sizeof(j.name) - 1);
    j.ta = *ta;
    j.ta.ta_Name = (STRPTR)j.name;
    j.msg.mn_ReplyPort = reply;
    j.msg.mn_Length = sizeof(j);
    p = CreateNewProcTags(NP_Entry, (ULONG)font_worker, NP_Name, (ULONG)"UP-Term font", NP_StackSize, 8192,
                          TAG_DONE);
    if (p) {
        PutMsg(&p->pr_MsgPort, &j.msg);
        WaitPort(reply);
        GetMsg(reply);
    }
    DeleteMsgPort(reply);
    return j.font;
}

/* A font by name ("topaz" or "topaz.font") and size, fixed width only:
 * from memory, else from disk. designed: only a size the font has on disk
 * (diskfont scales one otherwise; FPF_DESIGNED in the request says no). */
static struct TextFont *open_font(const char *fontname, WORD fontsize, int designed)
{
    struct TextFont *f = 0;
    if (fontname[0]) {
        struct TextAttr ta;
        char name[48];
        int i;
        for (i = 0; fontname[i] && i < (int)sizeof(name) - 7; i++)
            name[i] = fontname[i];
        name[i] = 0;
        if (i < 5 || (name[i - 5] != '.' || (name[i - 4] | 32) != 'f' || (name[i - 3] | 32) != 'o' ||
                      (name[i - 2] | 32) != 'n' || (name[i - 1] | 32) != 't'))
            strcpy(name + i, ".font");
        ta.ta_Name = (STRPTR)name;
        ta.ta_YSize = (UWORD)(fontsize ? fontsize : 8);
        ta.ta_Style = 0;
        ta.ta_Flags = designed ? FPF_DESIGNED : 0;
        f = OpenFont(&ta);
        if (!f || f->tf_YSize != ta.ta_YSize) {
            if (f)
                CloseFont(f);
            f = disk_font(&ta);
        }
        if (f && ((f->tf_Flags & FPF_PROPORTIONAL) || (designed && f->tf_YSize != ta.ta_YSize))) {
            CloseFont(f);
            f = 0;
        }
    }
    return f;
}

static struct TextFont *open_named(const char *fontname, WORD fontsize)
{
    return open_font(fontname, fontsize, 0);
}

/* a and b the same name, any case, ".font" optional on one of them */
static int strcmp_nocase(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (x != y)
            break;
    }
    if (!*a && !*b)
        return 0;
    if (!*a)
        return strcmp(b, ".font") && strcmp(b, ".FONT");
    if (!*b)
        return strcmp(a, ".font") && strcmp(a, ".FONT");
    return 1;
}

/* The font a pair (render/fontpair) gives this screen for the font
 * asked for -- the system's default font when none was -- or 0 when it
 * is the font asked for, or not installed. */
static struct TextFont *aspect_font(vtwin *w, char *name, int max, WORD *size)
{
    fontpair_choice ch;
    const char *base = w->fontname;
    int bsize = w->fontsize;
    struct TextFont *f;
    if (!w->aspect_known || w->aspect_off)
        return 0;
    if (!base[0]) {
        base = (const char *)GfxBase->DefaultFont->tf_Message.mn_Node.ln_Name;
        bsize = GfxBase->DefaultFont->tf_YSize;
    }
    fontpair_choose(base, bsize ? bsize : 8, w->square, w->square_fits, &ch);
    if (!ch.changed)
        return 0;
    f = open_named(ch.name, (WORD)ch.size);
    if (f && name) {
        strncpy(name, ch.name, (size_t)max - 1);
        name[max - 1] = 0;
        *size = (WORD)ch.size;
    }
    return f;
}

void vtwin_set_screen(vtwin *w, struct Screen *scr)
{
    struct DisplayInfo di;
    ULONG mode;
    w->aspect_known = 0;
    if (!scr)
        return;
    mode = GetVPModeID(&scr->ViewPort);
    if (mode == (ULONG)INVALID_ID ||
        !GetDisplayInfoData(0, (UBYTE *)&di, sizeof(di), DTAG_DISP, mode))
        return;
    w->square = di.Resolution.x == di.Resolution.y;
    /* 25 rows of 16-pixel cells under the screen's title bar and a window's */
    w->square_fits = scr->Height >= 25 * 16 + 2 * (scr->BarHeight + 1) + 4;
    w->aspect_known = 1;
}

int vtwin_fit_aspect(vtwin *w)
{
    char name[40], base[40];
    WORD size = 0, bsize = w->fontsize;
    struct TextFont *f;
    const char *now;
    if (!w->t || !w->win || w->given_font || !w->font)
        return 0;
    /* the font this screen wants: the pair's other one, or the one asked
     * for -- compared with the font drawn NOW (a move from square pixels
     * back to tall ones has TopazPro open while topaz was asked for: it
     * kept the 16-pixel font, rig 2026-10-03) */
    f = aspect_font(w, name, sizeof(name), &size);
    if (f)
        CloseFont(f); /* vtwin_set_font opens it again for the window */
    else {
        strncpy(base, w->fontname, sizeof(base) - 1);
        base[sizeof(base) - 1] = 0;
        if (!base[0]) {
            strncpy(base, (const char *)GfxBase->DefaultFont->tf_Message.mn_Node.ln_Name, sizeof(base) - 1);
            bsize = GfxBase->DefaultFont->tf_YSize;
        }
        strncpy(name, base, sizeof(name));
        size = bsize ? bsize : 8;
    }
    now = (const char *)w->font->tf_Message.mn_Node.ln_Name;
    if (now && size == w->font->tf_YSize && !strcmp_nocase(now, name))
        return 0; /* already that one */
    return vtwin_set_font(w, name, size);
}

struct TextFont *vtwin_open_font(vtwin *w)
{
    struct TextFont *f;
    if (w->given_font) {
        w->font_opened = 0;
        return w->font = w->given_font;
    }
    if ((f = aspect_font(w, 0, 0, 0)) != 0) {
        /* the pair's other font for this screen's pixels (P2) */
        w->font_opened = 1;
        return w->font = f;
    }
    f = open_named(w->fontname, w->fontsize);
    w->font_opened = f != 0;
    /* else the system default font: the one the user chose for text, as
     * the Shell uses it. It is fixed width by definition. */
    w->font = f ? f : GfxBase->DefaultFont;
    return w->font;
}

/* The spec's settings into the engine and the renderer (vtwin_attach,
 * vtwin_apply_settings). Every field is set, so one turned back off takes
 * effect in a live window too. */
static void settings(vtwin *w)
{
    int i;
    vr_set_defaults(&w->r, w->fg_rgb, w->bg_rgb);
    report_defaults(w);
    /* after report_defaults(): the default colours must be in place so a
     * palette change can re-derive the pens through cb_colors() */
    vt_set_bold_bright(w->t, w->bold_bright);
    vt_set_cursor_style(w->t, w->cursor_style);
    vt_set_cursor_blink(w->t, w->cursor_blink);
    for (i = 0; i < 16; i++)
        if (w->pal16[i] & 0x01000000UL)
            vt_set_palette(w->t, i, w->pal16[i] & 0xFFFFFFUL);
        else
            vt_clear_palette(w->t, i);
    if (w->cursor_rgb != VR_KEEP) {
        vt_set_default_colors(w->t, vt_default_color(w->t, 0),
                              vt_default_color(w->t, 1), w->cursor_rgb);
        vr_set_cursor_color(&w->r, w->cursor_rgb);
    }
    vr_set_selection_colors(&w->r, w->sel_fg_rgb, w->sel_bg_rgb);
}

static void bind(vtwin *w, struct Window *win);

int vtwin_attach(vtwin *w, struct Window *win)
{
    struct vt_callbacks cb;
    w->win = win;
    if (!w->font)
        vtwin_open_font(w);
    cb.damage = cb_damage;
    cb.scroll = cb_scroll;
    cb.reply = cb_reply;
    cb.bell = cb_bell;
    cb.title = cb_title;
    cb.layout = cb_layout;
    cb.colors = cb_colors;
    {
        /* size from the window before the engine exists */
        int cols = (win->Width - win->BorderLeft - win->BorderRight) / w->font->tf_XSize;
        int rows = (win->Height - win->BorderTop - win->BorderBottom) / w->font->tf_YSize;
        w->t = vt_new(cols > 0 ? cols : 1, rows > 0 ? rows : 1,
                      w->sb_lines < 0 ? 0 : w->sb_lines ? w->sb_lines : 500, &cb, w);
    }
    if (!w->t)
        return 0;
    vt_set_personality(w->t, w->pers);
    if (w->pers == VT_XTERM)
        vt_set_onlcr(w->t, 1); /* LF out as CR LF, as a Unix tty does */
    if (w->pers == VT_XTERM && w->latin1)
        vt_set_charset(w->t, VT_CS_LATIN1);
    else if (w->pers == VT_XTERM && w->cp437)
        vt_set_charset(w->t, VT_CS_CP437);
    bind(w, win);
    return 1;
}

/* The renderer on win for the engine there is: attach's second half, and
 * all of a rebind (the window moved to another screen). The grid takes
 * the window's size when it differs. */
static void bind(vtwin *w, struct Window *win)
{
    int k;
    w->win = win;
    vr_init(&w->r, win, w->font, w->t, w->pers == VT_PCANSI ? VT_ENC_CP437 : VT_ENC_LATIN1);
    if (w->own_rp) {
        w->r.rp = w->own_rp; /* a shared window: our pens and font, its layer */
        SetFont(w->own_rp, w->font);
    }
    if (w->inset_top)
        vr_set_inset(&w->r, w->inset_top);
    for (k = 1; k <= 10; k++) {
        if (!w->alt[k] && w->altname[k][0])
            w->alt[k] = open_named(w->altname[k], w->altsize[k] ? w->altsize[k] : w->font->tf_YSize);
        vr_set_alt_font(&w->r, k, w->alt[k]);
    }
    if (w->outline)
        vr_set_outline(&w->r, w->outline); /* a rebind keeps the open outline font */
    outline_sync(w);
    settings(w);
    vt_set_cell_pixels(w->t, w->font->tf_XSize, w->font->tf_YSize);
    vr_layout(&w->r);
    if (w->r.cols != vt_cols(w->t) || w->r.rows != vt_rows(w->t)) {
        vt_resize(w->t, w->r.cols, w->r.rows);
        w->host->resized(w->user);
    }
    vr_redraw(&w->r);
    vr_cursor_on(&w->r);
}

void vtwin_unbind(vtwin *w)
{
    if (!w->t || !w->win)
        return;
    frame_stop(w);
    w->render_pending = 0;
    w->layout_dirty = 0;
    w->dragging = 0;
    vti_mouse_reset(&w->mouse);
    w->pointer_on = 0; /* the window's ReportMouse goes with it */
    vr_set_outline(&w->r, 0);
    vr_free(&w->r);
    w->win = 0;
}

int vtwin_rebind(vtwin *w, struct Window *win)
{
    if (!w->t || w->win || !win)
        return 0;
    bind(w, win);
    vtwin_show_title(w);
    return 1;
}

int vtwin_set_font(vtwin *w, const char *name, WORD size)
{
    struct TextFont *f, *old = w->font;
    int opened;
    if (!w->t || !w->win || w->given_font)
        return 0; /* a window drawn in its own font (console.device units) keeps it */
    f = name[0] ? open_named(name, size) : GfxBase->DefaultFont;
    if (!f)
        return 0;
    opened = name[0] != 0;
    vr_cursor_off(&w->r);
    vr_set_font(&w->r, f);
    if (old && w->font_opened && old != f)
        CloseFont(old);
    w->font = f;
    w->font_opened = opened;
    strncpy(w->fontname, name, sizeof(w->fontname) - 1);
    w->fontname[sizeof(w->fontname) - 1] = 0;
    w->fontsize = size;
    vt_set_cell_pixels(w->t, f->tf_XSize, f->tf_YSize);
    vr_layout(&w->r);
    vt_resize(w->t, w->r.cols, w->r.rows);
    w->host->resized(w->user);
    /* the old cells' pixels go: the new grid may not cover them */
    EraseRect(w->r.rp, w->win->BorderLeft, w->win->BorderTop + w->inset_top,
              w->win->Width - w->win->BorderRight - 1, w->win->Height - w->win->BorderBottom - 1);
    vr_redraw(&w->r);
    vr_cursor_on(&w->r);
    return 1;
}

int vtwin_set_scrollback(vtwin *w, int lines)
{
    if (!w->t)
        return 0;
    if (w->r.view)
        vr_set_view(&w->r, 0); /* the old view may be past the new size */
    if (!vt_set_scrollback(w->t, lines))
        return 0;
    w->sb_lines = lines ? lines : -1; /* the spec's encoding: 0 is the built-in 500 */
    return 1;
}

void vtwin_show(vtwin *w, int on)
{
    if (!w->t || !w->win)
        return;
    vr_set_off(&w->r, !on);
    if (!on)
        return;
    vr_layout(&w->r);
    if (w->r.cols != vt_cols(w->t) || w->r.rows != vt_rows(w->t)) {
        vt_resize(w->t, w->r.cols, w->r.rows); /* the window changed while we were away */
        w->host->resized(w->user);
    }
    vtwin_show_title(w);
    EraseRect(w->r.rp, w->win->BorderLeft, w->win->BorderTop + w->inset_top,
              w->win->Width - w->win->BorderRight - 1, w->win->Height - w->win->BorderBottom - 1);
    vr_redraw(&w->r);
    vr_cursor_on(&w->r);
}

void vtwin_set_inset(vtwin *w, WORD top)
{
    w->inset_top = top;
    if (!w->t || !w->win)
        return;
    vr_set_inset(&w->r, top);
    vtwin_resize(w);
}

void vtwin_apply_settings(vtwin *w)
{
    if (!w->t || !w->win)
        return;
    vr_cursor_off(&w->r);
    outline_sync(w); /* a profile's font-fallback, live */
    settings(w);
    vr_redraw(&w->r);
    vr_cursor_on(&w->r);
    /* a blink just turned on runs on the frame clock, which only output
     * started: Settings > Cursor > Blinking did nothing in an idle window
     * (owner 2026-10-03: "the cursor doesnt blink when i select that") */
    if (w->r.has_blink || vr_cursor_blinks(&w->r))
        frame_start(w);
}

void vtwin_detach(vtwin *w)
{
    int k;
    /* the frame clock draws (cursor blink) through the renderer freed
     * below: stop it; an AUTO window closes mid-life and opens again
     * (the reopened window hung the handler on the rig, 2026-09-30) */
    frame_stop(w);
    w->render_pending = 0;
    w->layout_dirty = 0;
    w->want_cols = 0;
    w->dragging = 0;
    vti_mouse_reset(&w->mouse);
    w->pointer_on = 0; /* the window's ReportMouse goes with it */
    if (w->outline) {
        vr_set_outline(&w->r, 0);
        vo_close(w->outline);
        w->outline = 0;
    }
    if (w->t) {
        vr_free(&w->r);
        vt_free(w->t);
        w->t = 0;
    }
    w->win = 0;
    if (w->font && w->font_opened)
        CloseFont(w->font);
    w->font = 0;
    w->font_opened = 0;
    for (k = 1; k <= 10; k++)
        if (w->alt[k]) {
            CloseFont(w->alt[k]);
            w->alt[k] = 0;
        }
}

/* ---- output --------------------------------------------------------------------- */

void vtwin_write(vtwin *w, const vt_u8 *b, long n)
{
    if (!w->t)
        return;
    if (w->r.view) {
        /* new output shows the live screen, as xterm does; the engine's
         * pending batch goes first (ignored by the renderer while the view
         * is back), then the view redraws from the grid */
        vtwin_render(w);
        vr_set_view(&w->r, 0);
    }
    /* Frame-paced: the grid changes now, the screen at the next frame
     * (vtwin_render()), so a flood of one-line writes costs one scroll blit
     * per frame instead of one per line (the chip bus of a 4-plane hires
     * screen could not keep up: 45 ms per scroll, cycle-exact rig). */
    {
        vt_u32 sync = vt_modes(w->t) & VT_MODE_SYNC;
        vt_feed(w->t, b, n);
        w->render_pending = 1;
        if (sync && !(vt_modes(w->t) & VT_MODE_SYNC))
            vtwin_render(w); /* the program's frame is whole: shown now, not at the next tick */
    }
    pointer_sync(w); /* ?1003 set or reset: the window reports moves, or stops */
    if (w->layout_dirty) {
        w->layout_dirty = 0;
        vtwin_render(w);
        if (w->want_cols && w->win) {
            /* DECCOLM: the window as wide as that many columns (as far
             * as the screen allows); its new size resizes the grid */
            struct Window *win = w->win;
            WORD width = (WORD)(w->want_cols * w->font->tf_XSize + win->BorderLeft + win->BorderRight);
            if (width > win->WScreen->Width - win->LeftEdge)
                width = (WORD)(win->WScreen->Width - win->LeftEdge);
            w->want_cols = 0;
            ChangeWindowBox(win, win->LeftEdge, win->TopEdge, width, win->Height);
        } else {
            vtwin_resize(w); /* not inside vt_feed: the engine is mid-parse there */
        }
    }
    frame_start(w);
}

int vtwin_raw_report(vtwin *w, int cls)
{
    char b[48];
    int n = 0, k;
    static const char tail[] = ";0;0;0;0;0;0;0|";
    if (!w->t || !(vt_raw_events(w->t) & (1UL << cls)))
        return 0;
    b[n++] = (char)0x9B;
    if (cls >= 10)
        b[n++] = (char)('0' + cls / 10);
    b[n++] = (char)('0' + cls % 10);
    for (k = 0; tail[k]; k++)
        b[n++] = tail[k];
    w->host->input(w->user, (const vt_u8 *)b, n);
    return 1;
}

void vtwin_resize(vtwin *w)
{
    if (!w->t)
        return;
    if (vr_layout(&w->r)) {
        vt_resize(w->t, w->r.cols, w->r.rows);
        if (w->nodraw_resize)
            vt_write(w->t, (const vt_u8 *)"\x0c", 1); /* the ROM clears the unit, cursor home (DP4) */
        w->host->resized(w->user);
    }
    if (!w->nodraw_resize) {
        vr_redraw(&w->r);
        vr_cursor_on(&w->r);
    }
    vtwin_raw_report(w, 12); /* IECLASS_SIZEWINDOW */
}

void vtwin_refresh(vtwin *w)
{
    if (!w->win)
        return;
    BeginRefresh(w->win);
    vr_redraw(&w->r);
    EndRefresh(w->win, TRUE);
    vr_cursor_on(&w->r);
}

/* ---- keys -------------------------------------------------------------------------- */

static long special_key(UWORD code)
{
    switch (code) {
    case 0x4C: return VT_KEY_UP;
    case 0x4D: return VT_KEY_DOWN;
    case 0x4E: return VT_KEY_RIGHT;
    case 0x4F: return VT_KEY_LEFT;
    case 0x41: return VT_KEY_BACKSPACE;
    case 0x42: return VT_KEY_TAB;
    case 0x43: return VT_KEY_KP_ENTER;
    case 0x44: return VT_KEY_RETURN;
    case 0x45: return VT_KEY_ESCAPE;
    case 0x46: return VT_KEY_DELETE;
    case 0x47: return VT_KEY_INSERT;
    case 0x48: return VT_KEY_PAGE_UP;
    case 0x49: return VT_KEY_PAGE_DOWN;
    case 0x4B: return VT_KEY_F11;
    case 0x5F: return VT_KEY_HELP;
    case 0x6F: return VT_KEY_F12;
    case 0x70: return VT_KEY_HOME;
    case 0x71: return VT_KEY_END;
    default: break;
    }
    if (code >= 0x50 && code <= 0x59)
        return VT_KEY_F1 + (code - 0x50);
    return 0;
}

/* Amiga numeric keypad raw codes (the A1200/A500 layout). */
static long keypad_key(UWORD code)
{
    switch (code) {
    case 0x0F: return VT_KEY_KP_0;
    case 0x1D: return VT_KEY_KP_1;
    case 0x1E: return VT_KEY_KP_2;
    case 0x1F: return VT_KEY_KP_3;
    case 0x2D: return VT_KEY_KP_4;
    case 0x2E: return VT_KEY_KP_5;
    case 0x2F: return VT_KEY_KP_6;
    case 0x3D: return VT_KEY_KP_7;
    case 0x3E: return VT_KEY_KP_8;
    case 0x3F: return VT_KEY_KP_9;
    case 0x3C: return VT_KEY_KP_DOT;
    case 0x4A: return VT_KEY_KP_MINUS;
    case 0x5E: return VT_KEY_KP_PLUS;
    case 0x5D: return VT_KEY_KP_STAR;
    case 0x5C: return VT_KEY_KP_SLASH;
    case 0x5A: return VT_KEY_KP_LPAREN;
    case 0x5B: return VT_KEY_KP_RPAREN;
    default: return 0;
    }
}

/* The selection's text to the clipboard, as Latin-1 (the clipboard's). */
static void copy_selection(vtwin *w)
{
    /* allocated, not static: every window's process runs this code */
    char *utf, *lat;
    int ax, ay, bx, by;
    long n, i, k = 0;
    if (!vr_selection(&w->r, &ax, &ay, &bx, &by))
        return;
    utf = (char *)AllocVec(16384, MEMF_ANY);
    if (!utf)
        return;
    lat = utf; /* converted in place: Latin-1 is never longer */
    n = vt_copy_text(w->t, ax, ay, bx, by, utf, 16384);
    for (i = 0; i < n; i++) {
        unsigned char b = (unsigned char)utf[i];
        if (b < 0x80) {
            lat[k++] = (char)b;
        } else if ((b & 0xE0) == 0xC0 && i + 1 < n) {
            unsigned cp = ((b & 0x1F) << 6) | (utf[i + 1] & 0x3F);
            lat[k++] = (char)(cp < 0x100 ? cp : '?');
            i++;
        } else if ((b & 0xC0) != 0x80) {
            lat[k++] = '?'; /* beyond Latin-1 */
        }
    }
    clip_write(lat, k);
    FreeVec(utf);
}

/* The clipboard, typed into the program: Return for each line break, and
 * bracketed when the program asked for it (?2004). */
static void paste(vtwin *w)
{
    char *text = (char *)AllocVec(8192, MEMF_ANY); /* not static: see copy_selection */
    long n, i;
    vt_u8 out[40];
    int k, raw = w->host->raw(w->user);
    if (!text)
        return;
    n = clip_read(text, 8192);
    if (n <= 0) {
        FreeVec(text);
        return;
    }
    if (raw) {
        k = vt_encode_paste(w->t, 0, out);
        w->host->input(w->user, out, k);
    }
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)text[i];
        long key = ch == '\n' ? VT_KEY_RETURN : (long)ch;
        if (ch == '\r')
            continue;
        k = vt_encode_key(w->t, key, 0, out);
        w->host->pasted(w->user, out, k, key == VT_KEY_RETURN ? key : 0);
    }
    if (raw) {
        k = vt_encode_paste(w->t, 1, out);
        w->host->input(w->user, out, k);
    }
    FreeVec(text);
}

/* the window's menu (the owner's): the same copy and paste as the keys */
void vtwin_copy(vtwin *w)
{
    if (w->t && !w->no_clipboard)
        copy_selection(w);
}

void vtwin_paste(vtwin *w)
{
    if (w->t && !w->no_clipboard)
        paste(w);
}

/* The modifiers a key or a mouse event carries (vti_mods: Meta is Left
 * Amiga -- the rig's ';' is Alt + 0x29, Alt-as-Meta turned it into ESC +
 * o-umlaut -- or Alt with meta_alt). */
static int qual_mods(const vtwin *w, UWORD qual)
{
    return vti_mods(qual, w->meta_alt);
}

/* Right Amiga C/V copy and paste, Right Amiga Up/Down and Shift+PgUp/PgDn
 * move through the scrollback. Returns 1 when the key was the console's. */
static int console_key(vtwin *w, UWORD code, UWORD qual)
{
    int page = w->r.rows > 1 ? w->r.rows - 1 : 1;
    if (qual & IEQUALIFIER_RCOMMAND) {
        switch (code) {
        case 0x33: if (w->no_clipboard) return 0; copy_selection(w); return 1;
        case 0x34: if (w->no_clipboard) return 0; paste(w); return 1;
        case 0x4C: vr_set_view(&w->r, w->r.view + 1); return 1;
        case 0x4D: vr_set_view(&w->r, w->r.view - 1); return 1;
        default: return 0;
        }
    }
    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) {
        if (code == 0x48) {
            vr_set_view(&w->r, w->r.view + page);
            return 1;
        }
        if (code == 0x49) {
            vr_set_view(&w->r, w->r.view - page);
            if (!w->r.view)
                vr_cursor_on(&w->r);
            return 1;
        }
    }
    return 0;
}

void vtwin_key(vtwin *w, UWORD code, UWORD qual, ULONG prev, ULONG secs, ULONG micros)
{
    long key;
    int mods = 0, n = 0;
    vt_u8 out[40];
    if (!w->t)
        return;
    if ((qual & IEQUALIFIER_REPEAT) && !(vt_modes(w->t) & VT_MODE_AUTOREPEAT))
        return; /* DECARM off: a held key types once */
    if (vt_raw_events(w->t) & (1UL << 1)) {
        /* the program asked for raw keyboard events (CSI 1 {): an input
         * event report per key, press and release (matrix 5.2) */
        char b[80];
        int k = 0;
        long v[8];
        int i;
        v[0] = 1;
        v[1] = 0;
        v[2] = code;
        v[3] = qual;
        /* the previous two down keys (dead keys): one ULONG, prev1 code and
         * qualifier in the high word, prev2 in the low */
        v[4] = (long)(prev >> 16);
        v[5] = (long)(prev & 0xFFFF);
        v[6] = (long)secs;
        v[7] = (long)micros;
        b[k++] = (char)0x9B;
        for (i = 0; i < 8; i++) {
            char d[12];
            int m = 0;
            unsigned long x = (unsigned long)v[i];
            if (i)
                b[k++] = ';';
            do {
                d[m++] = (char)('0' + x % 10);
                x /= 10;
            } while (x);
            while (m)
                b[k++] = d[--m];
        }
        b[k++] = '|';
        w->host->input(w->user, (const vt_u8 *)b, k);
        return;
    }
    if (code & IECODE_UP_PREFIX)
        return;
    if (console_key(w, code, qual))
        return; /* copy, paste, scrollback: the console's own keys */
    if (w->r.view)
        vr_set_view(&w->r, 0);
    mods = qual_mods(w, qual);
    key = special_key(code);
    if (!key && w->pers == VT_XTERM && (vt_modes(w->t) & VT_MODE_APP_KEYPAD))
        key = keypad_key(code); /* DECKPAM: the keypad sends SS3 codes */
    if (key) {
        n = vt_encode_key(w->t, key, mods, out);
    } else {
        /* Character keys go through the keymap (dead keys, Alt chars). */
        struct InputEvent ie;
        UBYTE buf[16];
        LONG k, i;
        /* modifyOtherKeys (xterm, CSI > 4 ; n m): the keymap gives the
         * character without Ctrl and the encoder gets Ctrl and Shift, so
         * Ctrl+; or Ctrl+Shift+X can be told apart; otherwise the keymap
         * applies Ctrl itself (its control characters, as always) */
        int mok_ctrl = w->pers == VT_XTERM && (mods & VT_MOD_CTRL) && vt_modify_other_keys(w->t);
        ie.ie_NextEvent = 0;
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_SubClass = 0;
        ie.ie_Code = code;
        /* the keymap sees Alt (national characters); Left Amiga, our
         * Meta, it does not -- with meta_alt the other way round */
        ie.ie_Qualifier = (UWORD)(w->meta_alt ?
                                  (qual & ~(IEQUALIFIER_LALT | IEQUALIFIER_RALT)) :
                                  (qual & ~IEQUALIFIER_LCOMMAND));
        if (mok_ctrl)
            ie.ie_Qualifier &= (UWORD)~IEQUALIFIER_CONTROL;
        ie.ie_EventAddress = (APTR)prev;
        k = RawKeyConvert(&ie, (STRPTR)buf, sizeof(buf), w->keymap);
        for (i = 0; i < k && n < (int)sizeof(out) - 16; i++) {
            /* Meta (vt_encode_key adds the ESC for xterm), and Ctrl and
             * Shift when the keymap left Ctrl to the encoder */
            n += vt_encode_key(w->t, buf[i], (w->pers != VT_XTERM) ? 0 : mok_ctrl ? mods : (mods & VT_MOD_ALT),
                               out + n);
        }
    }
    if (n)
        w->host->key(w->user, out, n, key,
                     mods | ((qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) ? VTWIN_MOD_ALTKEY : 0));
}

/* ---- the mouse ---------------------------------------------------------------------- */

/* The text area's geometry, for vtinput's pixel-to-cell maths. */
static void geom(const vtwin *w, vti_geom *g)
{
    g->ox = w->r.ox;
    g->oy = w->r.oy;
    g->cw = w->r.cw;
    g->ch = w->r.ch;
    g->cols = w->r.cols;
    g->rows = w->r.rows;
}

/* The cell under window pixel mx, my: 0 outside the text area. */
static int cell_at(const vtwin *w, WORD mx, WORD my, int *x, int *y)
{
    vti_geom g;
    geom(w, &g);
    return vti_cell_at(&g, mx, my, x, y);
}

/* The selection to the cell under the pointer, while a drag is on. */
static void drag_to(vtwin *w, WORD mx, WORD my)
{
    int x, y;
    if (!w->dragging || !w->t || !cell_at(w, mx, my, &x, &y))
        return;
    if (x == w->drag_x && y == w->drag_y)
        return;
    w->drag_x = x;
    w->drag_y = y;
    vr_select(&w->r, 1, w->drag_ax, w->drag_ay, x, y - w->r.view);
    w->drag_moved = 1;
}

/* Pointer moves as IDCMP_MOUSEMOVE while a drag selects or the program
 * wants them (?1003, ?1002 with a button down): ReportMouse follows. Never
 * on a window that is not ours. */
static void pointer_sync(vtwin *w)
{
    int want;
    if (!w->t || !w->win || w->foreign_window)
        return;
    want = w->dragging || vti_wants_motion(&w->mouse, w->t);
    if (want != w->pointer_on) {
        ReportMouse((BOOL)want, w->win);
        w->pointer_on = want;
    }
}

/* The pointer at window pixel mx, my: the program's motion report, when
 * it wants one for that cell. */
static void motion_to(vtwin *w, WORD mx, WORD my)
{
    int x, y, n;
    vt_u8 out[40];
    if (!cell_at(w, mx, my, &x, &y))
        return;
    n = vti_motion(&w->mouse, w->t, x, y, w->mouse_mods, out);
    if (n)
        w->host->input(w->user, out, n);
}

/* vti_button decides: reports to a program that asked for them (Shift held
 * gives the mouse back to selection, as in xterm), else drag-to-select. */
void vtwin_mouse(vtwin *w, int move, UWORD code, UWORD qual, WORD mx, WORD my)
{
    int x = 0, y = 0, btn, down, n, in;
    vt_u8 out[40];
    if (!w->t)
        return;
    in = cell_at(w, mx, my, &x, &y);
    w->mouse_mods = qual_mods(w, qual); /* Ctrl +16, Meta +8 in the reports */
    if (move) {
        if (w->dragging)
            drag_to(w, mx, my);
        else
            motion_to(w, mx, my);
        return;
    }
    if (!vti_button_code(code, &btn, &down))
        return;
    switch (vti_button(&w->mouse, w->t, btn, down, x, y, in, w->mouse_mods,
                       w->dragging, 0, out, &n)) {
    case VTI_REPORT:
        w->host->input(w->user, out, n);
        if (w->mouse.held)
            frame_start(w); /* vtwin_tick follows the pointer for ?1002 */
        break;
    case VTI_SELECT:
        w->dragging = 1;
        w->drag_moved = 0;
        w->drag_x = x;
        w->drag_y = y;
        w->drag_ax = x;
        w->drag_ay = y - w->r.view;
        vr_select(&w->r, 0, 0, 0, 0, 0);
        frame_start(w); /* vtwin_tick follows the pointer */
        break;
    case VTI_SELECT_END:
        w->dragging = 0;
        if (!w->drag_moved)
            vr_select(&w->r, 0, 0, 0, 0, 0); /* a click clears the selection */
        else if (w->copy_on_select && !w->no_clipboard)
            copy_selection(w); /* the profile's copy-on-select */
        break;
    default:
        break;
    }
    pointer_sync(w);
}

/* The mouse wheel (vti_wheel decides): the program's report when it asked
 * for the mouse, otherwise the scrollback (the spec's wheel_scroll). */
void vtwin_wheel(vtwin *w, int up, UWORD qual, WORD mx, WORD my)
{
    int n, lines;
    vt_u8 out[40];
    vti_geom g;
    if (!w->t)
        return;
    geom(w, &g);
    n = vti_wheel(&g, w->t, w->r.view, up, qual_mods(w, qual), mx, my, out, &lines);
    if (n) {
        w->host->input(w->user, out, n);
        return;
    }
    if (w->wheel_scroll)
        vr_set_view(&w->r, w->r.view + lines);
}

void vtwin_focus(vtwin *w, int in)
{
    vt_u8 out[8];
    int n;
    if (!w->t)
        return;
    n = vt_encode_focus(w->t, in, out);
    if (n)
        w->host->input(w->user, out, n);
}

/* Find: scroll the view to the line a match is on. A match in the live grid
 * brings the view back to the live output (the line is already showing), a
 * match in the scrollback puts the line on the last row of the window so
 * what follows it is readable. Repeating with the same query carries on from
 * the line after the last hit and wraps at the newest line. */
void vtwin_select_all(vtwin *w)
{
    if (!w->t || !w->win)
        return;
    /* grid coordinates now: the scrollback is above row 0 */
    vr_select(&w->r, 1, 0, -vt_scrollback_lines(w->t), vt_cols(w->t) - 1, vt_rows(w->t) - 1);
}

void vtwin_clear_scrollback(vtwin *w)
{
    if (!w->t)
        return;
    if (w->r.view)
        vr_set_view(&w->r, 0); /* the view was in what goes */
    vr_select(&w->r, 0, 0, 0, 0, 0);
    vt_clear_scrollback(w->t);
}

void vtwin_reset(vtwin *w)
{
    if (!w->t || !w->win)
        return;
    vr_select(&w->r, 0, 0, 0, 0, 0);
    vt_reset(w->t);
    vtwin_apply_settings(w); /* the window's own settings again, drawn whole */
}

/* the next designed (not scaled) size of the window's font up (dir 1) or
 * down (-1), within 6..64 pixels: 0 when there is none. The sizes are the
 * face's: a pair's square font is its tall face's (TopazPro 16 is topaz
 * 8: down from it is topaz 11), and on square pixels the pair's square
 * font is one of the sizes (up from topaz 11 is TopazPro 16) -- stepping
 * TopazPro alone found nothing either way (rig 2026-10-03, menus_rig). */
int vtwin_font_step(vtwin *w, int dir)
{
    fontpair_choice tall, sq;
    char family[40], best[40];
    WORD cur, s, best_size = 0;
    if (!w->t || !w->win || !w->font || w->given_font)
        return 0;
    cur = w->font->tf_YSize;
    fontpair_choose((const char *)w->font->tf_Message.mn_Node.ln_Name, cur, 0, 1, &tall);
    strncpy(family, tall.name, sizeof(family) - 1);
    family[sizeof(family) - 1] = 0;
    if (w->aspect_known && !w->aspect_off) {
        fontpair_face(family, w->square, w->square_fits, &sq);
        if (sq.changed && (dir > 0 ? sq.size > cur : sq.size < cur)) {
            strncpy(best, sq.name, sizeof(best) - 1);
            best[sizeof(best) - 1] = 0;
            best_size = (WORD)sq.size;
        }
    }
    for (s = (WORD)(cur + dir); s >= 6 && s <= 64; s = (WORD)(s + dir)) {
        struct TextFont *f;
        if (best_size && (dir > 0 ? s >= best_size : s <= best_size))
            break; /* the pair's font is nearer */
        if ((f = open_font(family, s, 1)) != 0) {
            CloseFont(f);
            strcpy(best, family);
            best_size = s;
            break;
        }
    }
    if (!best_size)
        return 0;
    return vtwin_set_font(w, best, best_size);
}

int vtwin_set_size(vtwin *w, int cols, int rows)
{
    struct Window *win = w->win;
    WORD ww, wh;
    if (!w->t || !win || cols < 1 || rows < 1)
        return 0;
    ww = (WORD)(win->BorderLeft + win->BorderRight + cols * w->r.cw);
    wh = (WORD)(win->BorderTop + win->BorderBottom + w->inset_top + rows * w->r.ch);
    if (ww > win->WScreen->Width || wh > win->WScreen->Height)
        return 0; /* the screen is too small for it */
    ChangeWindowBox(win, (WORD)(win->LeftEdge + ww > win->WScreen->Width ? win->WScreen->Width - ww : win->LeftEdge),
                    (WORD)(win->TopEdge + wh > win->WScreen->Height ? win->WScreen->Height - wh : win->TopEdge),
                    ww, wh);
    return 1; /* the grid follows on IDCMP_NEWSIZE */
}

int vtwin_find(vtwin *w, const char *q)
{
    long row, from;
    if (!w->t)
        return 0;
    if (q && q[0]) {
        strncpy(w->find_q, q, sizeof(w->find_q) - 1);
        w->find_q[sizeof(w->find_q) - 1] = 0;
        w->find_next = VT_ROW_NONE; /* a new query starts at the oldest line */
    }
    if (!w->find_q[0])
        return 0;
    if (w->find_next == VT_ROW_NONE)
        from = -vt_scrollback_lines(w->t);
    else
        from = w->find_next;
    row = vt_find(w->t, w->find_q, from);
    if (row == VT_ROW_NONE) {
        /* wrap to the oldest line and try once more, so a repeated find
         * cycles instead of stopping at the end */
        if (w->find_next == VT_ROW_NONE)
            return 0;
        row = vt_find(w->t, w->find_q, -vt_scrollback_lines(w->t));
        if (row == VT_ROW_NONE)
            return 0;
    }
    if (row >= vt_rows(w->t)) {
        /* the scan reports the row a match starts on, which for a query that
         * crossed a wrap can be the row above: land on the last line of the
         * window so the whole hit is readable */
        row = vt_rows(w->t) - 1;
    }
    w->find_next = row + 1;
    if (row >= 0)
        vr_set_view(&w->r, 0); /* the live grid: it is on screen already */
    else
        vr_set_view(&w->r, -row + w->r.rows - 1);
    return 1;
}
