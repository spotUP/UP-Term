/* UP-Term Prefs: the settings editor for up-term/up-term in ENV: and
 * ENVARC: -- the profiles the XCON: handler reads for every window (plans
 * thoughts/shared/plans/2026-10-01-terminal-preferences.md and
 * 2026-10-14-prefs-ux.md).
 *
 * C:UP-Term Prefs opens a window with five pages, General, Colors, Palette,
 * Keyboard and Advanced, picked with the Page gadget. The fields edit one
 * profile at a time (the Profile gadget lists the file's profiles; New...,
 * Rename... and Delete work on the list), then the Amiga Prefs buttons: Save
 * writes ENVARC: and ENV: (kept across a reboot), Use writes ENV: only (until
 * the reboot), Cancel writes nothing. Save and Use end the editor once the
 * file is written; Cancel ends it at once; the close gadget asks "Discard
 * changes?" when there are unsaved edits. The previous file is kept as
 * up-term.orig. New XCON: windows use the file, and so do the windows already
 * open: the handler's watcher (handler/vtcon_handler.c watch_worker) reads
 * the file when it changes and each window whose own profile changed puts it
 * on live (profile_switch: colours, palette, font, scrollback, scroll bar,
 * cursor, bell, keys, completion). Only what is fixed when a window opens
 * needs a new window: the screen, its mode and depth, and the TERM / colors
 * a program already started was told.
 *
 * Each value has the control that fits it: a font, a fallback font and a
 * screen mode are picked in asl.library's requesters (prefs/prefs_asl.c, the
 * same module the handler's Settings menu uses), a colour in a colour window
 * (colour wheel, brightness, red / green / blue, hex), KingCON's completion
 * style in a cycle and two check boxes. Text is typed only where it is text
 * (hex digits, the link command, names).
 *
 * The window is GadTools, built from the gadget table in prefs_ui.c (id,
 * page, kind, label, help, place); the layout is font sensitive (prefs_ui.c
 * pl_layout, host-tested) and falls back to topaz 8 when the screen's font
 * would not fit the screen. What reaches the file is decided in
 * prefs/prefs_core.c (host-tested): a file the editor cannot hold whole is
 * not written back, a save is staged on a copy of the table, and the file
 * is replaced through a temporary file and renames.
 */
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/intuitionbase.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/displayinfo.h>
#include <libraries/gadtools.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/utility.h>

#include "../config/upconf.h"
#include "prefs_core.h"
#include "prefs_ui.h"
#include "prefs_asl.h"
#include "../tty/qmark.h"
#include "prefs_dos.h"

/* vbcc's startup runs main on a stack of this size whatever the starter gives
 * it; the cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6). */
long __stack = 16384;
const char upprefs_stack_cookie[] = "$STACK: 16384";

/* The clib's calls read these; startup.o provides DOSBase only. */
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *UtilityBase;

/* ENV: is what is in use now, ENVARC: what the next boot copies to ENV:. */
#define T_ENV PREFS_T_ENV
#define T_ENVARC PREFS_T_ENVARC
#define conf_dir prefs_conf_dir
#define conf_path prefs_conf_path

/* the same cap the handler reads with, so a file this writes is always a
 * file the handler can read whole */
#define CONF_MAX   UC_MAX_FILE
#define STATUS_MAX 128

/* the room round the gadgets inside the window */
#define MARGIN_X 8
#define MARGIN_Y 4

/* a swatch's line of text is the larger of these, a name the longest */
#define PROFNAME_CAP UC_NAME

struct app {
    struct Window *win;
    struct Screen *scr;
    struct DrawInfo *dri;
    APTR vi;                  /* GadTools' VisualInfo of the screen */
    struct TextAttr *ta;      /* the font of the gadgets: the screen's, or topaz 8 */
    WORD ox, oy;              /* the inner top left, in window coordinates */
    pl_result lay;            /* the boxes of every gadget */
    struct Gadget *glist_common; /* Page, status, Save / Use / Cancel */
    struct Gadget *glist[PG_NPAGES];
    struct Gadget *gad[PL_MAX_IDS]; /* by gadget id */
    LONG pen[PL_MAX_IDS];     /* a swatch's pen, -1 none */
    int page;                 /* the page in the window, -1 none yet */
    upconf conf;              /* the file's table, as loaded and as last written */
    upconf work;              /* a save is staged here, then copied to conf */
    char buf[CONF_MAX + 2];   /* the file text, read or about to be written */
    int writable;             /* the file was loaded whole: it may be written back */
    char prof[UC_NAME];       /* the profile the fields show */
    int prof_new;             /* that profile is not in the table yet */
    int tab_dirty;            /* the table changed (rename, delete) and is not saved */
    STRPTR plab[UC_MAX_PROFILES + 2]; /* the Profile gadget's labels */
    prefs_fields f;           /* what the gadgets show */
    prefs_fields orig;        /* what the profile held when it was loaded or last written */
    prefs_kc kc;              /* KingCON's style, decoded from f.kcmode */
    long sb_lines;            /* the scrollback gadget's number */
    int sb_none;              /* "No scrollback" */
    int colors;               /* the Colour mode cycle, prefs_colors_index of f.colors */
    char status[2][STATUS_MAX]; /* GadTools keeps the pointer: alternate */
    char action[STATUS_MAX];  /* the last action's line, shown when no gadget has help */
    int st;
    int helped;               /* the gadget whose help the status line shows */
    char theme[300];          /* the theme file last put in the fields ("" none): the
                               * requester opens its drawer next time */
};

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT };

static STRPTR page_labels[] = { (STRPTR)"General", (STRPTR)"Colors", (STRPTR)"Palette",
                                (STRPTR)"Keyboard", (STRPTR)"Advanced", 0 };
static STRPTR cursor_labels[] = { (STRPTR)"Block", (STRPTR)"Underline", (STRPTR)"Bar", 0 };
static STRPTR bell_labels[] = { (STRPTR)"None", (STRPTR)"Beep", (STRPTR)"Visual", 0 };
static STRPTR meta_labels[] = { (STRPTR)"Left Amiga", (STRPTR)"Alt", 0 };
static STRPTR complete_labels[] = { (STRPTR)"Unix", (STRPTR)"KingCON", 0 };
/* in the order of PREFS_KC_*, PREFS_SCREEN_*, PREFS_CLIP_* */
static STRPTR kcmatch_labels[] = { (STRPTR)"Window", (STRPTR)"List under the line", (STRPTR)"Tab cycles",
                                   (STRPTR)"List and Tab cycles", (STRPTR)"Common part only", 0 };
static STRPTR screen_labels[] = { (STRPTR)"Workbench", (STRPTR)"Own screen", (STRPTR)"Full screen", 0 };
static STRPTR backspace_labels[] = { (STRPTR)"Delete", (STRPTR)"Backspace", 0 };
static STRPTR clip_labels[] = { (STRPTR)"Set the clipboard", (STRPTR)"Set and read the clipboard",
                                (STRPTR)"Not use the clipboard", 0 };
/* in prefs_colors_index order; the Settings menu's Colour mode words */
static STRPTR colors_labels[] = { (STRPTR)"Not told", (STRPTR)"Full colour (RGB)", (STRPTR)"256 colours",
                                  (STRPTR)"16 colours", 0 };

/* A taglist from (tag, data) pairs, ended by TAG_DONE: t has room for them.
 * (A compound literal: vbcc warns about va_start, and the values are not
 * constants.) */
static struct TagItem *tl_a(struct TagItem *t, const ULONG *v)
{
    int n = 0;
    while (*v != TAG_DONE) {
        t[n].ti_Tag = v[0];
        t[n].ti_Data = v[1];
        n++;
        v += 2;
    }
    t[n].ti_Tag = TAG_DONE;
    t[n].ti_Data = 0;
    return t;
}
#define tl(t, ...) tl_a(t, (const ULONG[]){ __VA_ARGS__ })

/* The window a gadget of this page is in now (0: held off the window). */
static struct Window *win_of(struct app *a, int page)
{
    return page < 0 || page == a->page ? a->win : 0;
}

static int page_of(int id)
{
    const pg_item *it = pg_find(id);
    return it ? it->page : -1;
}

static void set_attr(struct app *a, int id, ULONG tag, ULONG data)
{
    struct TagItem t[2];
    if (id >= 0 && id < PL_MAX_IDS && a->gad[id])
        GT_SetGadgetAttrsA(a->gad[id], win_of(a, page_of(id)), 0, tl(t, tag, data, TAG_DONE));
}

/* ---- the status line ------------------------------------------------------------ */

/* Append to a status line: a string, then (when num >= 0) its decimal.
 * Leaves the line terminated; returns the length. */
static int puts_(char *dst, int at, const char *s, long num)
{
    char digits[12];
    int n = 0;
    while (*s && at < STATUS_MAX - 12)
        dst[at++] = *s++;
    if (num >= 0) {
        do {
            digits[n++] = (char)('0' + num % 10);
            num /= 10;
        } while (num);
        while (n)
            dst[at++] = digits[--n];
    }
    dst[at] = 0;
    return at;
}

static void show_line(struct app *a, const char *s)
{
    char *d;
    a->st ^= 1;
    d = a->status[a->st];
    puts_(d, 0, s, -1);
    set_attr(a, PG_STATUS, GTTX_Text, (ULONG)d);
}

/* What the last action did: stays until the next one. */
static void set_status(struct app *a, const char *s)
{
    puts_(a->action, 0, s, -1);
    a->helped = 0;
    show_line(a, a->action);
}

/* The help of the gadget under the pointer, or the last action's line. */
static void show_help(struct app *a, int id)
{
    const pg_item *it;
    if (id == a->helped)
        return;
    a->helped = id;
    it = id ? pg_find(id) : 0;
    show_line(a, it && it->help ? it->help : a->action);
}

/* ---- the fields <-> the gadgets ------------------------------------------------- */

/* The text field a string gadget edits (hex digits, the link command). */
static char *text_field(struct app *a, int id, int *cap)
{
    *cap = UC_MAX_VALUE;
    switch (id) {
    case PG_CURSOR_COLOUR_HEX: return a->f.curcol;
    case PG_FG_HEX: return a->f.fg;
    case PG_BG_HEX: return a->f.bg;
    case PG_SELFG_HEX: return a->f.selfg;
    case PG_SELBG_HEX: return a->f.selbg;
    case PG_LINK: return a->f.linkopen;
    case PG_TERM:
        *cap = (int)sizeof(a->f.term);
        return a->f.term;
    default:
        break;
    }
    if (id >= PG_PAL_HEX && id < PG_PAL_HEX + 16) {
        *cap = (int)sizeof(a->f.pal[0]);
        return a->f.pal[id - PG_PAL_HEX];
    }
    return 0;
}

/* The hex field of a swatch */
static int swatch_hex(int id)
{
    return id >= PG_PAL && id < PG_PAL + 16 ? id + (PG_PAL_HEX - PG_PAL) : id + 1;
}

static int is_swatch(int id)
{
    const pg_item *it = pg_find(id);
    return it && it->kind == PGK_SWATCH;
}

static int cursor_inverse(struct app *a)
{
    const char *s = a->f.curcol;
    const char *w = "inverse";
    while (*s && *w) {
        char c = *s++;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != *w++)
            return 0;
    }
    return !*s && !*w;
}

/* The int behind a cycle or check box (0: it is not a plain one). */
static int *int_of(struct app *a, int id)
{
    switch (id) {
    case PG_CURSOR: return &a->f.cursor;
    case PG_BLINK: return &a->f.blink;
    case PG_BELL: return &a->f.bell;
    case PG_BOLD: return &a->f.bold;
    case PG_WHEEL: return &a->f.wheel;
    case PG_COPY: return &a->f.copy_sel;
    case PG_SCROLLBAR: return &a->f.scrollbar;
    case PG_REFLOW: return &a->f.reflow;
    case PG_DITHER: return &a->f.dither;
    case PG_OUTLINE_AA: return &a->f.outline_aa;
    case PG_PADDING: return &a->f.pad_extend;
    case PG_META: return &a->f.meta_alt;
    case PG_AMIGAKEYS: return &a->f.amiga_keys;
    case PG_BACKSPACE: return &a->f.backspace_bs;
    case PG_COMPLETE: return &a->f.completion;
    case PG_KC_MATCH: return &a->kc.match;
    case PG_KC_COMMON: return &a->kc.common;
    case PG_KC_SILENT: return &a->kc.silent;
    case PG_KC_INFO: return &a->f.kcinfo;
    case PG_KC_CACHE: return &a->f.kccache;
    case PG_SCREEN: return &a->f.screen;
    case PG_ASPECT: return &a->f.aspect;
    case PG_CLIPBOARD: return &a->f.clipboard;
    case PG_HLCAT: return &a->f.hlcat;
    case PG_SCROLLBACK_NONE: return &a->sb_none;
    case PG_COLORS: return &a->colors;
    default:
        return 0;
    }
}

static STRPTR *labels_of(int id)
{
    switch (id) {
    case PG_PAGE: return page_labels;
    case PG_CURSOR: return cursor_labels;
    case PG_BELL: return bell_labels;
    case PG_META: return meta_labels;
    case PG_COMPLETE: return complete_labels;
    case PG_KC_MATCH: return kcmatch_labels;
    case PG_SCREEN: return screen_labels;
    case PG_BACKSPACE: return backspace_labels;
    case PG_CLIPBOARD: return clip_labels;
    case PG_COLORS: return colors_labels;
    default: return 0;
    }
}

static int count_of(STRPTR *l)
{
    int n = 0;
    while (l && l[n])
        n++;
    return n;
}

/* The profiles the Profile gadget lists: the table's, and the one being
 * edited when it is not in the table yet. The index of the one being edited. */
static int prof_labels(struct app *a)
{
    int i, n = 0, at = -1;
    for (i = 0; i < a->conf.nprof; i++) {
        a->plab[n] = (STRPTR)a->conf.prof[i];
        if (!strcmp(a->conf.prof[i], a->prof))
            at = n;
        n++;
    }
    if (at < 0) {
        a->plab[n] = (STRPTR)a->prof;
        at = n++;
    }
    a->plab[n] = 0;
    return at;
}

/* ---- scrollback, KingCON and the texts ---------------------------------------------- */

static void sb_load(struct app *a)
{
    prefs_sb_parse(a->f.sb, &a->sb_lines, &a->sb_none);
}

static void sb_store(struct app *a)
{
    prefs_sb_format(a->sb_lines, a->sb_none, a->f.sb);
}

static void kc_store(struct app *a)
{
    prefs_kc_encode(&a->kc, a->f.kcmode, (int)sizeof(a->f.kcmode));
}

/* "Name, depth 5" of the screen mode in the fields */
static void mode_text(struct app *a, char *out, int cap)
{
    unsigned long id;
    int depth = prefs_depth_parse(a->f.screendepth), j;
    struct NameInfo ni;
    out[0] = 0;
    if (!prefs_mode_parse(a->f.screenmode, &id) || !id) {
        strcpy(out, "Workbench's");
        return;
    }
    if (GetDisplayInfoData(0, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, (ULONG)id) > 0 && ni.Name[0]) {
        strncpy(out, (const char *)ni.Name, (size_t)(cap - 1));
        out[cap - 1] = 0;
    } else {
        strncpy(out, a->f.screenmode, (size_t)(cap - 1));
        out[cap - 1] = 0;
    }
    j = (int)strlen(out);
    if (depth && j < cap - 12) {
        strcpy(out + j, ", depth ");
        out[j + 8] = (char)('0' + depth);
        out[j + 9] = 0;
    } else if (j < cap - 16)
        strcpy(out + j, ", default depth");
}

static void show_texts(struct app *a)
{
    char t[UC_MAX_VALUE];
    set_attr(a, PG_FONT, GTTX_Text, (ULONG)(a->f.font[0] ? a->f.font : "Screen's font"));
    set_attr(a, PG_FALLBACK, GTTX_Text, (ULONG)(a->f.fallback[0] ? a->f.fallback : "None"));
    mode_text(a, t, sizeof(t));
    set_attr(a, PG_SCREEN_MODE, GTTX_Text, (ULONG)t);
}

/* ---- swatches ------------------------------------------------------------------------ */

static int swatch_rgb(struct app *a, int id, uc_u32 *rgb)
{
    int cap;
    char *s = text_field(a, swatch_hex(id), &cap);
    return s && s[0] && prefs_hex_parse(s, rgb);
}

static void pen_release(struct app *a, int id)
{
    if (a->pen[id] >= 0) {
        ReleasePen(a->scr->ViewPort.ColorMap, (ULONG)a->pen[id]);
        a->pen[id] = -1;
    }
}

static LONG pen_for(struct app *a, uc_u32 rgb)
{
    struct TagItem t[3];
    return ObtainBestPenA(a->scr->ViewPort.ColorMap,
                          ((rgb >> 16) & 0xFF) * 0x01010101UL, ((rgb >> 8) & 0xFF) * 0x01010101UL,
                          (rgb & 0xFF) * 0x01010101UL,
                          tl(t, OBP_Precision, PRECISION_GUI, OBP_FailIfBad, FALSE, TAG_DONE));
}

static void swatch_pen(struct app *a, int id)
{
    uc_u32 rgb;
    pen_release(a, id);
    if (swatch_rgb(a, id, &rgb))
        a->pen[id] = pen_for(a, rgb);
}

/* A swatch, with its label left of it: the colour in the field, else a
 * crossed box (the built-in colour stands). Only where the page is shown. */
static void draw_swatch(struct app *a, int id)
{
    const pg_item *it = pg_find(id);
    struct RastPort *rp;
    const pl_box *b;
    WORD x, y;
    char plain[40];
    int disabled;
    UWORD *pens;
    if (!it || !a->win || (it->page != a->page && it->page != PG_COMMON))
        return;
    rp = a->win->RPort;
    pens = a->dri->dri_Pens;
    b = &a->lay.box[id];
    x = (WORD)(a->ox + b->x);
    y = (WORD)(a->oy + b->y);
    disabled = a->gad[id] && (a->gad[id]->Flags & GFLG_DISABLED);
    if (pg_label(it) && a->lay.label_w[id]) {
        pg_plain_label(pg_label(it), plain, sizeof(plain));
        SetAPen(rp, pens[disabled ? SHADOWPEN : TEXTPEN]);
        SetBPen(rp, pens[BACKGROUNDPEN]);
        SetDrMd(rp, JAM2);
        Move(rp, x - a->lay.label_w[id], (WORD)(y + (b->h - a->lay.font_h) / 2 + rp->TxBaseline));
        Text(rp, (STRPTR)plain, (ULONG)strlen(plain));
    }
    {
        struct TagItem bt[3];
        DrawBevelBoxA(rp, x, y, b->w, b->h, tl(bt, GT_VisualInfo, (ULONG)a->vi, GTBB_Recessed, TRUE, TAG_DONE));
    }
    SetAPen(rp, a->pen[id] >= 0 && !disabled ? (UBYTE)a->pen[id] : pens[BACKGROUNDPEN]);
    RectFill(rp, x + 2, y + 1, x + b->w - 3, y + b->h - 2);
    if (a->pen[id] < 0 || disabled) {
        SetAPen(rp, pens[SHADOWPEN]);
        Move(rp, x + 3, y + b->h - 3);
        Draw(rp, x + b->w - 4, y + 2);
    }
}

static void draw_swatches(struct app *a)
{
    int i;
    for (i = 0; i < pg_nitems; i++)
        if (pg_items[i].kind == PGK_SWATCH)
            draw_swatch(a, pg_items[i].id);
}

/* ---- enabling, and the fields into the gadgets ---------------------------------------- */

static int is_disabled(struct app *a, const pg_item *it)
{
    switch (it->disabled) {
    case PGD_NOT_KINGCON: return a->f.completion != PREFS_COMPLETE_KINGCON;
    case PGD_NOT_OWN_SCREEN: return a->f.screen == PREFS_SCREEN_WORKBENCH;
    case PGD_SCROLLBACK_NONE: return a->sb_none;
    case PGD_CURSOR_INVERSE: return cursor_inverse(a);
    default: return 0;
    }
}

static void apply_enabled(struct app *a)
{
    int i;
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *it = &pg_items[i];
        struct Gadget *g = a->gad[it->id];
        int off;
        if (!it->disabled || !g)
            continue;
        off = is_disabled(a, it);
        if (it->kind == PGK_SWATCH) {
            struct Window *w = win_of(a, it->page);
            if (w) {
                if (off)
                    OffGadget(g, w, 0);
                else
                    OnGadget(g, w, 0);
            } else if (off)
                g->Flags |= GFLG_DISABLED;
            else
                g->Flags &= ~GFLG_DISABLED;
        } else
            set_attr(a, it->id, GA_Disabled, (ULONG)off);
    }
    draw_swatches(a);
}

/* Show the fields in the gadgets. */
static void show_fields(struct app *a)
{
    int i, at;
    struct TagItem t[4];
    at = prof_labels(a);
    if (a->gad[PG_PROFILE])
        GT_SetGadgetAttrsA(a->gad[PG_PROFILE], win_of(a, PG_PAGE_GENERAL), 0,
                           tl(t, GTCY_Labels, (ULONG)a->plab, GTCY_Active, (ULONG)at, TAG_DONE));
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *it = &pg_items[i];
        int cap;
        char *s;
        int *p;
        switch (it->kind) {
        case PGK_STRING:
            s = text_field(a, it->id, &cap);
            if (s)
                set_attr(a, it->id, GTST_String, (ULONG)s);
            break;
        case PGK_INTEGER:
            set_attr(a, it->id, GTIN_Number, (ULONG)a->sb_lines);
            break;
        case PGK_CYCLE:
            p = int_of(a, it->id);
            if (p)
                set_attr(a, it->id, GTCY_Active, (ULONG)*p);
            break;
        case PGK_CHECK:
            p = int_of(a, it->id);
            if (it->id == PG_CURSOR_INVERSE)
                set_attr(a, it->id, GTCB_Checked, (ULONG)cursor_inverse(a));
            else if (it->id == PG_KC_COMMON)
                set_attr(a, it->id, GTCB_Checked, (ULONG)(a->kc.match == PREFS_KC_COMMON_ONLY || a->kc.common));
            else if (p)
                set_attr(a, it->id, GTCB_Checked, (ULONG)*p);
            break;
        case PGK_SWATCH:
            swatch_pen(a, it->id);
            break;
        default:
            break;
        }
    }
    show_texts(a);
    apply_enabled(a);
}

/* Take the text of every string field: a string gadget reports only Return
 * and Tab, so what was typed without either is read here, before any
 * action that uses the fields. GadTools lets a program read the buffer. */
static void collect(struct app *a)
{
    int i, k, cap;
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *it = &pg_items[i];
        const char *s;
        char *val;
        if (!a->gad[it->id])
            continue;
        if (it->kind == PGK_STRING) {
            val = text_field(a, it->id, &cap);
            if (!val)
                continue;
            s = (const char *)((struct StringInfo *)a->gad[it->id]->SpecialInfo)->Buffer;
            for (k = 0; s[k] && k < cap - 1; k++)
                val[k] = s[k];
            val[k] = 0;
        } else if (it->kind == PGK_INTEGER && !a->sb_none) {
            long lines;
            int none;
            s = (const char *)((struct StringInfo *)a->gad[it->id]->SpecialInfo)->Buffer;
            prefs_sb_parse(s, &lines, &none);
            if (lines != a->sb_lines) {
                a->sb_lines = lines;
                sb_store(a);
            }
        }
    }
}

/* the page that has the gadget, and the gadget active (a bad entry) */
static void show_page(struct app *a, int page);
static void goto_gadget(struct app *a, int id)
{
    int pg = page_of(id);
    if (pg >= 0 && pg != a->page) {
        show_page(a, pg);
        set_attr(a, PG_PAGE, GTCY_Active, (ULONG)pg);
    }
    if (a->gad[id])
        ActivateGadget(a->gad[id], a->win, 0);
}

/* ---- the pages ----------------------------------------------------------------------- */

static void show_page(struct app *a, int page)
{
    if (a->page == page)
        return;
    if (a->page >= 0)
        RemoveGList(a->win, a->glist[a->page], -1);
    /* the old page's gadgets and labels go with the area they were in */
    EraseRect(a->win->RPort, a->ox - 4, a->oy + a->lay.pitch,
              a->ox + a->lay.inner_w + 3, a->oy + a->lay.box[PG_STATUS].y - 3);
    a->page = page;
    AddGList(a->win, a->glist[page], (UWORD)~0, -1, 0);
    RefreshGList(a->glist[page], a->win, 0, -1);
    GT_RefreshWindow(a->win, 0);
    apply_enabled(a);
}

/* ---- the file ---------------------------------------------------------------------- */

/* Read one file: up to CONF_MAX + 1 bytes, read straight rather than
 * Seek()ed (the handler found Seek answering 0 for ENV files on 3.1), so a
 * longer file shows as longer. 0 when absent, -1 on a read error. */
static long read_file(struct app *a, int t)
{
    BPTR f = Open((STRPTR)conf_path[t], MODE_OLDFILE);
    long got = 0, n;
    if (!f)
        return IoErr() == ERROR_OBJECT_NOT_FOUND ? 0 : -1;
    while (got < CONF_MAX + 1) {
        n = Read(f, a->buf + got, CONF_MAX + 1 - got);
        if (n < 0) {
            got = -1;
            break;
        }
        if (n == 0)
            break;
        got += n;
    }
    Close(f);
    if (got >= 0)
        a->buf[got] = 0;
    return got;
}

/* The settings in use (ENV:), else the saved ones (ENVARC:). A file this
 * editor cannot hold whole is shown as defaults and never written back. */
static void load_file(struct app *a)
{
    long got = read_file(a, T_ENV);
    int r;
    char msg[STATUS_MAX];
    if (got == 0)
        got = read_file(a, T_ENVARC);
    r = prefs_load(&a->conf, a->buf, got, CONF_MAX);
    a->writable = prefs_load_writable(r);
    switch (r) {
    case PREFS_LOAD_TOOBIG:
        puts_(msg, puts_(msg, 0, "The file is over ", CONF_MAX), " bytes: read only.", -1);
        set_status(a, msg);
        break;
    case PREFS_LOAD_LOSSY:
        set_status(a, "The file holds more than the editor can: read only.");
        break;
    case PREFS_LOAD_ERROR:
        set_status(a, "The file cannot be read: read only.");
        break;
    default:
        break;
    }
}

static void install_error(struct app *a, int t, int r)
{
    char msg[STATUS_MAX];
    int j = puts_(msg, 0, conf_path[t], -1);
    switch (r) {
    case PREFS_INSTALL_WRITE:
        puts_(msg, j, ": write failed, old file kept.", -1);
        break;
    case PREFS_INSTALL_BACKUP:
        puts_(msg, j, ": no backup possible, old file kept.", -1);
        break;
    case PREFS_INSTALL_PLACE:
        puts_(msg, j, ": cannot replace, old file kept.", -1);
        break;
    default:
        puts_(msg, j, ": see up-term.orig and up-term.new.", -1);
        break;
    }
    set_status(a, msg);
}

/* The profile named in the fields: its values from the table (or the built-in
 * ones for a name the table lacks), and what "unchanged" means from here. */
static void load_profile(struct app *a, const char *name)
{
    strcpy(a->prof, name);
    if (prefs_profile_exists(&a->conf, name)) {
        prefs_from_conf(&a->f, &a->conf, name);
        a->prof_new = 0;
    } else {
        prefs_defaults(&a->f);
        a->prof_new = 1;
    }
    a->orig = a->f;
    sb_load(a);
    a->colors = prefs_colors_index(a->f.colors);
    prefs_kc_decode(a->f.kcmode, &a->kc);
    show_fields(a);
}

/* Is there something a Cancel would lose? */
static int dirty(struct app *a)
{
    collect(a);
    return a->tab_dirty || a->prof_new || !prefs_equal(&a->f, &a->orig);
}

/* "Discard changes?" -- 1 to go on. */
static int confirm(struct app *a, const char *body, const char *yes, const char *no, const char *arg)
{
    struct EasyStruct es;
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"UP-Term Prefs";
    es.es_TextFormat = (UBYTE *)body;
    {
        char gads[40];
        strcpy(gads, yes);
        strcat(gads, "|");
        strcat(gads, no);
        ULONG args[1];
        args[0] = (ULONG)arg;
        es.es_GadgetFormat = (UBYTE *)gads;
        return EasyRequestArgs(a->win, &es, 0, (APTR)args) == 1;
    }
}

static int confirm_discard(struct app *a)
{
    return !dirty(a) || confirm(a, "Discard changes?", "Discard", "Keep editing", "");
}

/* The field a prefs_validate result names, as the gadget that holds it */
static int bad_gadget(int bad)
{
    if (bad >= PREFS_BAD_PAL && bad < PREFS_BAD_PAL + 16)
        return PG_PAL_HEX + bad - PREFS_BAD_PAL;
    switch (bad) {
    case PREFS_BAD_SELFG: return PG_SELFG_HEX;
    case PREFS_BAD_SELBG: return PG_SELBG_HEX;
    case PREFS_BAD_FG: return PG_FG_HEX;
    case PREFS_BAD_BG: return PG_BG_HEX;
    case PREFS_BAD_TERM: return PG_TERM;
    default: return PG_CURSOR_COLOUR_HEX;
    }
}

/* Use (keep = 0): ENV: only. Save (keep = 1): ENVARC:, then ENV:. 1 when the
 * file is written (the editor then ends: the windows already open apply it,
 * see the top of this file); 0 with the reason in the status line. */
static int commit(struct app *a, int keep)
{
    char name[UC_NAME];
    char msg[STATUS_MAX];
    long len;
    int bad, t, r;
    if (!a->writable) {
        set_status(a, "Read only: the file was not loaded whole.");
        return 0;
    }
    collect(a);
    prefs_clean_name(a->prof, name, sizeof(name));
    if (!name[0]) {
        set_status(a, "Name the profile first.");
        return 0;
    }
    bad = prefs_validate(&a->f);
    if (bad) {
        char lab[48];
        int j;
        prefs_bad_label(bad, lab, sizeof(lab));
        j = puts_(msg, 0, lab, -1);
        puts_(msg, j, ": six hex digits, RRGGBB, or blank.", -1);
        set_status(a, msg);
        goto_gadget(a, bad_gadget(bad));
        return 0;
    }
    len = prefs_stage(&a->work, &a->conf, name, &a->f, a->buf, CONF_MAX + 1);
    if (len == PREFS_STAGE_LOSSY) {
        set_status(a, "Read only: the file was not loaded whole.");
        return 0;
    }
    if (len == PREFS_STAGE_FULL) {
        /* a profile more than the table holds, or a key more than a
         * profile does (the editor's keys beside the file's own) */
        puts_(msg, puts_(msg, puts_(msg, 0, "No room: at most ", UC_MAX_PROFILES),
                         " profiles of ", UC_MAX_KEYS), " settings.", -1);
        set_status(a, msg);
        return 0;
    }
    if (len < 0) {
        puts_(msg, puts_(msg, 0, "The file would be over ", CONF_MAX), " bytes.", -1);
        set_status(a, msg);
        return 0;
    }
    r = prefs_dos_save(a->buf, len, keep, &t, a->f.hlcat);
    if (r == PREFS_DOS_NODIR) {
        puts_(msg, puts_(msg, 0, "Cannot create ", -1), conf_dir[t], -1);
        set_status(a, msg);
        return 0;
    }
    if (r != PREFS_INSTALL_OK) {
        install_error(a, t, r);
        return 0;
    }
    /* both files are in place: the editor's table is what they hold */
    CopyMem(&a->work, &a->conf, sizeof(a->conf));
    strcpy(a->prof, name);
    a->prof_new = 0;
    a->tab_dirty = 0;
    a->orig = a->f;
    return 1;
}

/* ---- gadgets, and the two small windows ------------------------------------------------ */

/* One GadTools gadget at a box (window coordinates added here). */
static struct Gadget *gad(struct app *a, struct Gadget *prev, ULONG kind, const pl_box *b,
                          const char *label, UWORD id, ULONG place, struct TagItem *tags)
{
    struct NewGadget ng;
    ng.ng_LeftEdge = (WORD)(a->ox + b->x);
    ng.ng_TopEdge = (WORD)(a->oy + b->y);
    ng.ng_Width = (WORD)b->w;
    ng.ng_Height = (WORD)b->h;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_TextAttr = a->ta;
    ng.ng_GadgetID = id;
    ng.ng_Flags = place;
    ng.ng_VisualInfo = a->vi;
    ng.ng_UserData = 0;
    return CreateGadgetA(kind, prev, &ng, tags);
}

/* One gadget of the table. */
static struct Gadget *make_gadget(struct app *a, struct Gadget *prev, const pg_item *it)
{
    struct TagItem t[8];
    const pl_box *b = &a->lay.box[it->id];
    const char *label = pg_label(it);
    struct Gadget *g;
    int *p;
    int disabled = it->disabled && is_disabled(a, it), cap;
    switch (it->kind) {
    case PGK_STRING:
        text_field(a, it->id, &cap);
        return gad(a, prev, STRING_KIND, b, label, (UWORD)it->id,
                   label ? PLACETEXT_LEFT : 0,
                   tl(t, GTST_MaxChars, (ULONG)(cap - 1),
                      GT_Underscore, (ULONG)'_', GA_Disabled, (ULONG)disabled, TAG_DONE));
    case PGK_INTEGER:
        return gad(a, prev, INTEGER_KIND, b, label, (UWORD)it->id, PLACETEXT_LEFT,
                   tl(t, GTIN_MaxChars, 5, GTIN_Number, (ULONG)a->sb_lines,
                      GT_Underscore, (ULONG)'_', GA_Disabled, (ULONG)disabled, TAG_DONE));
    case PGK_BUTTON:
        return gad(a, prev, BUTTON_KIND, b, label, (UWORD)it->id, PLACETEXT_IN,
                   tl(t, GT_Underscore, (ULONG)'_', GA_Disabled, (ULONG)disabled, TAG_DONE));
    case PGK_CYCLE:
        if (it->id == PG_PROFILE)
            return gad(a, prev, CYCLE_KIND, b, label, (UWORD)it->id, PLACETEXT_LEFT,
                       tl(t, GTCY_Labels, (ULONG)a->plab, GT_Underscore, (ULONG)'_', TAG_DONE));
        return gad(a, prev, CYCLE_KIND, b, label, (UWORD)it->id, PLACETEXT_LEFT,
                   tl(t, GTCY_Labels, (ULONG)labels_of(it->id), GT_Underscore, (ULONG)'_',
                      GA_Disabled, (ULONG)disabled, TAG_DONE));
    case PGK_CHECK:
        p = int_of(a, it->id);
        return gad(a, prev, CHECKBOX_KIND, b, label, (UWORD)it->id, PLACETEXT_RIGHT,
                   tl(t, GTCB_Checked, (ULONG)(p ? *p : 0), GT_Underscore, (ULONG)'_',
                      GA_Disabled, (ULONG)disabled, TAG_DONE));
    case PGK_TEXT:
        return gad(a, prev, TEXT_KIND, b, label,
                   (UWORD)it->id, label ? PLACETEXT_LEFT : 0,
                   tl(t, GTTX_Border, TRUE, GTTX_CopyText, TRUE, GTTX_Clipped, TRUE,
                      GT_Underscore, (ULONG)'_', GA_Disabled, (ULONG)disabled, TAG_DONE));
    default: /* PGK_SWATCH: a gadget the program draws */
        g = gad(a, prev, GENERIC_KIND, b, 0, (UWORD)it->id, 0, 0);
        if (g) {
            g->Flags = GFLG_GADGHNONE | (disabled ? GFLG_DISABLED : 0);
            g->Activation = GACT_RELVERIFY;
            g->GadgetType |= GTYP_BOOLGADGET;
        }
        return g;
    }
}

/* The common gadgets and the five pages, from the table. 0 when GadTools
 * could not make one (out of memory). */
static int build_gadgets(struct app *a)
{
    struct Gadget *g;
    int page, i;

    g = CreateContext(&a->glist_common);
    for (i = 0; i < pg_nitems && g; i++)
        if (pg_items[i].page == PG_COMMON) {
            g = make_gadget(a, g, &pg_items[i]);
            a->gad[pg_items[i].id] = g;
        }
    if (!g)
        return 0;
    for (page = 0; page < PG_NPAGES; page++) {
        g = CreateContext(&a->glist[page]);
        for (i = 0; i < pg_nitems && g; i++)
            if (pg_items[i].page == page) {
                g = make_gadget(a, g, &pg_items[i]);
                a->gad[pg_items[i].id] = g;
            }
        if (!g)
            return 0;
    }
    return 1;
}

/* A small modal window: parent blocked by an empty requester, its own port. */
struct modal {
    struct Window *win;
    struct Gadget *glist;
    struct Requester rq;
    int blocked;
};

static void modal_close(struct app *a, struct modal *m)
{
    if (m->win)
        CloseWindow(m->win);
    if (m->blocked)
        EndRequest(&m->rq, a->win);
    m->win = 0;
    FreeGadgets(m->glist);
    m->glist = 0;
}

/* ask_name: a window with a string gadget and OK / Cancel. 1 and the name
 * (cleaned) in out when OK was chosen with a name. */
static int ask_name(struct app *a, const char *title, const char *initial, char *out, int cap)
{
    struct modal m;
    struct Gadget *g, *gname = 0, *gok = 0, *gcancel = 0;
    struct NewGadget ng;
    struct TagItem t[6];
    struct IntuiMessage *im;
    int fh = a->lay.font_h, fw = a->lay.font_w, h = fh + 4, w, bw, done = 0, ok = 0;
    WORD ox = (WORD)(a->scr->WBorLeft + MARGIN_X), oy = (WORD)(a->scr->WBorTop + a->scr->Font->ta_YSize + 1 + MARGIN_Y);
    LONG left, top;
    memset(&m, 0, sizeof(m));
    w = (PROFNAME_CAP + 2) * fw + 12;
    bw = 12 * fw;
    g = CreateContext(&m.glist);
    ng.ng_TextAttr = a->ta;
    ng.ng_VisualInfo = a->vi;
    ng.ng_UserData = 0;
    ng.ng_Flags = 0;
    ng.ng_LeftEdge = ox;
    ng.ng_TopEdge = (WORD)(oy);
    ng.ng_Width = (WORD)w;
    ng.ng_Height = (WORD)h;
    ng.ng_GadgetText = 0;
    ng.ng_GadgetID = PN_NAME;
    gname = g = CreateGadgetA(STRING_KIND, g, &ng,
                              tl(t, GTST_String, (ULONG)initial, GTST_MaxChars, (ULONG)(cap - 1), TAG_DONE));
    if (gname) /* the caret after the text, where typing goes on (ActivateGadget leaves it at the start) */
        ((struct StringInfo *)gname->SpecialInfo)->BufferPos = (WORD)strlen(initial);
    ng.ng_TopEdge = (WORD)(oy + h + 8);
    ng.ng_Width = (WORD)bw;
    ng.ng_GadgetText = (STRPTR)"OK";
    ng.ng_GadgetID = PN_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gok = g = CreateGadgetA(BUTTON_KIND, g, &ng, 0);
    ng.ng_LeftEdge = (WORD)(ox + w - bw);
    ng.ng_GadgetText = (STRPTR)"Cancel";
    ng.ng_GadgetID = PN_CANCEL;
    gcancel = g = CreateGadgetA(BUTTON_KIND, g, &ng, 0);
    if (!g) {
        FreeGadgets(m.glist);
        return 0;
    }
    left = (a->scr->Width - (w + 2 * MARGIN_X)) / 2;
    top = (a->scr->Height - (2 * h + 8 + 2 * MARGIN_Y)) / 3;
    {
        struct TagItem wt[12];
        int n = 0;
        wt[n].ti_Tag = WA_Left;        wt[n++].ti_Data = (ULONG)left;
        wt[n].ti_Tag = WA_Top;         wt[n++].ti_Data = (ULONG)top;
        wt[n].ti_Tag = WA_InnerWidth;  wt[n++].ti_Data = (ULONG)(w + 2 * MARGIN_X);
        wt[n].ti_Tag = WA_InnerHeight; wt[n++].ti_Data = (ULONG)(2 * h + 8 + 2 * MARGIN_Y);
        wt[n].ti_Tag = WA_IDCMP;       wt[n++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | BUTTONIDCMP |
                                                         STRINGIDCMP | IDCMP_VANILLAKEY;
        wt[n].ti_Tag = WA_Flags;       wt[n++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                                                         WFLG_ACTIVATE | WFLG_SMART_REFRESH;
        wt[n].ti_Tag = WA_Title;       wt[n++].ti_Data = (ULONG)title;
        wt[n].ti_Tag = WA_PubScreen;   wt[n++].ti_Data = (ULONG)a->scr;
        wt[n].ti_Tag = WA_Gadgets;     wt[n++].ti_Data = (ULONG)m.glist;
        wt[n].ti_Tag = TAG_DONE;       wt[n].ti_Data = 0;
        m.win = OpenWindowTagList(0, wt);
    }
    if (!m.win) {
        FreeGadgets(m.glist);
        return 0;
    }
    InitRequester(&m.rq);
    m.blocked = Request(&m.rq, a->win) ? 1 : 0;
    GT_RefreshWindow(m.win, 0);
    ActivateGadget(gname, m.win, 0);
    while (!done) {
        Wait(1UL << m.win->UserPort->mp_SigBit);
        while (!done && (im = GT_GetIMsg(m.win->UserPort))) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            struct Gadget *ig = (struct Gadget *)im->IAddress;
            GT_ReplyIMsg(im);
            if (cls == IDCMP_CLOSEWINDOW)
                done = 1;
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(m.win);
                GT_EndRefresh(m.win, TRUE);
            } else if (cls == IDCMP_VANILLAKEY && code == 0x1b)
                done = 1;
            else if (cls == IDCMP_GADGETUP && ig) {
                if (ig->GadgetID == PN_CANCEL)
                    done = 1;
                else if (ig->GadgetID == PN_OK || ig->GadgetID == PN_NAME) {
                    prefs_clean_name((const char *)((struct StringInfo *)gname->SpecialInfo)->Buffer, out, cap);
                    ok = out[0] != 0;
                    done = 1;
                }
            }
        }
    }
    (void)gok;
    (void)gcancel;
    modal_close(a, &m);
    return ok;
}

/* ---- the colour window -------------------------------------------------------------------- */

struct colwin {
    struct app *a;
    struct modal m;
    struct Gadget *gr, *gg, *gb, *ghex;
    Object *wheel, *grad;
    LONG pen;                 /* the preview swatch's pen */
    UWORD gpens[3];           /* the brightness gradient's pens, ~0 ended */
    uc_u32 rgb;
    prefs_hsb hsb;
    WORD sx, sy, sw, sh;      /* the preview swatch, window coordinates */
};

static void cw_swatch(struct colwin *c)
{
    struct RastPort *rp = c->m.win->RPort;
    if (c->pen >= 0) {
        ReleasePen(c->a->scr->ViewPort.ColorMap, (ULONG)c->pen);
        c->pen = -1;
    }
    c->pen = pen_for(c->a, c->rgb);
    {
        struct TagItem bt[3];
        DrawBevelBoxA(rp, c->sx, c->sy, c->sw, c->sh, tl(bt, GT_VisualInfo, (ULONG)c->a->vi, GTBB_Recessed, TRUE, TAG_DONE));
    }
    SetAPen(rp, c->pen >= 0 ? (UBYTE)c->pen : c->a->dri->dri_Pens[BACKGROUNDPEN]);
    RectFill(rp, c->sx + 2, c->sy + 1, c->sx + c->sw - 3, c->sy + c->sh - 2);
}

/* Show c->rgb / c->hsb in every control except the one it came from (id). */
static void cw_show(struct colwin *c, int from)
{
    struct TagItem t[4];
    char hex[8];
    struct Window *w = c->m.win;
    if (from != PC_RED)
        GT_SetGadgetAttrsA(c->gr, w, 0, tl(t, GTSL_Level, (ULONG)((c->rgb >> 16) & 0xFF), TAG_DONE));
    if (from != PC_GREEN)
        GT_SetGadgetAttrsA(c->gg, w, 0, tl(t, GTSL_Level, (ULONG)((c->rgb >> 8) & 0xFF), TAG_DONE));
    if (from != PC_BLUE)
        GT_SetGadgetAttrsA(c->gb, w, 0, tl(t, GTSL_Level, (ULONG)(c->rgb & 0xFF), TAG_DONE));
    if (from != PC_HEX) {
        prefs_hex_format(c->rgb, hex);
        GT_SetGadgetAttrsA(c->ghex, w, 0, tl(t, GTST_String, (ULONG)hex, TAG_DONE));
    }
    if (c->wheel && from != PC_WHEEL)
        SetGadgetAttrsA((struct Gadget *)c->wheel, w, 0,
                        tl(t, WHEEL_Hue, c->hsb.hue, WHEEL_Saturation, c->hsb.sat, TAG_DONE));
    if (c->grad && from != PC_BRIGHTNESS)
        SetGadgetAttrsA((struct Gadget *)c->grad, w, 0, tl(t, GRAD_CurVal, c->hsb.bri >> 16, TAG_DONE));
    cw_swatch(c);
}

/* A colour from the red / green / blue / hex side: the wheel follows (a grey
 * keeps the hue it had) */
static void cw_set_rgb(struct colwin *c, uc_u32 rgb, int from)
{
    prefs_hsb h;
    c->rgb = rgb;
    prefs_rgb_to_hsb(rgb, &h);
    if (h.sat == 0)
        h.hue = c->hsb.hue;
    c->hsb = h;
    cw_show(c, from);
}

static ULONG udiff(ULONG a, ULONG b)
{
    return a > b ? a - b : b - a;
}

/* pick_colour: the colour window on *rgb. 1 when OK was chosen (*rgb new). */
static int pick_colour(struct app *a, const char *title, uc_u32 *rgb)
{
    struct colwin c;
    struct NewGadget ng;
    struct TagItem t[8], wt[12];
    struct IntuiMessage *im;
    struct Gadget *g, *gok, *gcancel;
    int fh = a->lay.font_h, fw = a->lay.font_w, h = fh + 4, pitch = fh + 6, i, n;
    int wheel_sz = 7 * pitch, grad_w = 2 * fw, gap = 8;
    int lw, sw, x0, hexw, bw, inner_w, inner_h, done = 0, ok = 0;
    WORD ox = (WORD)(a->scr->WBorLeft + MARGIN_X), oy = (WORD)(a->scr->WBorTop + a->scr->Font->ta_YSize + 1 + MARGIN_Y);
    static const char *names[3] = { "_Red", "_Green", "_Blue" };
    static const int sids[3] = { PC_RED, PC_GREEN, PC_BLUE };
    struct Gadget **sg[3];
    struct RastPort mr;
    ULONG hue, sat;
    struct Library *wheelbase = 0, *gradbase = 0;
    char wtitle[100];
    memset(&c, 0, sizeof(c));
    c.a = a;
    c.pen = -1;
    c.gpens[0] = c.gpens[1] = c.gpens[2] = (UWORD)~0;
    c.rgb = *rgb;
    prefs_rgb_to_hsb(*rgb, &c.hsb);
    sg[0] = &c.gr;
    sg[1] = &c.gg;
    sg[2] = &c.gb;
    /* measure with the screen's font */
    InitRastPort(&mr);
    SetFont(&mr, a->scr->RastPort.Font);
    lw = 0;
    for (i = 0; i < 3; i++) {
        char plain[16];
        pg_plain_label(names[i], plain, sizeof(plain));
        n = (int)TextLength(&mr, (STRPTR)plain, (ULONG)strlen(plain));
        if (n > lw)
            lw = n;
    }
    lw += 6;
    sw = 20 * fw;
    x0 = wheel_sz + gap + grad_w + gap;
    hexw = 8 * fw + 8;
    bw = 12 * fw;
    inner_w = x0 + lw + sw + 6 * fw;      /* the slider's number shows right of it */
    inner_h = wheel_sz + gap + h;
    c.sx = (WORD)(ox + x0);
    c.sy = (WORD)(oy + 4 * pitch + 2);
    c.sw = (WORD)(lw + sw);
    c.sh = (WORD)(2 * pitch - 4);
    g = CreateContext(&c.m.glist);
    ng.ng_TextAttr = a->ta;
    ng.ng_VisualInfo = a->vi;
    ng.ng_UserData = 0;
    for (i = 0; i < 3 && g; i++) {
        ng.ng_LeftEdge = (WORD)(ox + x0 + lw);
        ng.ng_TopEdge = (WORD)(oy + i * pitch);
        ng.ng_Width = (WORD)sw;
        ng.ng_Height = (WORD)h;
        ng.ng_GadgetText = (STRPTR)names[i];
        ng.ng_GadgetID = (UWORD)sids[i];
        ng.ng_Flags = PLACETEXT_LEFT;
        *sg[i] = g = CreateGadgetA(SLIDER_KIND, g, &ng,
                                   tl(t, GTSL_Min, 0, GTSL_Max, 255,
                                      GTSL_Level, (ULONG)(i == 0 ? (c.rgb >> 16) & 0xFF
                                                          : i == 1 ? (c.rgb >> 8) & 0xFF : c.rgb & 0xFF),
                                      GTSL_MaxLevelLen, 3, GTSL_LevelFormat, (ULONG)"%ld",
                                      GTSL_LevelPlace, PLACETEXT_RIGHT, GT_Underscore, (ULONG)'_',
                                      GA_RelVerify, TRUE, GA_Immediate, TRUE, TAG_DONE));
    }
    ng.ng_LeftEdge = (WORD)(ox + x0 + lw);
    ng.ng_TopEdge = (WORD)(oy + 3 * pitch);
    ng.ng_Width = (WORD)hexw;
    ng.ng_Height = (WORD)h;
    ng.ng_GadgetText = (STRPTR)"_Hex";
    ng.ng_GadgetID = PC_HEX;
    ng.ng_Flags = PLACETEXT_LEFT;
    {
        char hex[8];
        prefs_hex_format(c.rgb, hex);
        c.ghex = g = g ? CreateGadgetA(STRING_KIND, g, &ng,
                                       tl(t, GTST_String, (ULONG)hex, GTST_MaxChars, 7,
                                          GT_Underscore, (ULONG)'_', TAG_DONE)) : 0;
    }
    ng.ng_TopEdge = (WORD)(oy + wheel_sz + gap);
    ng.ng_LeftEdge = ox;
    ng.ng_Width = (WORD)bw;
    ng.ng_GadgetText = (STRPTR)"_OK";
    ng.ng_GadgetID = PC_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gok = g = g ? CreateGadgetA(BUTTON_KIND, g, &ng, tl(t, GT_Underscore, (ULONG)'_', TAG_DONE)) : 0;
    ng.ng_LeftEdge = (WORD)(ox + inner_w - bw);
    ng.ng_GadgetText = (STRPTR)"_Cancel";
    ng.ng_GadgetID = PC_CANCEL;
    gcancel = g = g ? CreateGadgetA(BUTTON_KIND, g, &ng, tl(t, GT_Underscore, (ULONG)'_', TAG_DONE)) : 0;
    if (!g) {
        FreeGadgets(c.m.glist);
        return 0;
    }
    n = 0;
    wt[n].ti_Tag = WA_Left;        wt[n++].ti_Data = (ULONG)((a->scr->Width - inner_w - 2 * MARGIN_X) / 2);
    wt[n].ti_Tag = WA_Top;         wt[n++].ti_Data = (ULONG)((a->scr->Height - inner_h) / 3);
    wt[n].ti_Tag = WA_InnerWidth;  wt[n++].ti_Data = (ULONG)(inner_w + 2 * MARGIN_X);
    wt[n].ti_Tag = WA_InnerHeight; wt[n++].ti_Data = (ULONG)(inner_h + 2 * MARGIN_Y);
    wt[n].ti_Tag = WA_IDCMP;       wt[n++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | BUTTONIDCMP |
                                                     STRINGIDCMP | SLIDERIDCMP | IDCMP_VANILLAKEY | IDCMP_IDCMPUPDATE;
    wt[n].ti_Tag = WA_Flags;       wt[n++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                                                     WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    wt[n].ti_Tag = WA_Title;       wt[n++].ti_Data = (ULONG)title;
    wt[n].ti_Tag = WA_PubScreen;   wt[n++].ti_Data = (ULONG)a->scr;
    wt[n].ti_Tag = WA_Gadgets;     wt[n++].ti_Data = (ULONG)c.m.glist;
    wt[n].ti_Tag = TAG_DONE;       wt[n].ti_Data = 0;
    c.m.win = OpenWindowTagList(0, wt);
    if (!c.m.win) {
        FreeGadgets(c.m.glist);
        return 0;
    }
    InitRequester(&c.m.rq);
    c.m.blocked = Request(&c.m.rq, a->win) ? 1 : 0;
    /* the wheel and the brightness slider: BOOPSI, not GadTools; when the
     * classes or the pens are not there the sliders and the hex field do it all */
    {
        LONG p0 = pen_for(a, 0x000000UL), p1 = pen_for(a, 0xFFFFFFUL);
        const char *why = 0;
        if (!prefs_grad_pens(c.gpens, p0, p1)) {
            if (p0 >= 0)
                ReleasePen(a->scr->ViewPort.ColorMap, (ULONG)p0);
            if (p1 >= 0)
                ReleasePen(a->scr->ViewPort.ColorMap, (ULONG)p1);
            why = "No free colours for the colour wheel: use the sliders.";
        } else if (!(wheelbase = OpenLibrary((STRPTR)"gadgets/colorwheel.gadget", 39L))) {
            why = "The colour wheel needs gadgets/colorwheel.gadget, which cannot be opened: use the sliders.";
        } else if (!(gradbase = OpenLibrary((STRPTR)"gadgets/gradientslider.gadget", 39L))) {
            why = "The colour wheel needs gadgets/gradientslider.gadget, which cannot be opened: use the sliders.";
        }
        if (!why) {
            struct TagItem wl[14], gl[14];
            c.wheel = NewObjectA(0, (UBYTE *)"colorwheel.gadget",
                                 tl(wl, GA_Left, (ULONG)ox, GA_Top, (ULONG)oy, GA_Width, (ULONG)wheel_sz,
                                    GA_Height, (ULONG)wheel_sz, GA_ID, PC_WHEEL, WHEEL_Screen, (ULONG)a->scr,
                                    WHEEL_MaxPens, 16, WHEEL_BevelBox, TRUE, WHEEL_Hue, c.hsb.hue,
                                    WHEEL_Saturation, c.hsb.sat, ICA_TARGET, (ULONG)ICTARGET_IDCMP, TAG_DONE));
            if (c.wheel)
                c.grad = NewObjectA(0, (UBYTE *)"gradientslider.gadget",
                                    tl(gl, GA_Left, (ULONG)(ox + wheel_sz + gap), GA_Top, (ULONG)oy,
                                       GA_Width, (ULONG)grad_w, GA_Height, (ULONG)wheel_sz, GA_ID, PC_BRIGHTNESS,
                                       GA_Previous, (ULONG)c.wheel, GRAD_MaxVal, 0xFFFF,
                                       GRAD_CurVal, c.hsb.bri >> 16, GRAD_PenArray, (ULONG)c.gpens,
                                       PGA_FREEDOM, FREEVERT, ICA_TARGET, (ULONG)ICTARGET_IDCMP, TAG_DONE));
            if (c.wheel && c.grad)
                AddGList(c.m.win, (struct Gadget *)c.wheel, (UWORD)~0, 2, 0);
            else {
                if (c.wheel)
                    DisposeObject(c.wheel);
                c.wheel = 0;
                why = "The colour wheel could not be made: use the sliders.";
            }
        }
        if (why)
            set_status(a, why);
        else {
            /* the sentinel: the title names the wheel only when the object exists */
            strncpy(wtitle, title, 60);
            wtitle[60] = 0;
            strcat(wtitle, " (colour wheel)");
            SetWindowTitles(c.m.win, (STRPTR)wtitle, (STRPTR)~0);
        }
    }
    GT_RefreshWindow(c.m.win, 0);
    if (c.wheel)
        RefreshGList((struct Gadget *)c.wheel, c.m.win, 0, 2);
    cw_swatch(&c);
    while (!done) {
        Wait(1UL << c.m.win->UserPort->mp_SigBit);
        while (!done && (im = GT_GetIMsg(c.m.win->UserPort))) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            struct Gadget *ig = (struct Gadget *)im->IAddress;
            struct TagItem *upd = cls == IDCMP_IDCMPUPDATE ? (struct TagItem *)im->IAddress : 0;
            ULONG upd_id = upd ? GetTagData(GA_ID, 0, upd) : 0;
            ULONG upd_hue = upd ? GetTagData(WHEEL_Hue, c.hsb.hue, upd) : 0;
            ULONG upd_sat = upd ? GetTagData(WHEEL_Saturation, c.hsb.sat, upd) : 0;
            ULONG upd_bri = upd ? GetTagData(GRAD_CurVal, c.hsb.bri >> 16, upd) : 0;
            GT_ReplyIMsg(im);
            if (cls == IDCMP_CLOSEWINDOW)
                done = 1;
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(c.m.win);
                GT_EndRefresh(c.m.win, TRUE);
                cw_swatch(&c);
            } else if (cls == IDCMP_VANILLAKEY) {
                if (code == 0x1b || code == 'c' || code == 'C')
                    done = 1;
                else if (code == 'o' || code == 'O' || code == 0x0d) {
                    ok = 1;
                    done = 1;
                } else if (code == 'h' || code == 'H')
                    ActivateGadget(c.ghex, c.m.win, 0);
            } else if ((cls == IDCMP_MOUSEMOVE || cls == IDCMP_GADGETUP) && ig) {
                uc_u32 v = c.rgb;
                switch (ig->GadgetID) {
                case PC_RED:
                    v = (v & 0x00FFFFUL) | ((uc_u32)code << 16);
                    cw_set_rgb(&c, v, PC_RED);
                    break;
                case PC_GREEN:
                    v = (v & 0xFF00FFUL) | ((uc_u32)code << 8);
                    cw_set_rgb(&c, v, PC_GREEN);
                    break;
                case PC_BLUE:
                    v = (v & 0xFFFF00UL) | (uc_u32)code;
                    cw_set_rgb(&c, v, PC_BLUE);
                    break;
                case PC_HEX:
                    if (prefs_hex_parse((const char *)((struct StringInfo *)c.ghex->SpecialInfo)->Buffer, &v))
                        cw_set_rgb(&c, v, PC_HEX);
                    else {
                        DisplayBeep(a->scr);
                        cw_show(&c, 0);
                    }
                    break;
                case PC_OK:
                    ok = 1;
                    done = 1;
                    break;
                case PC_CANCEL:
                    done = 1;
                    break;
                default:
                    break;
                }
            } else if (cls == IDCMP_IDCMPUPDATE && upd) {
                /* the wheel or the brightness slider moved; a notification
                 * that only echoes what a slider set is ignored */
                if (upd_id == PC_WHEEL && (udiff(upd_hue, c.hsb.hue) > 0x00100000UL ||
                                           udiff(upd_sat, c.hsb.sat) > 0x00100000UL)) {
                    hue = upd_hue;
                    sat = upd_sat;
                    c.hsb.hue = hue;
                    c.hsb.sat = sat;
                    c.rgb = prefs_hsb_to_rgb(&c.hsb);
                    cw_show(&c, PC_WHEEL);
                } else if (upd_id == PC_BRIGHTNESS && udiff(upd_bri, c.hsb.bri >> 16) > 8) {
                    c.hsb.bri = upd_bri * 65537UL;
                    c.rgb = prefs_hsb_to_rgb(&c.hsb);
                    cw_show(&c, PC_BRIGHTNESS);
                }
            }
        }
    }
    (void)gok;
    (void)gcancel;
    if (c.wheel) {
        RemoveGList(c.m.win, (struct Gadget *)c.wheel, 2);
        DisposeObject(c.grad);
        DisposeObject(c.wheel);
    }
    if (gradbase)
        CloseLibrary(gradbase);
    if (wheelbase)
        CloseLibrary(wheelbase);
    if (c.pen >= 0)
        ReleasePen(a->scr->ViewPort.ColorMap, (ULONG)c.pen);
    for (i = 0; i < 2; i++)
        if (c.gpens[i] != (UWORD)~0)
            ReleasePen(a->scr->ViewPort.ColorMap, (ULONG)c.gpens[i]);
    modal_close(a, &c.m);
    if (ok)
        *rgb = c.rgb;
    return ok;
}

/* ---- the pickers, and the profile list ------------------------------------------------------ */

/* Theme...: a theme file from the themes drawer (prefs_dos_theme_drawer,
 * as the window's Settings > Theme...: W30), its colours into the profile
 * being edited (prefs_apply_theme); Save or Use writes them. */
static void pick_theme(struct app *a)
{
    char path[300];
    BPTR f;
    long got;
    int named = 0;
    prefs_dos_theme_drawer(a->theme, "PROGDIR:", path, sizeof(path));
    if (!asl_pick_file(0, a->win, "Choose a theme", path, "#?.conf", 1, path, sizeof(path), &named) || !named)
        return;
    f = Open((STRPTR)path, MODE_OLDFILE);
    if (!f) {
        set_status(a, "The theme file cannot be opened.");
        return;
    }
    got = Read(f, a->buf, CONF_MAX); /* the file text is parsed already: buf is free */
    Close(f);
    a->buf[got > 0 ? got : 0] = 0;
    if (got > 0 && prefs_apply_theme(&a->f, &a->work, a->buf, got)) {
        strcpy(a->theme, path);
        show_fields(a);
        set_status(a, "Theme in the fields: Save or Use writes it.");
    } else
        set_status(a, "That file holds no colours.");
}

static void pick_font(struct app *a)
{
    char name[UC_MAX_VALUE], iname[UC_MAX_VALUE], spec[UC_MAX_VALUE];
    long size = 0, isize = 0;
    iname[0] = 0;
    prefs_font_parse(a->f.font, iname, sizeof(iname), &isize);
    if (!asl_pick_font(0, a->win, "UP-Term font", iname, isize, 1, name, sizeof(name), &size))
        return;
    if (!prefs_font_compose(name, size, spec, sizeof(spec))) {
        set_status(a, "The font's name has a blank or a colon: not usable.");
        return;
    }
    strcpy(a->f.font, spec);
    show_texts(a);
    set_status(a, "Font chosen: Save or Use writes it.");
}

static void pick_fallback(struct app *a)
{
    char path[300], name[UC_MAX_VALUE];
    int named = 0;
    if (!asl_pick_file(0, a->win, "Choose an outline font", "FONTS:", "#?.otag", 1, path, sizeof(path), &named) ||
        !named)
        return;
    if (!prefs_otag_basename(path, name, sizeof(name))) {
        set_status(a, "That is not an outline font (.otag).");
        return;
    }
    strcpy(a->f.fallback, name);
    show_texts(a);
    set_status(a, "Fallback font chosen: Save or Use writes it.");
}

static void pick_mode(struct app *a)
{
    unsigned long id = 0, nid;
    int depth = prefs_depth_parse(a->f.screendepth), ndepth;
    if (!prefs_mode_parse(a->f.screenmode, &id))
        id = 0;
    if (!asl_pick_screenmode(a->win, "UP-Term screen mode", id, depth, PREFS_DEPTH_MIN, PREFS_DEPTH_MAX, &nid, &ndepth))
        return;
    prefs_mode_format(nid, a->f.screenmode);
    prefs_depth_format(ndepth, a->f.screendepth);
    show_texts(a);
    set_status(a, "Screen mode chosen: Save or Use writes it.");
}

/* A swatch was clicked: the colour window on its colour; OK puts the colour in the field. */
static void pick_swatch(struct app *a, int id)
{
    uc_u32 rgb = 0x808080UL;
    int cap;
    char *s = text_field(a, swatch_hex(id), &cap);
    const pg_item *it = pg_find(id);
    char title[60];
    if (!s)
        return;
    collect(a);
    swatch_rgb(a, id, &rgb);
    strcpy(title, "Colour: ");
    pg_plain_label(pg_label(it), title + 8, (int)sizeof(title) - 8);
    if (pick_colour(a, title, &rgb)) {
        prefs_hex_format(rgb, s);
        set_attr(a, swatch_hex(id), GTST_String, (ULONG)s);
        swatch_pen(a, id);
        draw_swatch(a, id);
        apply_enabled(a);
        set_status(a, "Colour chosen: Save or Use writes it.");
    }
}

/* The Profile gadget chose entry idx. */
static void switch_profile(struct app *a, int idx)
{
    char name[UC_NAME];
    int n, at;
    prof_labels(a);
    n = count_of(a->plab);
    if (idx < 0 || idx >= n)
        return;
    strcpy(name, (const char *)a->plab[idx]);
    if (!strcmp(name, a->prof))
        return;
    if (!confirm_discard(a)) {
        at = prof_labels(a);
        set_attr(a, PG_PROFILE, GTCY_Active, (ULONG)at);
        return;
    }
    a->tab_dirty = a->tab_dirty && a->conf.nprof > 0;
    load_profile(a, name);
    set_status(a, "Loaded.");
}

static void profile_new(struct app *a)
{
    char name[UC_NAME];
    if (!confirm_discard(a))
        return;
    if (!ask_name(a, "New profile", "", name, sizeof(name)))
        return;
    if (prefs_profile_exists(&a->conf, name)) {
        set_status(a, "A profile with that name exists.");
        return;
    }
    load_profile(a, name);
    set_status(a, "New profile: Save writes it.");
}

static void profile_rename(struct app *a)
{
    char name[UC_NAME];
    int r;
    if (!ask_name(a, "Rename profile", a->prof, name, sizeof(name)))
        return;
    if (!strcmp(name, a->prof))
        return;
    if (a->prof_new) {
        if (prefs_profile_exists(&a->conf, name)) {
            set_status(a, "A profile with that name exists.");
            return;
        }
        strcpy(a->prof, name);
    } else {
        r = prefs_rename_profile(&a->conf, a->prof, name);
        if (r == PREFS_RENAME_EXISTS) {
            set_status(a, "A profile with that name exists.");
            return;
        }
        if (r != PREFS_RENAME_OK) {
            set_status(a, "The profile cannot be renamed.");
            return;
        }
        strcpy(a->prof, name);
        a->tab_dirty = 1;
    }
    show_fields(a);
    set_status(a, "Renamed: Save or Use writes the file.");
}

static void profile_delete(struct app *a)
{
    char name[UC_NAME];
    if (!confirm(a, "Delete the profile '%s'?", "Delete", "Keep", a->prof))
        return;
    strcpy(name, a->prof);
    if (!a->prof_new && upconf_rmprof(&a->conf, name))
        a->tab_dirty = 1;
    if (a->conf.nprof > 0)
        load_profile(a, a->conf.prof[0]);
    else
        load_profile(a, "default");
    set_status(a, "Deleted: Save or Use writes the file.");
}

/* ---- actions --------------------------------------------------------------------------------- */

static int get_state(struct app *a, int id)
{
    int *p;
    if (id == PG_CURSOR_INVERSE)
        return cursor_inverse(a);
    p = int_of(a, id);
    return p ? *p : 0;
}

static void put_state(struct app *a, int id, int v)
{
    int *p = int_of(a, id);
    if (id == PG_CURSOR_INVERSE) {
        if (v)
            strcpy(a->f.curcol, "inverse");
        else
            a->f.curcol[0] = 0;
        set_attr(a, PG_CURSOR_COLOUR_HEX, GTST_String, (ULONG)a->f.curcol);
    } else if (p)
        *p = v;
    if (id == PG_SCROLLBACK_NONE)
        sb_store(a);
    if (id == PG_COLORS)
        strcpy(a->f.colors, prefs_colors_value(a->colors));
    if (id == PG_KC_MATCH || id == PG_KC_COMMON || id == PG_KC_SILENT) {
        kc_store(a);
        set_attr(a, PG_KC_COMMON, GTCB_Checked, (ULONG)(a->kc.match == PREFS_KC_COMMON_ONLY || a->kc.common));
    }
    swatch_pen(a, PG_CURSOR_COLOUR);
    apply_enabled(a);
}

/* A gadget was released or reached by its key. val: a cycle's entry, a check
 * box's state. 1 when the editor is done. */
static int act(struct app *a, int id, int val)
{
    const pg_item *it = pg_find(id);
    if (!it)
        return 0;
    switch (id) {
    case PG_PAGE:
        show_page(a, val >= 0 && val < PG_NPAGES ? val : 0);
        return 0;
    case PG_PROFILE:
        switch_profile(a, val);
        return 0;
    case PG_PROFILE_NEW:
        profile_new(a);
        return 0;
    case PG_PROFILE_RENAME:
        profile_rename(a);
        return 0;
    case PG_PROFILE_DELETE:
        profile_delete(a);
        return 0;
    case PG_FONT:
    case PG_FONT_CHOOSE:
        pick_font(a);
        return 0;
    case PG_FONT_CLEAR:
        a->f.font[0] = 0;
        show_texts(a);
        return 0;
    case PG_FALLBACK:
    case PG_FALLBACK_CHOOSE:
        pick_fallback(a);
        return 0;
    case PG_FALLBACK_CLEAR:
        a->f.fallback[0] = 0;
        show_texts(a);
        return 0;
    case PG_SCREEN_MODE:
    case PG_SCREEN_CHOOSE:
        pick_mode(a);
        return 0;
    case PG_SCREEN_WORKBENCH:
        a->f.screenmode[0] = 0;
        a->f.screendepth[0] = 0;
        show_texts(a);
        return 0;
    case PG_THEME:
        collect(a); /* what was typed stays, the theme replaces only colours */
        pick_theme(a);
        return 0;
    case PG_LINK_DEFAULT:
        a->f.linkopen[0] = 0;
        set_attr(a, PG_LINK, GTST_String, (ULONG)a->f.linkopen);
        return 0;
    case PG_SAVE:
        return commit(a, 1);
    case PG_USE:
        return commit(a, 0);
    case PG_CANCEL:
        return 1; /* an explicit discard: nothing written, nothing asked */
    default:
        break;
    }
    if (it->kind == PGK_SWATCH) {
        pick_swatch(a, id);
        return 0;
    }
    if (it->kind == PGK_CYCLE || it->kind == PGK_CHECK) {
        put_state(a, id, val);
        return 0;
    }
    if (it->kind == PGK_STRING || it->kind == PGK_INTEGER) {
        /* Return or Tab in a field: the swatch follows a hex field's text */
        int sw = id >= PG_PAL_HEX && id < PG_PAL_HEX + 16 ? id - (PG_PAL_HEX - PG_PAL)
                 : is_swatch(id - 1) ? id - 1 : 0;
        collect(a);
        if (sw) {
            swatch_pen(a, sw);
            draw_swatch(a, sw);
        }
    }
    return 0;
}

/* A key: the gadget its letter reaches is used as if it was clicked. */
static int on_key(struct app *a, int key)
{
    const pg_item *it;
    struct Gadget *g;
    int v, n;
    if (key == 0x1b)
        return confirm_discard(a); /* Esc closes the window like its close gadget */
    it = pg_for_key(a->page, key);
    if (!it)
        return 0;
    g = a->gad[it->id];
    if ((it->disabled && is_disabled(a, it)) || !g)
        return 0;
    switch (it->kind) {
    case PGK_STRING:
    case PGK_INTEGER:
        ActivateGadget(g, a->win, 0);
        return 0;
    case PGK_CHECK:
        v = !get_state(a, it->id);
        set_attr(a, it->id, GTCB_Checked, (ULONG)v);
        return act(a, it->id, v);
    case PGK_CYCLE:
        if (it->id == PG_PAGE)
            n = PG_NPAGES;
        else if (it->id == PG_PROFILE)
            prof_labels(a), n = count_of(a->plab);
        else
            n = count_of(labels_of(it->id));
        if (n < 1)
            return 0;
        {
            int cur = it->id == PG_PAGE ? a->page : it->id == PG_PROFILE ? prof_labels(a) : get_state(a, it->id);
            int back = key >= 'A' && key <= 'Z';
            v = back ? (cur + n - 1) % n : (cur + 1) % n;
        }
        set_attr(a, it->id, GTCY_Active, (ULONG)v);
        return act(a, it->id, v);
    default: /* a button, a text box with a requester, a swatch */
        return act(a, it->id, 0);
    }
}

/* A gadget released: its code and, for a check box, its state. */
static int gadget_up(struct app *a, struct Gadget *g, UWORD code)
{
    const pg_item *it = pg_find(g->GadgetID);
    if (!it)
        return 0;
    if (it->kind == PGK_CHECK)
        return act(a, it->id, (g->Flags & GFLG_SELECTED) ? 1 : 0);
    return act(a, it->id, code);
}

/* ---- the window --------------------------------------------------------------------------- */

static struct Window *open_window(struct app *a)
{
    struct TagItem tags[14];
    int n = 0;
    LONG iw = a->lay.inner_w + 2 * MARGIN_X, ih = a->lay.inner_h + 2 * MARGIN_Y;
    LONG left = (a->scr->Width - iw) / 2, top = (a->scr->Height - ih) / 2;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    tags[n].ti_Tag = WA_Left;        tags[n++].ti_Data = (ULONG)left;
    tags[n].ti_Tag = WA_Top;         tags[n++].ti_Data = (ULONG)top;
    tags[n].ti_Tag = WA_InnerWidth;  tags[n++].ti_Data = (ULONG)iw;
    tags[n].ti_Tag = WA_InnerHeight; tags[n++].ti_Data = (ULONG)ih;
    tags[n].ti_Tag = WA_AutoAdjust;  tags[n++].ti_Data = TRUE;
    tags[n].ti_Tag = WA_IDCMP;       tags[n++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                                     IDCMP_VANILLAKEY | IDCMP_MOUSEMOVE |
                                                     BUTTONIDCMP | STRINGIDCMP | CYCLEIDCMP |
                                                     CHECKBOXIDCMP;
    /* the pointer over a gadget shows its help line: MOUSEMOVE, tested
     * against the layout (GadTools gadgets do not answer IDCMP_GADGETHELP) */
    tags[n].ti_Tag = WA_ReportMouse; tags[n++].ti_Data = TRUE;
    tags[n].ti_Tag = WA_Flags;       tags[n++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                                     WFLG_CLOSEGADGET | WFLG_ACTIVATE |
                                                     WFLG_SMART_REFRESH;
    tags[n].ti_Tag = WA_Title;       tags[n++].ti_Data = (ULONG)"UP-Term Prefs";
    tags[n].ti_Tag = WA_PubScreen;   tags[n++].ti_Data = (ULONG)a->scr;
    tags[n].ti_Tag = WA_Gadgets;     tags[n++].ti_Data = (ULONG)a->glist_common;
    tags[n].ti_Tag = TAG_DONE;       tags[n].ti_Data = 0;
    return OpenWindowTagList(0, tags);
}

/* The screen's font measures the labels. */
static int measure(void *ctx, const char *s)
{
    return (int)TextLength((struct RastPort *)ctx, (STRPTR)s, (ULONG)strlen(s));
}

int main(void)
{
    struct app *a = 0;
    struct IntuiMessage *im;
    struct RastPort mrp;
    pl_font pf;
    ULONG sig;
    int done = 0, rc = 1, i;

    /* `UP-Term Prefs ?` shows the usage and ends; the tool takes no arguments */
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)"UP-Term Prefs: no arguments; opens the preferences window\n");
        return RETURN_OK;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39L);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39L);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39L);
    UtilityBase = OpenLibrary((STRPTR)"utility.library", 39L);
    if (!IntuitionBase || !GfxBase || !GadToolsBase || !UtilityBase)
        goto out;

    a = (struct app *)AllocVec(sizeof(*a), MEMF_ANY | MEMF_CLEAR);
    if (!a)
        goto out;
    a->page = -1;
    for (i = 0; i < PL_MAX_IDS; i++)
        a->pen[i] = -1;

    a->scr = LockPubScreen(0);
    if (!a->scr)
        goto out;
    a->vi = GetVisualInfoA(a->scr, 0);
    a->dri = GetScreenDrawInfo(a->scr);
    if (!a->vi || !a->dri)
        goto out;
    /* the layout: the screen's font, or topaz 8 when the window would not fit */
    InitRastPort(&mrp);
    SetFont(&mrp, a->scr->RastPort.Font);
    pf.h = a->scr->Font->ta_YSize;
    pf.w = (int)TextLength(&mrp, (STRPTR)"0", 1);
    pf.text_w = measure;
    pf.ctx = &mrp;
    pl_layout(&pf, a->scr->Width - a->scr->WBorLeft - a->scr->WBorRight - 2 * MARGIN_X,
              a->scr->Height - (a->scr->BarHeight + 1) - (a->scr->WBorTop + a->scr->Font->ta_YSize + 1) -
              a->scr->WBorBottom - 2 * MARGIN_Y, &a->lay);
    a->ta = a->lay.fallback ? &topaz8 : a->scr->Font;
    a->ox = (WORD)(a->scr->WBorLeft + MARGIN_X);
    a->oy = (WORD)(a->scr->WBorTop + a->scr->Font->ta_YSize + 1 + MARGIN_Y);

    /* the profile first: the gadgets are made with its values */
    load_file(a);
    if (!prefs_profile_exists(&a->conf, "default") && a->conf.nprof > 0)
        strcpy(a->prof, a->conf.prof[0]);
    else
        strcpy(a->prof, "default");
    if (prefs_profile_exists(&a->conf, a->prof))
        prefs_from_conf(&a->f, &a->conf, a->prof);
    else
        prefs_defaults(&a->f);
    a->prof_new = 0; /* the first profile is the file's own, or "default" as built in: nothing to lose */
    a->orig = a->f;
    sb_load(a);
    a->colors = prefs_colors_index(a->f.colors);
    prefs_kc_decode(a->f.kcmode, &a->kc);
    prof_labels(a);
    if (!build_gadgets(a))
        goto out;

    a->win = open_window(a);
    if (!a->win)  /* the screen stays locked while the window lives: the small windows open on it */
        goto out;
    GT_RefreshWindow(a->win, 0);
    show_page(a, 0);
    show_fields(a);
    show_line(a, "Move the pointer over a control for its help.");

    sig = 1UL << a->win->UserPort->mp_SigBit;
    while (!done) {
        Wait(sig);
        while (!done && (im = GT_GetIMsg(a->win->UserPort))) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            WORD mx = im->MouseX, my = im->MouseY;
            struct Gadget *g = (struct Gadget *)im->IAddress;
            GT_ReplyIMsg(im);
            if (cls == IDCMP_CLOSEWINDOW)
                done = confirm_discard(a);
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(a->win);
                GT_EndRefresh(a->win, TRUE);
                draw_swatches(a);
            } else if (cls == IDCMP_VANILLAKEY)
                done = on_key(a, code);
            else if (cls == IDCMP_MOUSEMOVE)
                show_help(a, pl_hit(&a->lay, a->page, mx - a->ox, my - a->oy));
            else if (cls == IDCMP_GADGETUP && g)
                done = gadget_up(a, g, code);
        }
    }
    rc = 0;

out:
    /* whatever was made, undone once, in the reverse order */
    if (a) {
        if (a->win) {
            /* the shown page is chained behind the common gadgets (AddGList):
             * unhook it first, or FreeGadgets(glist_common) walks on into
             * the page FreeGadgets(glist[page]) already freed -- closing the
             * window took the machine down (rig, 2026-10-02) */
            if (a->page >= 0)
                RemoveGList(a->win, a->glist[a->page], -1);
            CloseWindow(a->win);
        }
        /* each list on its own now: freed once each */
        for (i = PG_NPAGES - 1; i >= 0; i--)
            FreeGadgets(a->glist[i]);
        FreeGadgets(a->glist_common);
        if (a->scr)
            for (i = 0; i < PL_MAX_IDS; i++)
                if (a->pen[i] >= 0)
                    ReleasePen(a->scr->ViewPort.ColorMap, (ULONG)a->pen[i]);
        if (a->dri && a->scr)
            FreeScreenDrawInfo(a->scr, a->dri);
        if (a->vi)
            FreeVisualInfo(a->vi);
        if (a->scr)
            UnlockPubScreen(0, a->scr);
        FreeVec(a);
    }
    if (UtilityBase)
        CloseLibrary(UtilityBase);
    if (GadToolsBase)
        CloseLibrary(GadToolsBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
