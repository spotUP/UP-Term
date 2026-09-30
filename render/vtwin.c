/* vtwin: one terminal bound to one window; see vtwin.h. Moved out of the
 * XCON: handler (plan 2026-09-30-console-device.md, DX1/DX2): the same code,
 * with the owner's policy behind vtwin_host. */
#include "vtwin.h"
#include <string.h>
#include <exec/memory.h>
#include <devices/inputevent.h>
#include <devices/console.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/diskfont.h>
#include <proto/console.h>
#include "../handler/clip.h"

extern struct GfxBase *GfxBase;
extern struct Library *DiskfontBase;

#define FRAME_MICROS 50000 /* 20 frames per second */

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

static void cb_bell(void *u)
{
    vtwin *w = (vtwin *)u;
    DisplayBeep(w->win ? w->win->WScreen : 0);
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
    if (w->win)
        SetWindowTitles(w->win, (UBYTE *)w->title, (UBYTE *)~0);
}

/* The engine's idea of the default colours: what the pens show. */
static void report_defaults(vtwin *w)
{
    ULONG fg = vr_pen_rgb(&w->r, w->r.pen_default_fg), bg = vr_pen_rgb(&w->r, w->r.pen_default_bg);
    vt_set_default_colors(w->t, fg, bg, fg);
}

/* A program changed the palette or the default colours (OSC 4, 10-12):
 * new pens, then the whole window redrawn (the engine marked it). */
static void cb_colors(void *u)
{
    vtwin *w = (vtwin *)u;
    ULONG fg = vt_default_color(w->t, 0), bg = vt_default_color(w->t, 1);
    vr_palette_changed(&w->r);
    vr_set_defaults(&w->r, fg, bg);
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

void vtwin_init(vtwin *w, const vtwin_host *host, void *user)
{
    w->host = host;
    w->user = user;
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
    w->render_pending = 0;
    vr_cursor_off(&w->r);
    vt_flush(w->t);
    vr_cursor_on(&w->r);
    if (w->r.has_blink || vr_cursor_blinks(&w->r))
        frame_start(w); /* blinking cells or cursor: the frames keep coming */
}

void vtwin_tick(vtwin *w)
{
    if (w->frame_busy && CheckIO((struct IORequest *)w->frame)) {
        WaitIO((struct IORequest *)w->frame);
        w->frame_busy = 0;
        vtwin_render(w); /* the frame is due */
        if (w->t && vr_blink_tick(&w->r))
            frame_start(w);
    }
    if (!w->frame_open)
        vtwin_render(w); /* no frame clock: draw at once */
}

/* A fixed-width font by name ("topaz" or "topaz.font") and size; 0 if it
 * cannot be opened or is proportional. */
static struct TextFont *open_named(const char *fontname, WORD fontsize)
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
        ta.ta_Flags = 0;
        f = OpenFont(&ta);
        if ((!f || f->tf_YSize != ta.ta_YSize) && DiskfontBase) {
            if (f)
                CloseFont(f);
            f = OpenDiskFont(&ta);
        }
        if (f && (f->tf_Flags & FPF_PROPORTIONAL)) {
            CloseFont(f);
            f = 0;
        }
    }
    return f;
}

struct TextFont *vtwin_open_font(vtwin *w)
{
    struct TextFont *f = open_named(w->fontname, w->fontsize);
    w->font_opened = f != 0;
    /* else the system default font: the one the user chose for text, as
     * the Shell uses it. It is fixed width by definition. */
    w->font = f ? f : GfxBase->DefaultFont;
    return w->font;
}

int vtwin_attach(vtwin *w, struct Window *win)
{
    struct vt_callbacks cb;
    int k;
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
        w->t = vt_new(cols > 0 ? cols : 1, rows > 0 ? rows : 1, 500, &cb, w);
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
    vr_init(&w->r, win, w->font, w->t, w->pers == VT_PCANSI ? VT_ENC_CP437 : VT_ENC_LATIN1);
    for (k = 1; k <= 10; k++) {
        if (!w->alt[k] && w->altname[k][0])
            w->alt[k] = open_named(w->altname[k], w->altsize[k] ? w->altsize[k] : w->font->tf_YSize);
        vr_set_alt_font(&w->r, k, w->alt[k]);
    }
    vr_set_defaults(&w->r, w->fg_rgb, w->bg_rgb);
    report_defaults(w);
    vt_set_cell_pixels(w->t, w->font->tf_XSize, w->font->tf_YSize);
    vr_redraw(&w->r);
    vr_cursor_on(&w->r);
    return 1;
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
    vt_feed(w->t, b, n);
    w->render_pending = 1;
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
        w->host->resized(w->user);
    }
    vr_redraw(&w->r);
    vr_cursor_on(&w->r);
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

/* Right Amiga C/V copy and paste, Right Amiga Up/Down and Shift+PgUp/PgDn
 * move through the scrollback. Returns 1 when the key was the console's. */
static int console_key(vtwin *w, UWORD code, UWORD qual)
{
    int page = w->r.rows > 1 ? w->r.rows - 1 : 1;
    if (qual & IEQUALIFIER_RCOMMAND) {
        switch (code) {
        case 0x33: copy_selection(w); return 1;
        case 0x34: paste(w); return 1;
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
    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        mods |= VT_MOD_SHIFT;
    if (qual & IEQUALIFIER_CONTROL)
        mods |= VT_MOD_CTRL;
    /* Meta (the ESC prefix, VT_MOD_ALT) is Left Amiga + key: Alt belongs
     * to the keymap, where many layouts type ; @ { [ with it (the rig's
     * ';' is Alt + 0x29; Alt-as-Meta turned it into ESC + o-umlaut). */
    if (qual & IEQUALIFIER_LCOMMAND)
        mods |= VT_MOD_ALT;
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
        ie.ie_NextEvent = 0;
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_SubClass = 0;
        ie.ie_Code = code;
        /* the keymap sees Alt (national characters); Left Amiga, our
         * Meta, it does not */
        ie.ie_Qualifier = (UWORD)(qual & ~IEQUALIFIER_LCOMMAND);
        ie.ie_EventAddress = (APTR)prev;
        k = RawKeyConvert(&ie, (STRPTR)buf, sizeof(buf), 0);
        for (i = 0; i < k && n < (int)sizeof(out) - 8; i++) {
            /* The keymap already applied Ctrl: pass the character, with
             * Meta only (vt_encode_key adds the ESC for xterm). */
            n += vt_encode_key(w->t, buf[i], (w->pers == VT_XTERM) ? (mods & VT_MOD_ALT) : 0, out + n);
        }
    }
    if (n)
        w->host->key(w->user, out, n, key, mods);
}

/* ---- the mouse ---------------------------------------------------------------------- */

/* Reports to a program that asked for them (Shift held gives the mouse back
 * to selection, as in xterm), else drag-to-select. */
void vtwin_mouse(vtwin *w, int move, UWORD code, UWORD qual, WORD mx, WORD my)
{
    int x, y, btn = -1, kind = 0, n, in;
    vt_u8 out[40];
    int shift = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
    if (!w->t)
        return;
    in = vr_cell_at(&w->r, mx, my, &x, &y);
    if (move) {
        if (w->dragging && in) {
            vr_select(&w->r, 1, w->drag_ax, w->drag_ay, x, y - w->r.view);
            w->drag_moved = 1;
        }
        return;
    }
    if (code == SELECTDOWN) { btn = 0; kind = 0; }
    else if (code == SELECTUP) { btn = 0; kind = 1; }
    else if (code == MENUDOWN) { btn = 2; kind = 0; }
    else if (code == MENUUP) { btn = 2; kind = 1; }
    if (btn < 0)
        return;
    if (!shift && in && !w->dragging) {
        n = vt_encode_mouse(w->t, btn, kind, x, y, 0, out);
        if (n) {
            w->host->input(w->user, out, n);
            return;
        }
    }
    if (btn != 0)
        return;
    if (kind == 0 && in) {
        w->dragging = 1;
        w->drag_moved = 0;
        w->drag_ax = x;
        w->drag_ay = y - w->r.view;
        vr_select(&w->r, 0, 0, 0, 0, 0);
        ReportMouse(TRUE, w->win);
    } else if (kind == 1 && w->dragging) {
        w->dragging = 0;
        ReportMouse(FALSE, w->win);
        if (!w->drag_moved)
            vr_select(&w->r, 0, 0, 0, 0, 0); /* a click clears the selection */
    }
}
