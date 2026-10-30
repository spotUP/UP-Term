/* amarchy-menu: the Amarchy quick menu and key viewer (plan 2026-10-22-amarchy-commodity.md,
 * Phase D). A gcc/ixemul program linked with libupkgui.a (decision D4: the library directly, no
 * upkg-ui process in between). The commodity starts it in an XCON: window per press:
 *     amarchy-menu menu [node]     Super+Space ("menu:root"), Super+Shift+Space ("menu:apps")
 *     amarchy-menu viewer          Super+K
 * Typing filters the current level (each key ends the library's menu through its hotkeys, the
 * filter grows, the menu is shown again), Backspace shortens the filter, Enter opens a group or
 * runs a leaf, Escape clears a filter, else goes up one level, else leaves. The rows come from
 * the one keymap (keymap_load) and the walking is amarchy/menu_core.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <upkgui/ui.h>
#include <exec/types.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "serve_sig.h"
#include "atiming.h"
#include "menu_core.h"
#include "index_core.h"
#include "ixwatch.h"
#include "leaf.h"
#include "gthemes_dos.h"
#include "pkgs_dos.h"
#include "job_dos.h"
#include "sentinel_dos.h"
#include "menu_open.h"
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#define timeval ndk_timeval
#define timerequest ndk_timerequest
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#undef timeval
#undef timerequest
#include "termenv.h"
#include "taskalive.h"
#include "../config/keymap_load.h"
#include "../handler/upreq.h"
#include "../handler/upactive.h"
#include "../tty/qmark.h"
#include "keymap_default.inc"

/* ixemul runs main on a stack of this size (ncurses and the UI library are deep); the cookie is what
 * vsh, the Shell and Workbench read from the file to start it on that much (a 4 KB shell stack froze
 * rig 4). tests/test_amarchy_stack.py reads both from the built binary. */
#define MENU_STACK 262144
#define MENU_STR2(x) #x
#define MENU_STR(x) MENU_STR2(x)
long __stack = MENU_STACK;
__attribute__((used)) const char amarchy_stack_cookie[] = "$STACK: " MENU_STR(MENU_STACK);

/* The commodity sizes the window with menu_core.h's constants (it cannot include ui.h): they must
 * equal the library's, else the OSD clips or leaves blank rows. A negative array size fails the build. */
typedef char osd_head_lines_match[MN_OSD_HEAD_LINES == UI_OSD_HEAD_LINES ? 1 : -1];
typedef char osd_bottom_lines_match[MN_OSD_BOTTOM_LINES == UI_OSD_BOTTOM_LINES ? 1 : -1];
typedef char osd_pad_match[MN_OSD_PAD == UI_OSD_PAD ? 1 : -1];

static keymap km;

/* every printable key plus Backspace, Delete and the Left and Right arrows ends the menu as a hotkey */
static char hot[128];

static void hotkeys_init(void)
{
    int c, n = 0;
    for (c = 32; c < 127; c++)
        hot[n++] = (char)c;
    hot[n++] = 8;
    hot[n++] = 127;
    hot[n++] = UI_HOTKEY_LEFT;
    hot[n++] = UI_HOTKEY_RIGHT;
    hot[n] = 0;
}

/* A value for an action that needs one; 1 with out set, 0 on Escape or an empty line. out[0] is the text the
 * line starts with (the caller clears it when there is none). */
static int ask_value(void *ctx, const char *label, const char *usage, char *out, int n)
{
    char prompt[UPREQ_ANSWER + 40];
    FILE *f;
    int ok = 0, i;
    (void)ctx;
    snprintf(prompt, sizeof prompt, "%s (%s):", label, usage);
    ui_opt.kind = K_INPUT;
    ui_opt.title = label;
    ui_opt.prompt = prompt;
    ui_opt.hotkeys = 0;
    ui_opt.nentries = 0;
    /* init is wide: widen the bytes one for one (Amiga text is Latin-1, so each byte is its code point) */
    for (i = 0; i < (int)(sizeof ui_opt.init / sizeof ui_opt.init[0]) - 1 && out[i]; i++)
        ui_opt.init[i] = (unsigned char)out[i];
    ui_opt.init[i] = 0;
    ui_prepare();
    if (ui_run() != 0 || !(f = tmpfile()))
        return 0;
    ui_answer(f);
    rewind(f);
    if (fgets(out, n, f)) {
        out[strcspn(out, "\r\n")] = 0;
        ok = out[0] != 0;
    }
    fclose(f);
    return ok;
}

/* A refused action says why in the menu's window before it closes (it used to exit silently,
 * the reason lost on stderr of a window that was going). */
static void say_refused(void *ctx, const char *label, long rc, const char *answer)
{
    const char *why = answer && answer[0] ? answer :
                      rc == UPREQ_NOPORT ? "no UP-Term window to run it in" :
                      rc == UPREQ_NOWINDOW ? "the terminal has no window" :
                      rc == UPREQ_BADVERSION ? "the terminal is another UP-Term version: reboot" :
                      "the terminal does not know it";
    char text[UPREQ_ANSWER + 80];
    (void)ctx;
    snprintf(text, sizeof text, "%s: %s", label, why);
    ui_message("%s", text);
}

static int text_run_job(void *ctx, const char *line, const char *title);
static const lf_ui leaf_ui = { ask_value, say_refused, 0, 1, 0, text_run_job };   /* the text menu is an UP-Term window: self_in_mru */
static int job_ran;             /* a job view ran: the menu stays open on the same row */

/* One level's items, sized by ix_level_cap once the index is loaded (menu_loop) */
static ix_index ixs;
static int menu_cond;           /* decided once at open: mn_cond(terminal present, DOPUS.1 present, WORKBENCH present) */
static mn_item *it;
static int *idx, cap;
static entry *ent;
/* The scan this menu watches (RESCAN-FEEDBACK): its status row is the first entry of every level while
 * wt.row is set. Ticks poll it while wt.on; the DONE or FAILED row stays until the menu closes. */
static ix_watcher wt;

/* MENU-TOGGLE: the menu hotkey pressed again signals Ctrl-C to this task; ixemul turns it into SIGINT,
 * a plain break bit is checked as well. Either sets closing; the next tick ends the menu. */
static volatile int closing;

static void on_int(int sig)
{
    (void)sig;
    closing = 1;
}

/* THEME-PREVIEW-LIVE-A1200: Style > Amarchy theme recolours the open consoles and agent tabs (this menu draws in one of
 * them, so it recolours too) through the F3 bridge on every highlight move (ui_on_move, per key); any other row, Esc and closing give the
 * consoles the saved theme back (gt_preview_step). libupkgui moves the cursor itself: the move is read from ui_selected
 * after ui_prepare and on each tick. pv_drawn is what the consoles show; pv_sel the row last synced. */
static char pv_drawn[GT_NAME];
static int pv_sel = -1, pv_shift, pv_count;

static void preview_sync(int restore)
{
    char path[IX_PATH], cur[GT_NAME];
    const char *row = 0;
    int s = ui_selected();
    pv_sel = s;
    if (!restore && it && s >= pv_shift && s - pv_shift < pv_count) {
        const mn_item *m = &it[idx[s - pv_shift]];
        if (MN_IS_INDEX(m) && ix_launch_path(&ixs, m, path, (int)sizeof path))
            row = path;
    }
    gt_dos_current(cur, (int)sizeof cur);
    if (!pv_drawn[0])
        strcpy(pv_drawn, cur);       /* the menu opened: the consoles show the saved theme */
    gt_preview_step(row, cur, pv_drawn, (int)sizeof pv_drawn, gt_dos_preview_send);
}

/* THEME-PREVIEW-PER-KEY: libupkgui calls this right after a key moved the cursor, so the consoles recolour on every
 * arrow key, not on the next tick. The tick still syncs the moves no key made (a rebuilt level). */
static void on_move(void *ctx)
{
    (void)ctx;
    preview_sync(0);
}

static int on_tick(void *ctx)
{
    (void)ctx;
    if (ui_selected() != pv_sel)
        preview_sync(0);             /* the highlight moved */
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);
        closing = 1;
    }
    if (closing)
        return 1;
    return wt.on && ixw_poll(&wt);
}

/* ---- the job view (plan 2026-10-27-amarchy-job-view.md RJ.2t, RJ.5): a textbox that follows the job's output.
 * All logic is job_core (lines, footer, tone, keys); this is the libupkgui glue. ---- */
static jc tjob;
static long tjob_seen;           /* absolute index of the next line to hand to the textbox */

static void job_show(void)
{
    static const int st[] = { UI_ST_DIM, UI_ST_OK, UI_ST_FAIL };   /* indexed by jc_tone */
    char foot[JOB_FOOT_MAX];
    long first = jc_first_new(&tjob, tjob_seen), n = tjob.total;
    jc_footer(&tjob, foot, sizeof foot);
    for (; first < n; first++)
        ui_add_line(jc_line(&tjob, (int)(first - jc_dropped(&tjob))));
    tjob_seen = n;
    ui_status(foot, st[jc_tone(&tjob)]);
}

/* The tick of the job: take the new output and redraw when something arrived or the state changed. */
static int job_tick(void *ctx)
{
    long total = tjob.total;
    int state = tjob.state, ended = tjob.ended;
    (void)ctx;
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);
        closing = 1;
    }
    if (closing)
        return 1;
    jb_poll_publish(&tjob);
    if (tjob.total != total || tjob.state != state || tjob.ended != ended) {
        job_show();
        ui_invalidate();
    }
    return 0;
}

/* A job: row: run the command and show its output; returns when the view closes. Esc while it runs cancels it, Esc
 * or Enter after the end closes, and the menu hotkey or Ctrl-C cancels then closes. A job that cannot start runs as
 * run: (lf_run_line). */
static int text_run_job(void *ctx, const char *line, const char *title)
{
    (void)ctx;
    if (!jb_start(&tjob, line, 0, title))
        return lf_run_line(line, &leaf_ui);
    ui_opt.kind = K_TEXTBOX;
    ui_opt.title = title && title[0] ? title : "Job";
    ui_opt.backtitle = "Amarchy";
    ui_opt.prompt = "";
    ui_opt.hotkeys = 0;
    ui_opt.nentries = 0;
    ui_prepare();
    ui_clear_lines();
    ui_follow(1);
    tjob_seen = 0;
    job_show();
    ui_on_tick(job_tick, 0);
    for (;;) {
        int rc = ui_run(), act, d;
        if (closing || rc == EXIT_NO_TERMINAL)
            break;
        if (rc == UI_TICK)
            continue;
        act = jc_key_act(&tjob, rc == 255 ? JKEY_ESC : JKEY_ENTER, 1, &d);
        if (act == JA_CLOSE)
            break;
        if (act == JA_CANCEL) {
            jb_cancel(&tjob);
            job_show();
        }
    }
    if (tjob.state == JC_RUNNING)
        jb_cancel(&tjob);
    lf_job_done(&ixs, line, tjob.state);   /* an upkg job: the Packages list reloads at the next entry */
    jb_end();
    ui_on_tick(on_tick, 0);
    ui_on_move(on_move, 0);
    job_ran = 1;
    return 0;
}

static void free_level(void)
{
    free(it);
    free(idx);
    free(ent);
    it = 0;
    idx = 0;
    ent = 0;
}

/* Size the level buffers for the loaded index (one more entry for the status row). 1 on success. */
static int alloc_level(void)
{
    cap = ix_level_cap(&ixs);
    it = malloc(cap * sizeof *it);
    idx = malloc(cap * sizeof *idx);
    ent = malloc((cap + 1) * sizeof *ent);
    if (it && idx && ent)
        return 1;
    free_level();
    return 0;
}

/* The scan finished: read the new index so the Apps level lists the new rows. */
static int reload_index(void)
{
    free_level();
    ix_free(&ixs);
    ix_load_file(&km, &ixs, IX_CACHE_PATH);
    lf_load_listers(&ixs);
    gt_dos_load_rows(&ixs);
    return alloc_level();
}

static int menu_levels(const char *start)
{
    int n, c, i, rc = 0;
    char node[KM_ID * 2], filter[KM_LABEL], prompt[96];
    char keep[IXW_ROW > MN_LABEL ? IXW_ROW : MN_LABEL];   /* the entry the cursor stays on over a rebuild */
    mn_bsguard bsg;
    int keep_status = 0, shift, keep_ix = -1;   /* keep_ix: a setting row the cursor stays on after a set */

    strncpy(node, start, sizeof node - 1);
    node[sizeof node - 1] = 0;
    filter[0] = 0;
    mn_bsguard_init(&bsg);
    keep[0] = 0;
    for (;;) {
        if (pk_dos_due(&ixs, node)) {      /* Install > Packages entered: upkg gives its rows now, not on every open */
            free_level();
            pk_dos_load_rows(&ixs);
            if (!alloc_level()) {
                ui_message("%s", "amarchy-menu: out of memory");
                return 20;
            }
        }
        n = ix_level_t(&km, &ixs, node, filter, it, cap, menu_cond);
        lf_setting_relabel(it, n);          /* a settings row shows its value */
        c = mn_filter(it, n, filter, idx, cap);
        shift = wt.row[0] ? 1 : 0;
        if (shift) {
            ent[0].tag = wt.row;
            ent[0].item = "";
        }
        for (i = 0; i < c; i++) {
            ent[i + shift].tag = it[idx[i]].label;
            ent[i + shift].item = MN_IS_GROUP(&it[idx[i]]) ? ">" : "";
        }
        {   /* the rows shown (not the status row), the same value the drawn menu publishes */
            char rows[256];
            mn_rows_join(it, idx, c, rows, sizeof rows);
            am_sentinel_str("AMARCHY_ROWS", rows);
        }
        ui_opt.kind = K_MENU;
        ui_opt.title = "Menu";
        ui_opt.backtitle = node[0] ? node : "Amarchy";
        ui_opt.hotkeys = hot;
        ui_opt.entries = ent;
        ui_opt.nentries = c + shift;
        ui_opt.default_item = c + shift ? ent[c ? shift : 0].tag : 0;
        if (shift && keep_status)
            ui_opt.default_item = ent[0].tag;
        else if (keep_ix >= 0 && (i = mn_pick_setting(it, idx, c, keep_ix)) >= 0)
            ui_opt.default_item = ent[i + shift].tag;
        else if (keep[0] && (i = mn_pick(it, idx, c, keep)) >= 0)
            ui_opt.default_item = ent[i + shift].tag;
        keep_ix = -1;
        keep_status = 0;
        keep[0] = 0;
        snprintf(prompt, sizeof prompt, "Filter: %s", filter);
        ui_opt.prompt = prompt;
        ui_prepare();
        pv_shift = shift;
        pv_count = c;
        preview_sync(0);                   /* the level opens on its row: (current) in the theme list, so no write */
        at_mark("ready");
        ui_on_tick(on_tick, 0);
        rc = ui_run();
        if (closing)
            return 255;                    /* MENU-TOGGLE: leave from any level, as Escape at the root */
        if (rc == UI_TICK) {                   /* the status row changed: rebuild, cursor stays */
            int s = ui_selected();
            keep_status = shift && s == 0;
            if (!keep_status && s >= shift && s - shift < c)
                strcpy(keep, it[idx[s - shift]].label);
            if (wt.state == IXW_DONE && !reload_index()) {
                ui_message("%s", "amarchy-menu: out of memory");
                return 20;
            }
            continue;
        }
        if (rc == EXIT_NO_TERMINAL)
            return rc;
        if (rc == 255) {                       /* Escape */
            char up[KM_ID * 2];
            int act = mn_back(0, filter, node);
            if (act == MN_BACK_CLEAR_FILTER) {
                filter[0] = 0;
            } else if (act == MN_BACK_UP && mn_parent(node, up, sizeof up)) {
                strcpy(node, up);
            } else
                return 0;
            continue;
        }
        if (rc != 0)
            return rc;
        i = ui_hotkey();
        if (i == UI_HOTKEY_RIGHT) {            /* Right enters a group or a setting with a page, as Return does */
            int s = ui_selected();
            if (s >= shift && s < c + shift && mn_right_enters(&it[idx[s - shift]]))
                i = 0;                         /* the Return path below; a leaf is not run by Right */
            else {
                keep_status = shift && s == 0;
                if (!keep_status && s >= shift && s < c + shift)
                    strcpy(keep, it[idx[s - shift]].label);
                continue;                      /* nothing to enter: the cursor stays */
            }
        }
        if (i) {                               /* a typed key: grow or shorten the filter */
            int l = (int)strlen(filter);
            if (i == 8 || i == 127 || i == UI_HOTKEY_LEFT) {
                /* libupkgui ui_hotkey reports no auto-repeat flag, so this path keeps the time guard */
                int act = mn_back_rep(&bsg, at_ticks(), filter, node);
                char up[KM_ID * 2];
                if (act == MN_BACK_NONE)
                    continue;               /* a held Backspace repeating past the empty query */
                if (act == MN_BACK_DROP_CHAR)
                    filter[l - 1] = 0;
                else if (act == MN_BACK_UP && mn_parent(node, up, sizeof up))
                    strcpy(node, up);
                else
                    return 0;               /* root, no filter: closes as Escape */
            } else if (l < (int)sizeof filter - 1) {
                filter[l] = (char)i;
                filter[l + 1] = 0;
            }
            continue;
        }
        if (ui_selected() < shift || ui_selected() >= c + shift)
            continue;                          /* the status row does nothing */
        i = idx[ui_selected() - shift];
        if (MN_IS_INDEX(&it[i])) {
            char path[IX_PATH];
            if (!ix_launch_path(&ixs, &it[i], path, sizeof path))
                continue;
            rc = lf_open_index(it[i].label, path, &leaf_ui);
            {   /* a theme pick saved the theme: back out ONE level (the saved theme is the one the closing restore keeps) */
                char up[KM_ID * 2];
                if (!rc && mn_pick_back(path, node, up, sizeof up)) {
                    strcpy(node, up);
                    filter[0] = 0;
                    continue;
                }
            }
            return rc;
        }
        if (lf_pkg_search_row(node, &it[i])) {  /* Packages > Search asks its text, then lists the matches */
            if (lf_pkg_search(&ixs, &leaf_ui)) {
                mn_child_node(node, &it[i], node, sizeof node);
                filter[0] = 0;
            }
            continue;
        }
        if (MN_IS_PKGACT(&it[i])) {             /* a package action: a job, Launch, or No on the Remove confirm page */
            mn_item row = it[i];                /* the level is rebuilt below */
            int back = lf_pkg_back_levels(&row), k;
            job_ran = 0;
            if (row.ix >= 0 && row.seg[0] && row.seg[0] != PK_ACT_NO) {
                rc = lf_pkg_action(&ixs, &row, &leaf_ui);
                if (!job_ran)
                    return rc;                  /* Launch, or a refusal: the menu closes as a program row does */
                if (closing)
                    return 255;
            }
            for (k = 0; k < back; k++) {
                char up[KM_ID * 2];
                if (mn_parent(node, up, sizeof up))
                    strcpy(node, up);
            }
            filter[0] = 0;
            continue;
        }
        if (MN_IS_GROUP(&it[i])) {
            mn_child_node(node, &it[i], node, sizeof node);
            filter[0] = 0;
            continue;
        }
        if (MN_IS_SETTING(&it[i]) || MN_IS_CHOICE(&it[i])) {
            int choice = MN_IS_CHOICE(&it[i]);
            char up[KM_ID * 2];
            if (!choice && (SD_HAS_PICKER(&sd_settings[it[i].ix]) || SD_HAS_LIST(&sd_settings[it[i].ix]))) {
                mn_child_node(node, &it[i], node, sizeof node);   /* the colour list */
                filter[0] = 0;
                continue;
            }
            if (!lf_setting_enter(&it[i], &leaf_ui))
                continue;
            if (choice && mn_parent(node, up, sizeof up))
                strcpy(node, up);           /* the pick is made: back to the page */
            else
                keep_ix = it[i].ix;
            continue;
        }
        {
            ix_prog p0;
            int alive0 = 0, have0 = ixw_snapshot(&p0, &alive0);   /* the run counter before the launch */
            if (lf_plan(km.menu[it[i].row].kind) == LF_SCAN && ix_watcher_adopt_live(&wt, have0, &p0, alive0))
                continue;                               /* one scanner at a time: show the live one */
            job_ran = 0;
            rc = lf_run(&km.menu[it[i].row], &leaf_ui);
            if (job_ran) {                       /* the job view closed: the menu again, on the same row */
                if (closing)
                    return 255;
                strcpy(keep, it[i].label);
                continue;
            }
            if (rc != LF_WATCH)
                return rc;
            ix_watcher_begin(&wt, 1, have0, &p0, alive0);
        }
    }
}

static int menu_loop(const char *start)
{
    int rc = 20;
    ix_load_file(&km, &ixs, IX_CACHE_PATH);
    lf_load_listers(&ixs);
    gt_dos_load_rows(&ixs);
    menu_cond = mn_cond(upactive_terminal_present(leaf_ui.self_in_mru), upactive_dopus_present(), upactive_workbench_present());
    {   /* a scan that runs already (started from a menu that closed on deactivation) is shown too */
        ix_prog p0;
        int alive0 = 0, have0 = ixw_snapshot(&p0, &alive0);
        ix_watcher_begin(&wt, 0, have0, have0 ? &p0 : 0, alive0);
    }
    if (alloc_level())
        rc = menu_levels(start);
    else
        ui_message("%s", "amarchy-menu: out of memory");
    if (pv_drawn[0])
        preview_sync(1);   /* closed on a previewed row (Esc, toggle): the consoles get the saved theme back */
    pv_drawn[0] = 0;       /* a warm server's next open starts from the saved theme again */
    pv_sel = -1;
    free_level();
    ix_free(&ixs);
    ix_pkg_free(&ixs);   /* one open per process: the cache loads once per process */
    return rc;
}

static int viewer(void)
{
    static char text[KM_MAX_BIND * 100];
    char *p, *nl;
    mn_viewer_text(&km, text, sizeof text);
    ui_clear_lines();
    for (p = text; *p; p = nl ? nl + 1 : p + strlen(p)) {
        nl = strchr(p, '\n');
        if (nl)
            *nl = 0;
        ui_add_line(p);
        if (!nl)
            break;
    }
    ui_opt.kind = K_TEXTBOX;
    ui_opt.title = "Key bindings";
    ui_opt.prompt = "";
    ui_opt.hotkeys = 0;
    ui_opt.nentries = 0;
    ui_prepare();
    ui_on_tick(on_tick, 0);
    {
        int vrc = ui_run();
        return closing ? 255 : vrc;
    }
}

/* front_screen_name (AMB3: leaves open on the screen in front) reads the front screen from it */
struct IntuitionBase *IntuitionBase;

/* The theme's background is palette pen 0 (libupkgui init_theme: every attribute is a pair on background 0).
 * Painted before newterm so a console that opened in its own default pens is not shown in them. ncurses'
 * own first clear (clearok at ui_prepare, `clear=\E[H\E[2J` with bce) still runs in the default pair; see
 * thoughts/shared/research/2026-10-27-amarchy-warm-menu-server.md. */
static void prepaint(void)
{
    fputs("\033[?25l\033[0;40m\033[H\033[2J", stdout);
    fflush(stdout);
}

/* One ui_start to ui_stop on whatever stdio is bound to: the menu or the viewer. */
static int run_ui(int is_viewer, const char *node)
{
    int rc;
    const char *t;
    termenv_ensure();
    t = mn_term_default(getenv("TERM"));
    if (t)
        setenv("TERM", t, 1);
    if (!is_viewer)
        ui_osd(1);                             /* the menu is a launcher: no frame, no footer */
    prepaint();
    rc = ui_start("amarchy-menu");
    at_mark("terminal");
    if (rc) {
        /* the window closes when this exits: keep the message readable until Return */
        fputs("amarchy-menu: no terminal could be started. Press Return.\n", stderr);
        (void)getchar();
        return rc;
    }
    ui_on_tick(on_tick, 0);                    /* ui_stop clears the tick: set again after each ui_start */
    ui_on_move(on_move, 0);
    rc = is_viewer ? viewer() : menu_loop(node);
    ui_stop();
    /* Escape is a normal leave; so is end of file on the console (the window closed on
     * deactivation, CLOSEINACTIVE): ui_run reports it as EXIT_NO_TERMINAL */
    return rc == 255 || rc == EXIT_NO_TERMINAL ? 0 : rc;
}

/* ---- the warm server: `amarchy-menu serve` (menu_open.h) ---- */
static int std_save[3] = { -1, -1, -1 };

/* One open: the commodity sent the window spec; the server opens that XCON: window itself (no shell runs on
 * it, so no prompt prints), makes it the console of this process and binds fd 0/1/2 to it (probes P1/P2,
 * thoughts/shared/research/2026-10-27-menusrv-probes.md), runs the menu, and gives everything back. */
static void serve_one(const struct ms_msg *q)
{
    BPTR w, oldin, oldout;
    struct MsgPort *oldcon;
    int fd, i;
    at_mark("start");
    SetSignal(0, SIGBREAKF_CTRL_C);
    closing = 0;
    keymap_free(&km);
    keymap_load(&km, km_default_text);
    lf_reset();
    w = Open((STRPTR)q->spec, MODE_NEWFILE);
    if (!w)
        return;
    oldcon = SetConsoleTask(((struct FileHandle *)BADDR(w))->fh_Type);
    fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        fflush(stdout);
        fflush(stderr);
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        clearerr(stdin);
        clearerr(stdout);
        clearerr(stderr);
        oldin = SelectInput(w);
        oldout = SelectOutput(w);
        mo_register();
        run_ui(q->kind == MS_VIEWER, q->node[0] && strcmp(q->node, "root") ? q->node : "");
        mo_unregister();
        SelectInput(oldin);
        SelectOutput(oldout);
        fflush(stdout);
        fflush(stderr);
        for (i = 0; i < 3; i++)
            if (std_save[i] >= 0)
                dup2(std_save[i], i);
        clearerr(stdin);
        clearerr(stdout);
        clearerr(stderr);
    }
    SetConsoleTask(oldcon);
    Close(w);
}

static int serve(void)
{
    struct MsgPort *port;
    struct ms_msg *m, q;
    int i, quit = 0;
    for (i = 0; i < 3; i++)
        std_save[i] = dup(i);
    port = CreateMsgPort();
    if (!port)
        return 20;
    port->mp_Node.ln_Name = (char *)MS_PORT;
    Forbid();
    {
        struct MsgPort *old = FindPort((STRPTR)MS_PORT);
        if (old && task_alive((struct Task *)old->mp_SigTask, MS_TASK_NAME)) {
            Permit();
            DeleteMsgPort(port);
            return 5;                          /* a live server exists */
        }
        if (old)
            RemPort(old);                      /* a killed server's stale port */
    }
    AddPort(port);
    Permit();
    signal(SIGINT, on_int);
    menu_ignore_hangup();                     /* a closed XCON console must not print "Hangup" or end the server */
    hotkeys_init();
    while (!quit) {
        Wait(1UL << port->mp_SigBit | SIGBREAKF_CTRL_C);
        SetSignal(0, SIGBREAKF_CTRL_C);        /* a stray toggle after a close must not end the server */
        while (!quit && (m = (struct ms_msg *)GetMsg(port)) != 0) {
            q = *m;
            if (q.kind == MS_QUIT) {
                Forbid();
                RemPort(port);                 /* gone before the commodity sees the reply */
                Permit();
                quit = 1;
            }
            ReplyMsg(&m->m);                   /* at once: the commodity never waits for the menu */
            if (!quit)
                serve_one(&q);
        }
    }
    while ((m = (struct ms_msg *)GetMsg(port)) != 0)
        ReplyMsg(&m->m);
    DeleteMsgPort(port);
    return 0;
}

int main(int argc, char **argv)
{
    lf_init();
    if (qmark_argv(argc, argv)) { /* `amarchy-menu ?` shows the usage and ends */
        fputs("usage: amarchy-menu menu [node] | viewer | serve\n", stdout);
        return 0;
    }
    if (argc < 2) {
        fputs("usage: amarchy-menu menu [node] | viewer | serve\n", stderr);
        return 2;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!strcmp(argv[1], "serve"))
        return serve();
    at_mark("start");
    keymap_load(&km, km_default_text);
    mo_register();
    signal(SIGINT, on_int);
    menu_ignore_hangup();                     /* the leaf's UP-Term window closes this console: no mid-launch kill */
    hotkeys_init();
    return run_ui(!strcmp(argv[1], "viewer"), argc > 2 && strcmp(argv[2], "root") ? argv[2] : "");
}
