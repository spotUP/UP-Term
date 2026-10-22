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
#include "menu_core.h"
#include "../config/keymap_load.h"
#include "../handler/upreq.h"
#include "keymap_default.inc"

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

/* leaf actions that need no terminal of ours: the commodity's own set */
static int run_leaf(const km_menu *m)
{
    switch (m->kind) {
    case KA_RUN:
        return system(m->arg);
    case KA_REXX: {
        char line[160];
        snprintf(line, sizeof line, "RX \"%s\"", m->arg);
        return system(line);
    }
    case KA_OPEN:
        return system("NewShell \"XCON:0/20/640/300/UP-Term/CLOSE\"");
    default: {
        /* tab:, menuid: and slash: belong to a terminal window: the handler's request port
         * (handler/upreq.h) takes the action by name and runs it as a key binding would */
        upreq_body r;
        long rc;
        if (!upreq_fill(&r, m->kind, m->arg)) {
            fprintf(stderr, "amarchy-menu: %s is not a terminal action\n", m->arg);
            return UPREQ_UNKNOWN;
        }
        rc = upreq_send(&r);
        if (rc)
            fprintf(stderr, "amarchy-menu: %s refused (%ld)\n", r.action, rc);
        return (int)rc;
    }
    }
}

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
        return run_leaf(&km.menu[it[i].row]);
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
    keymap_load(&km, km_default_text);
    if (argc < 2) {
        fputs("usage: amarchy-menu menu [node] | viewer\n", stderr);
        return 2;
    }
    hotkeys_init();
    rc = ui_start("amarchy-menu");
    if (rc)
        return rc;
    if (!strcmp(argv[1], "viewer"))
        rc = viewer();
    else
        rc = menu_loop(argc > 2 && strcmp(argv[2], "root") ? argv[2] : "");
    ui_stop();
    return rc == 255 ? 0 : rc;
}
