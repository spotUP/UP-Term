/* UP-Term Prefs: the settings editor for up-term/up-term in ENV: and
 * ENVARC: -- the profiles the XCON: handler reads for every window (plan
 * thoughts/shared/plans/2026-10-01-terminal-preferences.md).
 *
 * C:UP-Term Prefs opens a window with two pages, General and Colors, picked
 * with the Page gadget. The fields edit one profile at a time: name it in
 * the Profile field, Load it (or start it with New), change the values, then
 * the Amiga Prefs buttons: Save writes ENVARC: and ENV: (kept across a
 * reboot), Use writes ENV: only (until the reboot), Cancel writes nothing.
 * The previous file is kept as up-term.orig. New XCON: windows use the file;
 * open windows keep their own settings (live apply is a later phase).
 *
 * The window is GadTools. Each page is a gadget list of its own; the shown
 * one is in the window, the other is held off it, so no two gadgets ever
 * share a place on screen. What reaches the file is decided in
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
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <libraries/asl.h>

#include "../config/upconf.h"
#include "prefs_core.h"
#include "prefs_dos.h"

/* The clib's calls read these; startup.o provides DOSBase only. */
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *AslBase;      /* the theme requester; 0: no Theme button action */

/* ENV: is what is in use now, ENVARC: what the next boot copies to ENV:. */
#define T_ENV PREFS_T_ENV
#define T_ENVARC PREFS_T_ENVARC
#define conf_dir prefs_conf_dir
#define conf_path prefs_conf_path

/* the same cap the handler reads with, so a file this writes is always a
 * file the handler can read whole */
#define CONF_MAX   UC_MAX_FILE
#define STATUS_MAX 128

/* layout, in pixels of topaz 8 from the window's inner top left */
#define ROW(i)    (20 + (i) * 16)
#define FIELD_X   128
#define AREA_W    466
#define PAGE_ROWS 11
#define STATUS_Y  (ROW(PAGE_ROWS) + 4)
#define BUTTON_Y  (STATUS_Y + 20)
#define INNER_W   (AREA_W + 16)
#define INNER_H   (4 + BUTTON_Y + 14 + 6)

enum {
    ID_PAGE = 1,
    ID_PROF, ID_LOAD, ID_NEW, ID_DEL,
    ID_FONT, ID_SB, ID_CURCOL, ID_FALLBACK,
    ID_CURSOR, ID_BLINK, ID_BELL, ID_BOLD, ID_META, ID_COPY, ID_WHEEL, ID_REFLOW, ID_COMPLETE, ID_KCMODE, ID_KCINFO, ID_KCCACHE,
    ID_FG, ID_BG, ID_SELFG, ID_SELBG, ID_PAL,           /* ID_PAL + 0..15 */
    ID_SAVE = ID_PAL + 16, ID_USE, ID_CANCEL, ID_STATUS, ID_PALTEXT, ID_THEME
};

/* A string field: its gadget, the page it is on, the text it edits. */
struct strfield {
    struct Gadget *g;
    int page;
    char *val;
    int cap;
};

/* the string fields: General's profile, font, fallback font, scrollback, cursor colour
 * and KingCON style; Colors' text, background, selection pair and the 16
 * palette entries */
#define N_STR (6 + 4 + 16)

struct app {
    struct Window *win;
    APTR vi;                  /* GadTools' VisualInfo of the screen */
    WORD ox, oy;              /* the inner top left, in window coordinates */
    struct Gadget *glist_common; /* Page, status, Save / Use / Cancel */
    struct Gadget *glist[2];  /* the General and the Colors page */
    int page;                 /* the page in the window, -1 none yet */
    struct Gadget *gstatus, *gcursor, *gblink, *gbell, *gbold, *gmeta, *gcopy, *gwheel, *greflow, *gcomplete, *gkcinfo, *gkccache;
    struct strfield str[N_STR];
    int nstr;
    upconf conf;              /* the file's table, as loaded and as last written */
    upconf work;              /* a save is staged here, then copied to conf */
    char buf[CONF_MAX + 2];   /* the file text, read or about to be written */
    int writable;             /* the file was loaded whole: it may be written back */
    char prof[UC_NAME];       /* the profile the fields show */
    prefs_fields f;
    char status[2][STATUS_MAX]; /* GadTools keeps the pointer: alternate */
    int st;
};

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT };

static STRPTR page_labels[] = { (STRPTR)"General", (STRPTR)"Colors", 0 };
static STRPTR cursor_labels[] = { (STRPTR)"Block", (STRPTR)"Underline", (STRPTR)"Bar", 0 };
static STRPTR bell_labels[] = { (STRPTR)"None", (STRPTR)"Beep", (STRPTR)"Visual", 0 };
static STRPTR meta_labels[] = { (STRPTR)"Left Amiga", (STRPTR)"Alt", 0 };
static STRPTR complete_labels[] = { (STRPTR)"Unix", (STRPTR)"KingCON", 0 };

/* The window a gadget of this page is in now (0: held off the window). */
static struct Window *win_of(struct app *a, int page)
{
    return page < 0 || page == a->page ? a->win : 0;
}

static void set_attr(struct app *a, struct Gadget *g, int page, ULONG tag, ULONG data)
{
    struct TagItem t[2];
    t[0].ti_Tag = tag;
    t[0].ti_Data = data;
    t[1].ti_Tag = TAG_DONE;
    t[1].ti_Data = 0;
    if (g)
        GT_SetGadgetAttrsA(g, win_of(a, page), 0, t);
}

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

static void set_status(struct app *a, const char *s)
{
    char *d;
    a->st ^= 1;
    d = a->status[a->st];
    puts_(d, 0, s, -1);
    set_attr(a, a->gstatus, -1, GTTX_Text, (ULONG)d);
}

/* ---- the fields <-> the gadgets ------------------------------------------------ */

static void add_str(struct app *a, struct Gadget *g, int page, char *val, int cap)
{
    struct strfield *s;
    if (a->nstr >= N_STR)
        return; /* never past the table (a 25th field once overwrote nstr) */
    s = &a->str[a->nstr++];
    s->g = g;
    s->page = page;
    s->val = val;
    s->cap = cap;
}

/* Show the fields in the gadgets. */
static void show_fields(struct app *a)
{
    int i;
    for (i = 0; i < a->nstr; i++)
        set_attr(a, a->str[i].g, a->str[i].page, GTST_String, (ULONG)a->str[i].val);
    set_attr(a, a->gcursor, 0, GTCY_Active, (ULONG)a->f.cursor);
    set_attr(a, a->gbell, 0, GTCY_Active, (ULONG)a->f.bell);
    set_attr(a, a->gmeta, 0, GTCY_Active, (ULONG)a->f.meta_alt);
    set_attr(a, a->gblink, 0, GTCB_Checked, (ULONG)a->f.blink);
    set_attr(a, a->gbold, 0, GTCB_Checked, (ULONG)a->f.bold);
    set_attr(a, a->gcopy, 0, GTCB_Checked, (ULONG)a->f.copy_sel);
    set_attr(a, a->gwheel, 0, GTCB_Checked, (ULONG)a->f.wheel);
    set_attr(a, a->greflow, 0, GTCB_Checked, (ULONG)a->f.reflow);
    set_attr(a, a->gcomplete, 0, GTCY_Active, (ULONG)a->f.completion);
    set_attr(a, a->gkcinfo, 0, GTCB_Checked, (ULONG)a->f.kcinfo);
    set_attr(a, a->gkccache, 0, GTCB_Checked, (ULONG)a->f.kccache);
}

/* Take the text of every string field: a string gadget reports only Return
 * and Tab, so what was typed without either is read here, before any
 * action that uses the fields. GadTools lets a program read the buffer. */
static void collect(struct app *a)
{
    int i, k;
    for (i = 0; i < a->nstr; i++) {
        const char *s = (const char *)((struct StringInfo *)a->str[i].g->SpecialInfo)->Buffer;
        for (k = 0; s[k] && k < a->str[i].cap - 1; k++)
            a->str[i].val[k] = s[k];
        a->str[i].val[k] = 0;
    }
}

/* ---- the pages --------------------------------------------------------------------- */

static void show_page(struct app *a, int page)
{
    if (a->page == page)
        return;
    if (a->page >= 0)
        RemoveGList(a->win, a->glist[a->page], -1);
    /* the old page's gadgets and labels go with the area they were in */
    EraseRect(a->win->RPort, a->ox - 8, a->oy + ROW(0) - 2,
              a->ox + AREA_W + 7, a->oy + ROW(PAGE_ROWS) - 1);
    a->page = page;
    AddGList(a->win, a->glist[page], (UWORD)~0, -1, 0);
    RefreshGList(a->glist[page], a->win, 0, -1);
    GT_RefreshWindow(a->win, 0);
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

/* Use (keep = 0): ENV: only. Save (keep = 1): ENVARC:, then ENV:. */
static void commit(struct app *a, int keep)
{
    char name[UC_NAME];
    char msg[STATUS_MAX];
    long len;
    int bad, t, r;
    if (!a->writable) {
        set_status(a, "Read only: the file was not loaded whole.");
        return;
    }
    collect(a);
    prefs_clean_name(a->prof, name, sizeof(name));
    if (!name[0]) {
        set_status(a, "Name the profile first.");
        return;
    }
    bad = prefs_validate(&a->f);
    if (bad) {
        int j;
        if (bad == PREFS_BAD_SELFG)
            j = puts_(msg, 0, "Selected text", -1);
        else if (bad == PREFS_BAD_SELBG)
            j = puts_(msg, 0, "Selection", -1);
        else
            j = puts_(msg, 0, "Palette ", bad - PREFS_BAD_PAL);
        puts_(msg, j, ": six hex digits, or blank.", -1);
        set_status(a, msg);
        return;
    }
    len = prefs_stage(&a->work, &a->conf, name, &a->f, a->buf, CONF_MAX + 1);
    if (len == PREFS_STAGE_FULL) {
        /* a profile more than the table holds, or a key more than a
         * profile does (the editor's keys beside the file's own) */
        puts_(msg, puts_(msg, puts_(msg, 0, "No room: at most ", UC_MAX_PROFILES),
                         " profiles of ", UC_MAX_KEYS), " settings.", -1);
        set_status(a, msg);
        return;
    }
    if (len < 0) {
        puts_(msg, puts_(msg, 0, "The file would be over ", CONF_MAX), " bytes.", -1);
        set_status(a, msg);
        return;
    }
    r = prefs_dos_save(a->buf, len, keep, &t);
    if (r == PREFS_DOS_NODIR) {
        puts_(msg, puts_(msg, 0, "Cannot create ", -1), conf_dir[t], -1);
        set_status(a, msg);
        return;
    }
    if (r != PREFS_INSTALL_OK) {
        install_error(a, t, r);
        return;
    }
    /* both files are in place: the editor's table is what they hold */
    CopyMem(&a->work, &a->conf, sizeof(a->conf));
    strcpy(a->prof, name);
    show_fields(a);
    set_status(a, keep ? "Saved. New windows use it." : "In use until reboot. New windows use it.");
}

/* ---- the window -------------------------------------------------------------------- */

static struct Gadget *gad(struct app *a, struct Gadget *prev, ULONG kind,
                          int x, int y, int w, int h, const char *label, UWORD id,
                          ULONG place, struct TagItem *tags)
{
    struct NewGadget ng;
    ng.ng_LeftEdge = (WORD)(a->ox + x);
    ng.ng_TopEdge = (WORD)(a->oy + y);
    ng.ng_Width = (WORD)w;
    ng.ng_Height = (WORD)h;
    ng.ng_GadgetText = (STRPTR)label;
    ng.ng_TextAttr = &topaz8;
    ng.ng_GadgetID = id;
    ng.ng_Flags = place;
    ng.ng_VisualInfo = a->vi;
    ng.ng_UserData = 0;
    return CreateGadgetA(kind, prev, &ng, tags);
}

static struct Gadget *str_gad(struct app *a, struct Gadget *prev, int page, int x, int y,
                              int w, const char *label, UWORD id, char *val, int cap)
{
    struct TagItem t[2];
    struct Gadget *g;
    t[0].ti_Tag = GTST_MaxChars;
    t[0].ti_Data = (ULONG)(cap - 1);
    t[1].ti_Tag = TAG_DONE;
    t[1].ti_Data = 0;
    g = gad(a, prev, STRING_KIND, x, y, w, 14, label, id, PLACETEXT_LEFT, t);
    if (g)
        add_str(a, g, page, val, cap);
    return g;
}

static struct Gadget *cycle_gad(struct app *a, struct Gadget *prev, int y, const char *label,
                                UWORD id, STRPTR *labels)
{
    struct TagItem t[2];
    t[0].ti_Tag = GTCY_Labels;
    t[0].ti_Data = (ULONG)labels;
    t[1].ti_Tag = TAG_DONE;
    t[1].ti_Data = 0;
    return gad(a, prev, CYCLE_KIND, FIELD_X, y, 140, 14, label, id, PLACETEXT_LEFT, t);
}

static struct Gadget *check_gad(struct app *a, struct Gadget *prev, int y, const char *label, UWORD id)
{
    return gad(a, prev, CHECKBOX_KIND, FIELD_X, y, 26, 11, label, id, PLACETEXT_LEFT, 0);
}

/* The three lists. 0 when GadTools could not make one (out of memory). */
static int build_gadgets(struct app *a)
{
    static const char *pal_names[16] = {
        "0", "1", "2", "3", "4", "5", "6", "7",
        "8", "9", "10", "11", "12", "13", "14", "15"
    };
    struct Gadget *g;
    struct TagItem t[3];
    int i;

    /* common: the page, the status line, Save / Use / Cancel */
    g = CreateContext(&a->glist_common);
    t[0].ti_Tag = GTCY_Labels;
    t[0].ti_Data = (ULONG)page_labels;
    t[1].ti_Tag = TAG_DONE;
    g = gad(a, g, CYCLE_KIND, 100, 0, 140, 14, "Page", ID_PAGE, PLACETEXT_LEFT, t);
    t[0].ti_Tag = GTTX_Border;
    t[0].ti_Data = TRUE;
    t[1].ti_Tag = TAG_DONE;
    g = a->gstatus = gad(a, g, TEXT_KIND, 0, STATUS_Y, AREA_W, 14, 0, ID_STATUS, 0, t);
    g = gad(a, g, BUTTON_KIND, 0, BUTTON_Y, 80, 14, "Save", ID_SAVE, PLACETEXT_IN, 0);
    g = gad(a, g, BUTTON_KIND, (AREA_W - 80) / 2, BUTTON_Y, 80, 14, "Use", ID_USE, PLACETEXT_IN, 0);
    g = gad(a, g, BUTTON_KIND, AREA_W - 80, BUTTON_Y, 80, 14, "Cancel", ID_CANCEL, PLACETEXT_IN, 0);
    if (!g)
        return 0;

    /* General */
    g = CreateContext(&a->glist[0]);
    g = str_gad(a, g, 0, FIELD_X, ROW(0), 150, "Profile", ID_PROF, a->prof, UC_NAME);
    g = gad(a, g, BUTTON_KIND, 282, ROW(0), 56, 14, "Load", ID_LOAD, PLACETEXT_IN, 0);
    g = gad(a, g, BUTTON_KIND, 342, ROW(0), 56, 14, "New", ID_NEW, PLACETEXT_IN, 0);
    g = gad(a, g, BUTTON_KIND, 402, ROW(0), 64, 14, "Delete", ID_DEL, PLACETEXT_IN, 0);
    g = str_gad(a, g, 0, FIELD_X, ROW(1), 200, "Font", ID_FONT, a->f.font, UC_MAX_VALUE);
    g = str_gad(a, g, 0, FIELD_X, ROW(2), 80, "Scrollback", ID_SB, a->f.sb, UC_MAX_VALUE);
    /* glyphs the font lacks (Nerd Font icons, CJK): an installed outline
     * font's, FONTS:<name>.otag (plan 2026-10-03-outline-fonts.md) */
    g = str_gad(a, g, 0, 336, ROW(2), 130, "Fallback font", ID_FALLBACK, a->f.fallback, UC_MAX_VALUE);
    g = str_gad(a, g, 0, FIELD_X, ROW(3), 80, "Cursor colour", ID_CURCOL, a->f.curcol, UC_MAX_VALUE);
    g = a->gcursor = cycle_gad(a, g, ROW(4), "Cursor", ID_CURSOR, cursor_labels);
    g = a->gblink = check_gad(a, g, ROW(5) + 1, "Cursor blinks", ID_BLINK);
    g = a->gbell = cycle_gad(a, g, ROW(6), "Bell", ID_BELL, bell_labels);
    g = a->gbold = check_gad(a, g, ROW(7) + 1, "Bold is bright", ID_BOLD);
    g = a->gmeta = cycle_gad(a, g, ROW(8), "Meta key", ID_META, meta_labels);
    g = a->gcopy = check_gad(a, g, ROW(9) + 1, "Copy on select", ID_COPY);
    g = a->gwheel = check_gad(a, g, ROW(10) + 1, "Wheel scrolls", ID_WHEEL);
    {
        /* beside the check boxes: the window is as tall as a PAL Workbench allows */
        struct TagItem t[2];
        t[0].ti_Tag = GTCY_Labels;
        t[0].ti_Data = (ULONG)complete_labels;
        t[1].ti_Tag = TAG_DONE;
        t[1].ti_Data = 0;
        g = a->gcomplete = gad(a, g, CYCLE_KIND, 336, ROW(9), 120, 14, "Tab completion",
                               ID_COMPLETE, PLACETEXT_LEFT, t);
    }
    /* KingCON's own switches: its FNCMODE letters (W window, L list, B
     * cycle, C common part first, S silent; blank is W) and .info files */
    g = str_gad(a, g, 0, 336, ROW(10), 120, "KingCON style", ID_KCMODE, a->f.kcmode, UC_MAX_VALUE);
    g = a->gkcinfo = gad(a, g, CHECKBOX_KIND, 430, ROW(8) + 1, 26, 11, ".info in lists", ID_KCINFO,
                         PLACETEXT_LEFT, 0);
    g = a->gkccache = gad(a, g, CHECKBOX_KIND, 430, ROW(7) + 1, 26, 11, "Directory cache",
                          ID_KCCACHE, PLACETEXT_LEFT, 0);
    /* a resize re-wraps the lines and the scrollback (gaps #11) */
    g = a->greflow = gad(a, g, CHECKBOX_KIND, 430, ROW(5) + 1, 26, 11, "Reflow on resize",
                         ID_REFLOW, PLACETEXT_LEFT, 0);
    if (!g)
        return 0;

    /* Colors */
    g = CreateContext(&a->glist[1]);
    g = str_gad(a, g, 1, FIELD_X, ROW(0), 80, "Text colour", ID_FG, a->f.fg, UC_MAX_VALUE);
    g = str_gad(a, g, 1, FIELD_X, ROW(1), 80, "Background", ID_BG, a->f.bg, UC_MAX_VALUE);
    t[0].ti_Tag = GTTX_Text;
    t[0].ti_Data = (ULONG)"Palette, RRGGBB (blank: built-in)";
    t[1].ti_Tag = TAG_DONE;
    g = gad(a, g, TEXT_KIND, 0, ROW(2), AREA_W, 14, 0, ID_PALTEXT, 0, t);
    for (i = 0; i < 16; i++)
        g = str_gad(a, g, 1, 32 + (i % 4) * 112, ROW(3 + i / 4), 80, pal_names[i],
                    (UWORD)(ID_PAL + i), a->f.pal[i], sizeof(a->f.pal[i]));
    g = str_gad(a, g, 1, FIELD_X, ROW(7), 80, "Selected text", ID_SELFG, a->f.selfg, UC_MAX_VALUE);
    g = str_gad(a, g, 1, FIELD_X, ROW(8), 80, "Selection", ID_SELBG, a->f.selbg, UC_MAX_VALUE);
    g = gad(a, g, BUTTON_KIND, FIELD_X, ROW(9), 120, 14, "Theme...", ID_THEME, PLACETEXT_IN, 0);
    return g != 0;
}

static struct Window *open_window(struct app *a, struct Screen *scr)
{
    struct TagItem tags[12];
    int n = 0;
    LONG left = (scr->Width - INNER_W) / 2, top = (scr->Height - INNER_H) / 2;
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    tags[n].ti_Tag = WA_Left;        tags[n++].ti_Data = (ULONG)left;
    tags[n].ti_Tag = WA_Top;         tags[n++].ti_Data = (ULONG)top;
    tags[n].ti_Tag = WA_InnerWidth;  tags[n++].ti_Data = INNER_W;
    tags[n].ti_Tag = WA_InnerHeight; tags[n++].ti_Data = INNER_H;
    tags[n].ti_Tag = WA_AutoAdjust;  tags[n++].ti_Data = TRUE;
    tags[n].ti_Tag = WA_IDCMP;       tags[n++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                                     BUTTONIDCMP | STRINGIDCMP | CYCLEIDCMP |
                                                     CHECKBOXIDCMP;
    tags[n].ti_Tag = WA_Flags;       tags[n++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                                     WFLG_CLOSEGADGET | WFLG_ACTIVATE |
                                                     WFLG_SMART_REFRESH;
    tags[n].ti_Tag = WA_Title;       tags[n++].ti_Data = (ULONG)"UP-Term Prefs";
    tags[n].ti_Tag = WA_PubScreen;   tags[n++].ti_Data = (ULONG)scr;
    tags[n].ti_Tag = WA_Gadgets;     tags[n++].ti_Data = (ULONG)a->glist_common;
    tags[n].ti_Tag = TAG_DONE;       tags[n].ti_Data = 0;
    return OpenWindowTagList(0, tags);
}

/* A gadget was released. 1 when the editor is done. */
/* Theme...: a theme file from the kit's drawer, its colours into the
 * profile being edited (prefs_apply_theme); Save or Use writes them. */
static void pick_theme(struct app *a)
{
    struct FileRequester *fr;
    char path[300];
    BPTR f;
    long got;
    if (!AslBase) {
        set_status(a, "No asl.library: no theme requester.");
        return;
    }
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
            ASLFR_Window, (ULONG)a->win, ASLFR_TitleText, (ULONG)"Choose a theme",
            ASLFR_InitialDrawer, (ULONG)"ENVARC:up-term/themes", ASLFR_InitialPattern, (ULONG)"#?.conf",
            ASLFR_DoPatterns, TRUE, ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (!fr)
        return;
    if (!AslRequest(fr, 0) || !fr->fr_File[0]) {
        FreeAslRequest(fr);
        return;
    }
    strncpy(path, (const char *)fr->fr_Drawer, sizeof(path) - 1);
    path[sizeof(path) - 1] = 0;
    AddPart((STRPTR)path, (STRPTR)fr->fr_File, sizeof(path));
    FreeAslRequest(fr);
    f = Open((STRPTR)path, MODE_OLDFILE);
    if (!f) {
        set_status(a, "The theme file cannot be opened.");
        return;
    }
    got = Read(f, a->buf, CONF_MAX); /* the file text is parsed already: buf is free */
    Close(f);
    a->buf[got > 0 ? got : 0] = 0;
    if (got > 0 && prefs_apply_theme(&a->f, &a->work, a->buf, got)) {
        show_fields(a);
        set_status(a, "Theme in the fields: Save or Use writes it.");
    } else
        set_status(a, "That file holds no colours.");
}

static int gadget_up(struct app *a, struct Gadget *g, UWORD code)
{
    char name[UC_NAME];
    switch (g->GadgetID) {
    case ID_PAGE:
        show_page(a, code ? 1 : 0);
        break;
    case ID_LOAD:
        collect(a);
        prefs_clean_name(a->prof, name, sizeof(name));
        if (!name[0])
            break;
        if (prefs_profile_exists(&a->conf, name)) {
            strcpy(a->prof, name);
            prefs_from_conf(&a->f, &a->conf, name);
            show_fields(a);
            set_status(a, "Loaded.");
        } else
            set_status(a, "Not in the file: New starts it, Save writes it.");
        break;
    case ID_NEW:
        collect(a); /* keeps the typed name */
        prefs_defaults(&a->f);
        show_fields(a);
        set_status(a, "New profile: Save writes it.");
        break;
    case ID_DEL:
        collect(a);
        prefs_clean_name(a->prof, name, sizeof(name));
        if (name[0] && upconf_rmprof(&a->conf, name)) {
            prefs_defaults(&a->f);
            show_fields(a);
            set_status(a, "Deleted: Save or Use writes the file.");
        } else
            set_status(a, "Not in the file.");
        break;
    case ID_THEME:
        collect(a); /* what was typed stays, the theme replaces only colours */
        pick_theme(a);
        break;
    case ID_CURSOR:
        a->f.cursor = code;
        break;
    case ID_BELL:
        a->f.bell = code;
        break;
    case ID_META:
        a->f.meta_alt = code ? 1 : 0;
        break;
    case ID_COMPLETE:
        a->f.completion = code ? PREFS_COMPLETE_KINGCON : PREFS_COMPLETE_UNIX;
        break;
    case ID_BLINK:
        a->f.blink = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_BOLD:
        a->f.bold = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_KCCACHE:
        a->f.kccache = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_KCINFO:
        a->f.kcinfo = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_COPY:
        a->f.copy_sel = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_WHEEL:
        a->f.wheel = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_REFLOW:
        a->f.reflow = (g->Flags & GFLG_SELECTED) ? 1 : 0;
        break;
    case ID_SAVE:
        commit(a, 1);
        break;
    case ID_USE:
        commit(a, 0);
        break;
    case ID_CANCEL:
        return 1;
    default:
        break; /* a string field: read by collect() when it is used */
    }
    return 0;
}

int main(void)
{
    struct app *a = 0;
    struct Screen *scr = 0;
    struct IntuiMessage *im;
    ULONG sig;
    int done = 0, rc = 1;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39L);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39L);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39L);
    AslBase = OpenLibrary((STRPTR)"asl.library", 38L);
    if (!IntuitionBase || !GfxBase || !GadToolsBase)
        goto out;

    a = (struct app *)AllocVec(sizeof(*a), MEMF_ANY | MEMF_CLEAR);
    if (!a)
        goto out;
    a->page = -1;

    scr = LockPubScreen(0);
    if (!scr)
        goto out;
    a->vi = GetVisualInfoA(scr, 0);
    if (!a->vi)
        goto out;
    a->ox = (WORD)(scr->WBorLeft + 8);
    a->oy = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1 + 4);
    if (!build_gadgets(a))
        goto out;

    a->win = open_window(a, scr);
    UnlockPubScreen(0, scr);
    scr = 0;
    if (!a->win)
        goto out;
    GT_RefreshWindow(a->win, 0);
    show_page(a, 0);

    load_file(a);
    /* the profile to edit first: "default", else the file's first, else "default" */
    if (!prefs_profile_exists(&a->conf, "default") && a->conf.nprof > 0)
        strcpy(a->prof, a->conf.prof[0]);
    else
        strcpy(a->prof, "default");
    prefs_from_conf(&a->f, &a->conf, a->prof);
    show_fields(a);

    sig = 1UL << a->win->UserPort->mp_SigBit;
    while (!done) {
        Wait(sig);
        while (!done && (im = GT_GetIMsg(a->win->UserPort))) {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            struct Gadget *g = (struct Gadget *)im->IAddress;
            GT_ReplyIMsg(im);
            if (cls == IDCMP_CLOSEWINDOW)
                done = 1;
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(a->win);
                GT_EndRefresh(a->win, TRUE);
            } else if (cls == IDCMP_GADGETUP && g)
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
        FreeGadgets(a->glist[1]);
        FreeGadgets(a->glist[0]);
        FreeGadgets(a->glist_common);
        if (a->vi)
            FreeVisualInfo(a->vi);
        FreeVec(a);
    }
    if (scr)
        UnlockPubScreen(0, scr);
    if (GadToolsBase)
        CloseLibrary(GadToolsBase);
    if (AslBase)
        CloseLibrary(AslBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
