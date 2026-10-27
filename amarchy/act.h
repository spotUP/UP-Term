/* Amarchy actions (plan 2026-10-22-amarchy-commodity.md section 3). Amiga only. */
#ifndef AMARCHY_ACT_H
#define AMARCHY_ACT_H

/* run:<command line> and rexx:<script>: SystemTags asynchronously, input and
 * output NIL:, a 32 KB stack, the commodity's command path cloned so the child
 * finds C: and outlives us. 1 when started. */
int act_run(const char *cmd);
int act_rexx(const char *arg);
/* run cmd with a new XCON: window as its input and output (the quick menu, the key viewer). */
int act_in_window(const char *cmd, int cols, int rows);
/* The menu and key viewer through the warm server (menu_open.h): text 1 sends to `amarchy-menu serve`, text 0
 * to `amarchy-gui serve`, when it is alive; else the cold path (act_in_window(line, cols, rows) for the text
 * menu, act_run(line) for the drawn one) and a throttled server restart. dir: where the programs live;
 * kind: KA_MENU or KA_VIEWER; node: the start node ("root" or empty for the top). Always 1. */
int act_menu_open(const char *dir, int text, int kind, const char *node, const char *line, int cols, int rows);
/* Start `amarchy-menu serve` and `amarchy-gui serve` (after cx_install); not waited for. 1 when both were tried. */
int act_menu_server_start(const char *dir);
/* Commodity quit: close a showing menu, end both servers (bounded wait each). */
void act_menu_server_quit(void);
/* open:uterm: a new UP-Term window (the README's NewShell XCON: line). */
int act_open_uterm(void);

#endif
