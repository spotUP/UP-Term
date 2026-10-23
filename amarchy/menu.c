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
#include "atiming.h"
#include "menu_core.h"
#include "leaf.h"
#include "../config/keymap_load.h"
#include "../handler/upreq.h"
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
typedef char osd_pad_match[MN_OSD_PAD == UI_OSD_PAD ? 1 : -1];

static keymap km;

/* every printable key plus Backspace and Delete ends the menu as a hotkey */
static char hot[128];

static void hotkeys_init(void)
{
    int c, n = 0;
    for (c = 32; c < 127; c++)
        hot[n++] = (char)c;
    hot[n++] = 8;
    hot[n++] = 127;
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

static const lf_ui leaf_ui = { ask_value, say_refused, 0 };

static int menu_loop(const char *start)
{
    mn_item it[MN_MAX_ITEMS];
    int idx[MN_MAX_ITEMS], n, c, i, rc = 0;
    char node[KM_ID * 2], filter[KM_LABEL], prompt[96];
    entry ent[MN_MAX_ITEMS];

    strncpy(node, start, sizeof node - 1);
    node[sizeof node - 1] = 0;
    filter[0] = 0;
    for (;;) {
        n = mn_children(&km, node, it, MN_MAX_ITEMS);
        c = mn_filter(it, n, filter, idx, MN_MAX_ITEMS);
        for (i = 0; i < c; i++) {
            ent[i].tag = it[idx[i]].label;
            ent[i].item = it[idx[i]].row < 0 ? ">" : "";
        }
        ui_opt.kind = K_MENU;
        ui_opt.title = "Menu";
        ui_opt.backtitle = node[0] ? node : "Amarchy";
        ui_opt.hotkeys = hot;
        ui_opt.entries = ent;
        ui_opt.nentries = c;
        ui_opt.default_item = c ? ent[0].tag : 0;
        snprintf(prompt, sizeof prompt, "Filter: %s", filter);
        ui_opt.prompt = prompt;
        ui_prepare();
        at_mark("ready");
        rc = ui_run();
        if (rc == EXIT_NO_TERMINAL)
            return rc;
        if (rc == 255) {                       /* Escape */
            char up[KM_ID * 2];
            if (filter[0]) {
                filter[0] = 0;
            } else if (mn_parent(node, up, sizeof up)) {
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
            if (i == 8 || i == 127) {
                if (l)
                    filter[l - 1] = 0;
            } else if (l < (int)sizeof filter - 1) {
                filter[l] = (char)i;
                filter[l + 1] = 0;
            }
            continue;
        }
        if (ui_selected() < 0 || ui_selected() >= c)
            continue;
        i = idx[ui_selected()];
        if (it[i].row < 0) {
            mn_child_node(node, &it[i], node, sizeof node);
            filter[0] = 0;
            continue;
        }
        return lf_run(&km.menu[it[i].row], &leaf_ui);
    }
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
    return ui_run();
}

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
