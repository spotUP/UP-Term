/* MENU-TOGGLE: the open menu (amarchy-gui or amarchy-menu) owns the public port MO_PORT; a menu hotkey
 * pressed while it exists closes that menu (Ctrl-C to its task) instead of opening a second one.
 * The port carries no messages: it is only the registry name -> task. */
#ifndef AMARCHY_MENU_OPEN_H
#define AMARCHY_MENU_OPEN_H

#define MO_PORT "AMARCHY.MENU"

#include <exec/ports.h>
#include "menusrv_core.h"

/* The warm menu server (plan 2026-10-27-amarchy-warm-menu-server.md): `amarchy-menu serve` stays loaded
 * and owns the public port MS_PORT; the commodity sends one ms_msg per open instead of loading the 438 KB
 * program. The server replies at once on receipt (before it opens anything), so an unreplied message at the
 * next open means the server is wedged. */
/* MS_PORT, MS_TASK_NAME, GS_PORT, GS_TASK_NAME: menusrv_core.h */
enum { MS_MENU = 1, MS_VIEWER, MS_QUIT };
#define MS_SPEC 192
#define MS_NODE 64
struct ms_msg {
    struct Message m;
    LONG kind;
    char spec[MS_SPEC];   /* the full XCON: spec of the window; the server opens it itself */
    char node[MS_NODE];   /* the start node, empty for the root */
};

/* The commodity's side: one static message, one reply port, and the two things only act.c knows. */
typedef struct mo_srv {
    const char *port;                       /* the server's public port: MS_PORT or GS_PORT */
    const char *task;                       /* the program it runs: MS_TASK_NAME or GS_TASK_NAME */
    struct MsgPort *reply;
    struct ms_msg msg;
    int out;                                /* msg is on the server's queue, unreplied */
    char spec[MS_SPEC];                     /* the window spec for the next send */
    void *user;
    void (*spawn)(void *user, const ms_req *r);     /* the cold path: a menu of its own */
    void (*start)(void *user);                      /* start the server process */
    ms_state st;
} mo_srv;
/* Fill ops for ms_open: find (FindPort + task_alive under Forbid, draining the reply port), take_over
 * (RemPort of a stale port), send (PutMsg inside the same Forbid as FindPort; falls back to spawn when the
 * port went), spawn/start_server (s->spawn, s->start), now (seconds). */
void mo_srv_ops(ms_ops *ops, mo_srv *s);
/* Commodity quit: tell a live idle server to end and wait up to 2 s for its reply. 1 when it replied. */
int mo_srv_quit(mo_srv *s);

/* The menu program, first thing in main: clear a stale Ctrl-C, add the port (unless one exists),
 * remove it again at exit (atexit). */
void mo_register(void);
/* RemPort (in Forbid) and free; safe to call twice. */
void mo_unregister(void);
/* The commodity: if a menu is open, tell it to close. 1 if one was found. */
int mo_close_open(void);

#endif
