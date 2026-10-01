/* UP-Term Prefs: the settings editor for /ENV/up-term/up-term -- the
 * profiles the XCON: handler reads for every window (plan
 * thoughts/shared/plans/2026-10-01-terminal-preferences.md).
 *
 * C:UP-Term Prefs opens a two-page window, General and Colors. The fields
 * edit one profile at a time: name it in the profile field, Load it (or
 * start it with New), change the values, Save. The previous file is kept
 * as up-term.orig. New XCON: windows use the file; open windows keep
 * their own settings (live apply is a later phase).
 *
 * One Intuition window, two pages of gadgets: the active page's tab is
 * grey (you are here) and the other page's gadgets are disabled. Every
 * control does one thing when clicked; the "toggle" buttons (cursor,
 * bell, ...) cycle their value and their label, so no gadget state lives
 * outside this program.
 */
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include "../config/upconf.h"

/* The clib's Intuition calls read IntuitionBase; startup.o provides the
 * DOSBase one, so only this one is ours to define. */
struct IntuitionBase *IntuitionBase;

/* V39's headers renamed the classic gadget types; the values are the ones
 * every Intuition has always used: 2 is the string gadget (it edits its
 * GadgetText when clicked and typed into), 3 the plain label. */
#define PREFS_STRING GTYP_GADGET0002
#define PREFS_LABEL  GTYP_PROPGADGET

#define CONF_PATH  "/ENV/up-term/up-term"
#define CONF_ORIG  "/ENV/up-term/up-term.orig"
#define CONF_TMP   "/ENV/up-term/up-term.new"
#define CONF_DIR   "/ENV/up-term"
/* the same cap the handler reads with, so a file this writes is always a
 * file the handler can read whole */
#define CONF_MAX   UC_MAX_FILE
#define LOCK_DIRONLY 4       /* Lock(): the path is a directory, not a file */

#define WIN_W 470
#define WIN_H 306
#define STATUS_MAX 128

/* The gadget IDs. The palette fields are ID_PAL + i. */
enum {
    ID_TABGEN, ID_TABCOL,
    ID_PROFN, ID_LOAD, ID_NEW, ID_DEL,
    ID_FONT, ID_SB, ID_CURCOL,
    ID_CURSTYLE, ID_BLINK, ID_BELL, ID_BOLD, ID_META, ID_COPY, ID_WHEEL,
    ID_FG, ID_BG, ID_PAL,
    ID_SAVE, ID_CANCEL, ID_STATUS
};

struct app {
    struct MsgPort *port;
    struct Window *win;
    upconf conf;              /* the file's table, edited in place */
    /* the profile the fields show now; every field holds a value of upconf's
     * own size, so a hand-edited file cannot overflow one */
    char prof[UC_NAME];
    char font[UC_MAX_VALUE];
    char sb[UC_MAX_VALUE];
    char curcol[UC_MAX_VALUE];
    char fg[UC_MAX_VALUE];
    char bg[UC_MAX_VALUE];
    char pal[16][16];
    char status[128];
    /* the cycle buttons' labels */
    char lb_curstyle[40];
    char lb_blink[40];
    char lb_bell[40];
    char lb_bold[40];
    char lb_meta[40];
    char lb_copy[40];
    char lb_wheel[40];
    int cursor_style;         /* 1 block, 3 underline, 5 bar */
    int blink, bell, bold, meta_alt, copy_sel, wheel;
    int page;                 /* 0 General, 1 Colors */
    /* the gadgets, in window order: the tabs, the General controls and
     * labels, the Colors controls and labels, then Save / Cancel / status */
    struct Gadget gtab_gen, gtab_col;
    struct Gadget gprof, gload, gnew, gdel;
    struct Gadget gfont, gsb, gcurcol;
    struct Gadget gcurstyle, gblink, gbell, gbold, gmeta, gcopy, gwheel;
    struct Gadget glab_gen[4];
    struct Gadget gfg, gbg, gpal[16];
    struct Gadget glab_col[18];
    struct Gadget gsave, gcancel, gstatus;
};

/* Repaint the window. The gadget list is static and changes only in the
 * text and the disabled flags, so a full refresh is the whole job. */
static void redraw(struct app *a)
{
    if (a->win)
        RefreshGadgets(0, a->win, 0);
}

/* A window gadget: label a string, reported on the release still over it. */
static void mk(struct Gadget *g, struct Gadget *next, int x, int y, int w, int h,
               UWORD type, char *label, UWORD id)
{
    g->NextGadget = next;
    g->LeftEdge = (WORD)x;
    g->TopEdge = (WORD)y;
    g->Width = (WORD)w;
    g->Height = (WORD)h;
    g->Flags = 0;
    g->Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
    g->GadgetType = type;
    g->GadgetRender = 0;
    g->SelectRender = 0;
    g->GadgetText = (struct IntuiText *)label;
    g->MutualExclude = 0;
    g->SpecialInfo = 0;
    g->GadgetID = id;
    g->UserData = 0;
}

static void set_status(struct app *a, const char *s)
{
    int i = 0;
    while (*s && i < (int)sizeof(a->status) - 1)
        a->status[i++] = *s++;
    a->status[i] = 0;
    redraw(a);
}

/* Append to a status line: a string, then (when num >= 0) its decimal.
 * Leaves the line terminated; returns the length. */
static int puts_(char *dst, int at, const char *s, int num)
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

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* A palette field is exactly six hex digits, 0x or # optional. */
/* "000000" form of a colour, for the palette fields. */
static void rgb_chars(uc_u32 rgb, char *out, int cap)
{
    int i;
    if (cap < 7)
        return;
    for (i = 5; i >= 0; i--) {
        out[i] = "0123456789ABCDEF"[rgb & 0xF];
        rgb >>= 4;
    }
    out[6] = 0;
}

/* ---- the editor <-> the table ------------------------------------------------ */

static void refresh_labels(struct app *a)
{
    static const char *cursor_names[3] = { "Cursor: block", "Cursor: underline", "Cursor: bar" };
    static const char *bell_names[3] = { "Bell: none", "Bell: beep", "Bell: visual" };
    int cs = a->cursor_style == 3 ? 1 : a->cursor_style == 5 ? 2 : 0;
    int bell = a->bell < 0 ? 0 : a->bell > 2 ? 2 : a->bell;
    strcpy(a->lb_curstyle, cursor_names[cs]);
    strcpy(a->lb_blink, a->blink ? "Cursor blink: on" : "Cursor blink: off");
    strcpy(a->lb_bell, bell_names[bell]);
    strcpy(a->lb_bold, a->bold ? "Bold bright: on" : "Bold bright: off");
    strcpy(a->lb_meta, a->meta_alt ? "Meta: Alt" : "Meta: Left Amiga");
    strcpy(a->lb_copy, a->copy_sel ? "Copy on select: on" : "Copy on select: off");
    strcpy(a->lb_wheel, a->wheel ? "Wheel: scroll" : "Wheel: off");
}

/* The built-in values, into the fields. */
static void defaults(struct app *a)
{
    a->cursor_style = 1;
    a->blink = 0;
    a->bell = 1;
    a->bold = 1;
    a->meta_alt = 0;
    a->copy_sel = 0;
    a->wheel = 1;
    a->font[0] = 0;
    a->sb[0] = 0;
    a->curcol[0] = 0;
    a->fg[0] = 0;
    a->bg[0] = 0;
    memset(a->pal, 0, sizeof(a->pal));
    refresh_labels(a);
}

/* Put the profile's values into the fields (an absent key takes its
 * default, as the handler's built-ins would). */
static void load_profile(struct app *a, const char *name)
{
    const char *v;
    uc_u32 pal[16];
    int i;
    strcpy(a->font, upconf_str(&a->conf, name, "font", ""));
    strcpy(a->sb, upconf_str(&a->conf, name, "scrollback", ""));
    strcpy(a->curcol, upconf_str(&a->conf, name, "cursor-color", ""));
    strcpy(a->fg, upconf_str(&a->conf, name, "fg", ""));
    strcpy(a->bg, upconf_str(&a->conf, name, "bg", ""));
    v = upconf_str(&a->conf, name, "cursor", "block");
    a->cursor_style = ieq(v, "underline") ? 3 : ieq(v, "bar") ? 5 : 1;
    a->blink = ieq(upconf_str(&a->conf, name, "cursor-blink", "off"), "on");
    v = upconf_str(&a->conf, name, "bell", "beep");
    a->bell = ieq(v, "none") ? 0 : ieq(v, "visual") ? 2 : 1;
    a->bold = ieq(upconf_str(&a->conf, name, "bold-bright", "on"), "on");
    a->meta_alt = ieq(upconf_str(&a->conf, name, "meta", "amiga"), "alt");
    a->copy_sel = ieq(upconf_str(&a->conf, name, "copy-on-select", "off"), "on");
    a->wheel = !ieq(upconf_str(&a->conf, name, "wheel", "scroll"), "ignore");
    upconf_palette_parse(upconf_get(&a->conf, name, "palette"), pal);
    for (i = 0; i < 16; i++)
        if (pal[i] & 0x01000000UL)
            rgb_chars(pal[i] & 0xFFFFFFUL, a->pal[i], sizeof(a->pal[i]));
        else
            a->pal[i][0] = 0;
    refresh_labels(a);
    redraw(a);
}

static void set_page(struct app *a, int page)
{
    struct Gadget *g;
    int i;
    a->page = page;
    g = &a->gprof;
    for (i = 0; g && i < 18; i++) { /* the General controls and labels */
        if (page)
            g->Flags |= GFLG_DISABLED;
        else
            g->Flags &= ~(UWORD)GFLG_DISABLED;
        g = g->NextGadget;
    }
    g = &a->gfg;
    for (i = 0; g && i < 36; i++) { /* the Colors controls and labels */
        if (!page)
            g->Flags |= GFLG_DISABLED;
        else
            g->Flags &= ~(UWORD)GFLG_DISABLED;
        g = g->NextGadget;
    }
    if (page)
        a->gtab_gen.Flags |= GFLG_DISABLED;
    else
        a->gtab_gen.Flags &= ~(UWORD)GFLG_DISABLED;
    if (!page)
        a->gtab_col.Flags |= GFLG_DISABLED;
    else
        a->gtab_col.Flags &= ~(UWORD)GFLG_DISABLED;
    redraw(a);
}

/* ---- the file ---------------------------------------------------------------- */

static int ensure_dir(void)
{
    BPTR l = CreateDir(CONF_DIR);
    if (l)
        UnLock(l);
    l = Lock(CONF_DIR, LOCK_DIRONLY);
    if (l) {
        UnLock(l);
        return 1;
    }
    return 0;
}

/* The current file becomes up-term.orig (its old backup goes). */
static void backup(void)
{
    BPTR src, dst;
    char *buf;
    LONG n;
    src = Open(CONF_PATH, MODE_OLDFILE);
    if (!src)
        return;
    n = Seek(src, 0, OFFSET_END);
    if (n > 0 && n <= CONF_MAX) {
        buf = (char *)AllocVec(n + 1, MEMF_ANY);
        if (buf) {
            Seek(src, 0, OFFSET_BEGINNING);
            n = Read(src, buf, n);
            if (n > 0) {
                DeleteFile(CONF_ORIG); /* a missing old backup is not an error */
                dst = Open(CONF_ORIG, MODE_NEWFILE);
                if (dst) {
                    Write(dst, buf, n);
                    Close(dst);
                }
            }
            FreeVec(buf);
        }
    }
    Close(src);
}

static void load_file(struct app *a)
{
    BPTR f;
    char *buf;
    LONG n;
    f = Open(CONF_PATH, MODE_OLDFILE);
    if (!f)
        return; /* no file: the table stays empty, the defaults stand */
    n = Seek(f, 0, OFFSET_END);
    if (n > 0 && n <= CONF_MAX) {
        buf = (char *)AllocVec(n + 1, MEMF_ANY);
        if (buf) {
            Seek(f, 0, OFFSET_BEGINNING);
            n = Read(f, buf, n);
            if (n > 0)
                upconf_parse(&a->conf, buf, n);
            FreeVec(buf);
        }
    }
    Close(f);
}

/* The profile name as typed: trimmed, its [ ] stripped, what fits UC_NAME. */
static void clean_name(struct app *a, char *out, int cap)
{
    char *p = a->prof, *o = out;
    while (*p == ' ' || *p == '\t')
        p++;
    while (*p) {
        if (*p != '[' && *p != ']' && (int)(o - out) < cap - 1)
            *o++ = *p;
        p++;
    }
    while (o > out && (o[-1] == ' ' || o[-1] == '\t'))
        o--;
    *o = 0;
}

static int profile_exists(struct app *a, const char *name)
{
    const char *names[UC_MAX_PROFILES + 1];
    int i, n = upconf_profiles(&a->conf, names);
    for (i = 0; i < n; i++)
        if (ieq(names[i], name))
            return 1;
    return 0;
}

static int save_all(struct app *a)
{
    upconf *c = &a->conf;
    char name[UC_NAME];
    char *buf;
    char palstr[UC_MAX_VALUE];
    uc_u32 pal[16];
    long len;
    int i;
    clean_name(a, name, sizeof(name));
    if (!name[0]) {
        set_status(a, "Name the profile first.");
        return 0;
    }
    /* Validate everything before the table is touched: a value the file
     * cannot hold must leave the loaded file exactly as it was. */
    memset(pal, 0, sizeof(pal));
    for (i = 0; i < 16; i++) {
        if (a->pal[i][0]) {
            uc_u32 rgb;
            if (!upconf_hex6(a->pal[i], &rgb)) {
                char msg[STATUS_MAX];
                int j = puts_(msg, 0, "Palette ", i);
                j = puts_(msg, j, ": six hex digits, or blank.", 0);
                set_status(a, msg);
                return 0;
            }
            pal[i] = 0x01000000UL | rgb;
        }
    }
    if (upconf_palette_str(pal, palstr, sizeof(palstr)) < 0) {
        /* a value slot is UC_MAX_VALUE long and the palette line is one
         * value; a full 16-entry grid is 159 bytes and fills it exactly, so reaching
         * this means the table grew shorter than the palette */
        char msg[STATUS_MAX];
        int j = 0;
        for (i = 0; i < 16; i++)
            if (pal[i] & 0x01000000UL)
                j++;
        j = puts_(msg, 0, "Palette: ", j);
        j = puts_(msg, j, " colours exceed ", UC_MAX_VALUE - 1);
        puts_(msg, j, " characters.", 0);
        set_status(a, msg);
        return 0;
    }
    buf = (char *)AllocVec(CONF_MAX + 1, MEMF_ANY);
    if (!buf) {
        set_status(a, "Out of memory.");
        return 0;
    }
    upconf_rmprof(c, name); /* re-enter its keys fresh */
    if (a->font[0])
        upconf_set(c, name, "font", a->font);
    if (a->sb[0])
        upconf_set(c, name, "scrollback", a->sb);
    if (a->curcol[0])
        upconf_set(c, name, "cursor-color", a->curcol);
    if (a->fg[0])
        upconf_set(c, name, "fg", a->fg);
    if (a->bg[0])
        upconf_set(c, name, "bg", a->bg);
    upconf_set(c, name, "cursor",
               a->cursor_style == 3 ? "underline" : a->cursor_style == 5 ? "bar" : "block");
    upconf_set(c, name, "cursor-blink", a->blink ? "on" : "off");
    upconf_set(c, name, "bell", a->bell == 0 ? "none" : a->bell == 2 ? "visual" : "beep");
    upconf_set(c, name, "bold-bright", a->bold ? "on" : "off");
    upconf_set(c, name, "meta", a->meta_alt ? "alt" : "amiga");
    upconf_set(c, name, "copy-on-select", a->copy_sel ? "on" : "off");
    upconf_set(c, name, "wheel", a->wheel ? "scroll" : "ignore");
    if (palstr[0])
        upconf_set(c, name, "palette", palstr);
    else
        upconf_del(c, name, "palette");
    len = upconf_save(c, buf, CONF_MAX);
    buf[CONF_MAX] = 0;
    if (len < 0) {
        FreeVec(buf);
        set_status(a, "The file would be too large.");
        return 0;
    }
    if (!ensure_dir()) {
        FreeVec(buf);
        set_status(a, "Cannot create ENVARC:up-term.");
        return 0;
    }
    backup();
    /* Write the whole file beside the target and rename it over: until the
     * rename the live file is untouched, so a short write or a full disk
     * cannot leave the user's profiles half-written. Rename cannot replace an
     * existing file, hence the delete right before it -- at that point the
     * new file is already complete on disk and the old one is in .orig. */
    DeleteFile(CONF_TMP);
    {
        BPTR f = Open(CONF_TMP, MODE_NEWFILE);
        LONG w;
        if (!f) {
            FreeVec(buf);
            set_status(a, "Cannot write ENVARC:up-term/up-term.");
            return 0;
        }
        w = Write(f, buf, len);
        Close(f);
        FreeVec(buf);
        if (w != len) {
            DeleteFile(CONF_TMP);
            set_status(a, "The write failed (your old file is untouched).");
            return 0;
        }
    }
    DeleteFile(CONF_PATH);
    if (Rename(CONF_TMP, CONF_PATH) != 0) {
        DeleteFile(CONF_TMP);
        set_status(a, "Cannot replace ENVARC:up-term/up-term (see up-term.orig).");
        return 0;
    }
    set_status(a, c->overflow ? "Saved (the file was cut short)." : "Saved. New windows use it.");
    /* the file holds the trimmed name: keep the field showing what was
     * written, so Load and Save agree from here on */
    strcpy(a->prof, name);
    return 1;
}

/* ---- the window --------------------------------------------------------------- */

static void build_gadgets(struct app *a)
{
    int i;
    static const char *gen_labels[4] = { "Profile", "Font", "Scrollback", "Cursor colour" };
    static const int gen_y[4] = { 30, 54, 78, 102 };
    static const char *col_labels[18] = {
        "Text colour", "Background",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"
    };
    static const int col_x[18] = {
        8, 8,
        8, 114, 220, 326, 8, 114, 220, 326, 8, 114, 220, 326, 8, 114, 220, 326
    };
    static const int col_y[18] = {
        30, 54,
        82, 82, 82, 82, 108, 108, 108, 108, 134, 134, 134, 134, 160, 160, 160, 160
    };
    mk(&a->gtab_gen, &a->gtab_col, 312, 6, 72, 14, BOOLGADGET, "General", ID_TABGEN);
    mk(&a->gtab_col, &a->gprof, 392, 6, 72, 14, BOOLGADGET, "Colors", ID_TABCOL);
    mk(&a->gprof, &a->gload, 64, 28, 100, 12, PREFS_STRING, a->prof, ID_PROFN);
    mk(&a->gload, &a->gnew, 170, 28, 40, 14, BOOLGADGET, "Load", ID_LOAD);
    mk(&a->gnew, &a->gdel, 214, 28, 40, 14, BOOLGADGET, "New", ID_NEW);
    mk(&a->gdel, &a->gfont, 258, 28, 44, 14, BOOLGADGET, "Delete", ID_DEL);
    mk(&a->gfont, &a->gsb, 64, 52, 150, 12, PREFS_STRING, a->font, ID_FONT);
    mk(&a->gsb, &a->gcurcol, 90, 76, 70, 12, PREFS_STRING, a->sb, ID_SB);
    mk(&a->gcurcol, &a->gcurstyle, 110, 100, 70, 12, PREFS_STRING, a->curcol, ID_CURCOL);
    mk(&a->gcurstyle, &a->gblink, 260, 52, 190, 14, BOOLGADGET, a->lb_curstyle, ID_CURSTYLE);
    mk(&a->gblink, &a->gbell, 260, 76, 190, 14, BOOLGADGET, a->lb_blink, ID_BLINK);
    mk(&a->gbell, &a->gbold, 260, 100, 190, 14, BOOLGADGET, a->lb_bell, ID_BELL);
    mk(&a->gbold, &a->gmeta, 260, 124, 190, 14, BOOLGADGET, a->lb_bold, ID_BOLD);
    mk(&a->gmeta, &a->gcopy, 260, 148, 190, 14, BOOLGADGET, a->lb_meta, ID_META);
    mk(&a->gcopy, &a->gwheel, 260, 172, 190, 14, BOOLGADGET, a->lb_copy, ID_COPY);
    mk(&a->gwheel, &a->glab_gen[0], 260, 196, 190, 14, BOOLGADGET, a->lb_wheel, ID_WHEEL);
    for (i = 0; i < 4; i++)
        mk(&a->glab_gen[i], i < 3 ? &a->glab_gen[i + 1] : &a->gfg,
           8, gen_y[i], 96, 8, PREFS_LABEL, (char *)gen_labels[i], 0);
    mk(&a->gfg, &a->gbg, 100, 28, 80, 12, PREFS_STRING, a->fg, ID_FG);
    mk(&a->gbg, &a->gpal[0], 100, 52, 80, 12, PREFS_STRING, a->bg, ID_BG);
    for (i = 0; i < 16; i++)
        mk(&a->gpal[i], i < 15 ? &a->gpal[i + 1] : &a->glab_col[0],
           30 + (i % 4) * 106, 82 + (i / 4) * 26, 74, 12, PREFS_STRING, a->pal[i],
           (ULONG)ID_PAL + i);
    for (i = 0; i < 18; i++)
        mk(&a->glab_col[i], i < 17 ? &a->glab_col[i + 1] : &a->gsave,
           col_x[i], col_y[i], 96, 8, PREFS_LABEL, (char *)col_labels[i], 0);
    mk(&a->gsave, &a->gcancel, 8, 284, 56, 16, BOOLGADGET, "Save", ID_SAVE);
    mk(&a->gcancel, &a->gstatus, 70, 284, 56, 16, BOOLGADGET, "Cancel", ID_CANCEL);
    mk(&a->gstatus, 0, 134, 286, 326, 12, PREFS_STRING, a->status, ID_STATUS);
    a->gstatus.Flags |= GFLG_DISABLED; /* the status line: text, no editing */
}

/* The window: two pages of gadgets in one static list, on a public screen
 * so it shows up wherever Workbench happens to be. */
static struct Window *open_window(struct app *a, struct Screen *scr,
                                  int left, int top)
{
    struct TagItem tags[10];
    int n = 0;
    tags[n].ti_Tag = WA_Left;       tags[n++].ti_Data = left;
    tags[n].ti_Tag = WA_Top;        tags[n++].ti_Data = top;
    tags[n].ti_Tag = WA_Width;      tags[n++].ti_Data = WIN_W;
    tags[n].ti_Tag = WA_Height;     tags[n++].ti_Data = WIN_H;
    tags[n].ti_Tag = WA_IDCMP;      tags[n++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_GADGETUP |
                                                   IDCMP_RAWKEY | IDCMP_ACTIVEWINDOW |
                                                   IDCMP_INACTIVEWINDOW;
    tags[n].ti_Tag = WA_Flags;      tags[n++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                                   WFLG_CLOSEGADGET | WFLG_ACTIVATE |
                                                   WFLG_SMART_REFRESH;
    tags[n].ti_Tag = WA_Title;      tags[n++].ti_Data = (ULONG)"UP-Term Prefs";
    tags[n].ti_Tag = WA_PubScreen;  tags[n++].ti_Data = (ULONG)scr;
    tags[n].ti_Tag = WA_Gadgets;    tags[n++].ti_Data = (ULONG)&a->gtab_gen;
    tags[n].ti_Tag = TAG_DONE;      tags[n].ti_Data = 0;
    return OpenWindowTagList(0, tags);
}

int main(void)
{
    struct app *a;
    struct IntuiMessage *im;
    struct Screen *scr;
    int done = 0;
    int left, top;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 35L);
    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 39L);
    if (!IntuitionBase || !DOSBase) {
        if (IntuitionBase)
            CloseLibrary((struct Library *)IntuitionBase);
        if (DOSBase)
            CloseLibrary((struct Library *)DOSBase);
        return 1;
    }

    a = (struct app *)AllocVec(sizeof(*a), MEMF_ANY | MEMF_CLEAR);
    if (!a) {
        CloseLibrary((struct Library *)DOSBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }

    load_file(a);
    defaults(a);

    /* the profile to edit first: "default", else the file's first, else "default" */
    if (profile_exists(a, "default"))
        strcpy(a->prof, "default");
    else {
        const char *names[UC_MAX_PROFILES + 1];
        if (upconf_profiles(&a->conf, names) > 0 && names[0])
            strcpy(a->prof, names[0]);
        else
            strcpy(a->prof, "default");
    }

    build_gadgets(a);

    a->port = CreateMsgPort();
    if (!a->port)
        goto fail;

    scr = LockPubScreen(0);
    if (!scr)
        goto fail;
    left = (scr->Width - WIN_W) / 2 + 40;
    top = (scr->Height - WIN_H) / 2;
    if (left < 8)
        left = 8;
    if (top < 8)
        top = 8;

    a->win = open_window(a, scr, left, top);
    UnlockPubScreen(0, scr);
    if (!a->win)
        goto fail;

    set_page(a, 0);
    load_profile(a, a->prof); /* fills the fields and shows them */

    for (;;) {
        im = (struct IntuiMessage *)GetMsg(a->port);
        if (!im)
            continue;
        if (im->Class & IDCMP_CLOSEWINDOW)
            done = 1;
        else if (im->Class & IDCMP_GADGETUP) {
            struct Gadget *g = (struct Gadget *)im->IAddress;
            if (g && g->GadgetType == BOOLGADGET) {
                switch (g->GadgetID) {
                case ID_TABGEN:
                    if (a->page != 0)
                        set_page(a, 0);
                    break;
                case ID_TABCOL:
                    if (a->page != 1)
                        set_page(a, 1);
                    break;
                case ID_LOAD: {
                    char name[UC_NAME];
                    clean_name(a, name, sizeof(name));
                    if (name[0]) {
                        if (profile_exists(a, name)) {
                            strcpy(a->prof, name);
                            load_profile(a, name);
                            set_status(a, "Loaded.");
                        } else
                            set_status(a, "Not in the file: New starts it, Save writes it.");
                    }
                    break;
                }
                case ID_NEW:
                    defaults(a);
                    redraw(a);
                    set_status(a, "New profile: Save writes it.");
                    break;
                case ID_DEL: {
                    char name[UC_NAME];
                    clean_name(a, name, sizeof(name));
                    if (name[0] && upconf_rmprof(&a->conf, name)) {
                        defaults(a);
                        redraw(a);
                        set_status(a, "Deleted.");
                    } else
                        set_status(a, "Not in the file.");
                    break;
                }
                case ID_CURSTYLE:
                    a->cursor_style = a->cursor_style == 1 ? 3 : a->cursor_style == 3 ? 5 : 1;
                    break;
                case ID_BLINK:
                    a->blink = !a->blink;
                    break;
                case ID_BELL:
                    a->bell = (a->bell + 1) % 3;
                    break;
                case ID_BOLD:
                    a->bold = !a->bold;
                    break;
                case ID_META:
                    a->meta_alt = !a->meta_alt;
                    break;
                case ID_COPY:
                    a->copy_sel = !a->copy_sel;
                    break;
                case ID_WHEEL:
                    a->wheel = !a->wheel;
                    break;
                case ID_SAVE:
                    save_all(a);
                    break;
                case ID_CANCEL:
                    done = 1;
                    break;
                default:
                    break;
                }
                if (g == &a->gcurstyle || g == &a->gblink || g == &a->gbell ||
                    g == &a->gbold || g == &a->gmeta || g == &a->gcopy || g == &a->gwheel)
                    refresh_labels(a);
                redraw(a);
            } else if (g) {
                redraw(a); /* a string field was clicked */
            }
        } else if (im->Class & IDCMP_RAWKEY) {
            if (im->IAddress)
                redraw(a); /* typed into a field */
        } else if (im->Class & (IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW)) {
            redraw(a);
        }
        ReplyMsg((struct Message *)im);
        if (done)
            break;
    }

    CloseWindow(a->win);
    DeleteMsgPort(a->port);
    FreeVec(a);
    CloseLibrary((struct Library *)DOSBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;

fail:
    /* whatever was made, undone once, in the reverse order */
    if (a->win)
        CloseWindow(a->win);
    if (a->port)
        DeleteMsgPort(a->port);
    FreeVec(a);
    CloseLibrary((struct Library *)DOSBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 1;
}
