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
#include "atiming.h"
#include "menu_core.h"
#include "index_core.h"
#include "ixwatch.h"
#include "leaf.h"
#include "gthemes_dos.h"
#include "menu_open.h"
#include <signal.h>
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

/* every printable key plus Backspace, Delete and the Left arrow ends the menu as a hotkey */
static char hot[128];

static void hotkeys_init(void)
{
    int c, n = 0;
    for (c = 32; c < 127; c++)
        hot[n++] = (char)c;
    hot[n++] = 8;
    hot[n++] = 127;
    hot[n++] = UI_HOTKEY_LEFT;
    hot[n] = 0;
}

/* A value for an action that needs one; 1 with out set, 0 on Escape or an empty line. */
static int ask_value(void *ctx, const char *label, const char *usage, char *out, int n)
{
    char prompt[UPREQ_ANSWER + 40];
    FILE *f;
    int ok = 0;
    (void)ctx;
    snprintf(prompt, sizeof prompt, "%s (%s):", label, usage);
    ui_opt.kind = K_INPUT;
    ui_opt.title = label;
    ui_opt.prompt = prompt;
    ui_opt.hotkeys = 0;
    ui_opt.nentries = 0;
    ui_opt.init[0] = 0;
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

static const lf_ui leaf_ui = { ask_value, say_refused, 0, 1 };   /* the text menu is an UP-Term window: self_in_mru */

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

static int on_tick(void *ctx)
{
    (void)ctx;
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);
        closing = 1;
    }
    if (closing)
        return 1;
    return wt.on && ixw_poll(&wt);
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
    char keep[IXW_ROW > KM_LABEL ? IXW_ROW : KM_LABEL];   /* the entry the cursor stays on over a rebuild */
    int keep_status = 0, shift, keep_ix = -1;   /* keep_ix: a setting row the cursor stays on after a set */

    strncpy(node, start, sizeof node - 1);
    node[sizeof node - 1] = 0;
    filter[0] = 0;
    keep[0] = 0;
    for (;;) {
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
        if (i) {                               /* a typed key: grow or shorten the filter */
            int l = (int)strlen(filter);
            if (i == 8 || i == 127 || i == UI_HOTKEY_LEFT) {
                int act = mn_back(1, filter, node);
                char up[KM_ID * 2];
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
            return lf_open_index(it[i].label, path, &leaf_ui);
        }
        if (MN_IS_GROUP(&it[i])) {
            mn_child_node(node, &it[i], node, sizeof node);
            filter[0] = 0;
            continue;
        }
        if (MN_IS_SETTING(&it[i]) || MN_IS_CHOICE(&it[i])) {
            int choice = MN_IS_CHOICE(&it[i]);
            char up[KM_ID * 2];
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
            rc = lf_run(&km.menu[it[i].row], &leaf_ui);
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
    free_level();
    ix_free(&ixs);
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

int main(int argc, char **argv)
{
    int rc;
    if (qmark_argv(argc, argv)) { /* `amarchy-menu ?` shows the usage and ends */
        fputs("usage: amarchy-menu menu [node] | viewer\n", stdout);
        return 0;
    }
    at_mark("start");
    keymap_load(&km, km_default_text);
    if (argc < 2) {
        fputs("usage: amarchy-menu menu [node] | viewer\n", stderr);
        return 2;
    }
    mo_register();
    signal(SIGINT, on_int);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    hotkeys_init();
    {
        const char *t = mn_term_default(getenv("TERM"));
        if (t)
            setenv("TERM", t, 1);
    }
    if (strcmp(argv[1], "viewer"))
        ui_osd(1);                             /* the menu is a launcher: no frame, no footer */
    rc = ui_start("amarchy-menu");
    at_mark("terminal");
    if (rc) {
        /* the window closes when this exits: keep the message readable until Return */
        fputs("amarchy-menu: no terminal could be started. Press Return.\n", stderr);
        (void)getchar();
        return rc;
    }
    if (!strcmp(argv[1], "viewer"))
        rc = viewer();
    else
        rc = menu_loop(argc > 2 && strcmp(argv[2], "root") ? argv[2] : "");
    ui_stop();
    /* Escape is a normal leave; so is end of file on the console (the window closed on
     * deactivation, CLOSEINACTIVE): ui_run reports it as EXIT_NO_TERMINAL */
    return rc == 255 || rc == EXIT_NO_TERMINAL ? 0 : rc;
}
