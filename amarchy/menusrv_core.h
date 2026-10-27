/* The warm menu server's open decision, pure (host-tested by tests/test_menusrv.c): whether a hotkey open
 * is sent to the running server or falls back to a spawned menu, and when the server is (re)started. The
 * Amiga calls (FindPort + task_alive, PutMsg, the spawn, the server start) come in as ms_ops. */
#ifndef MENUSRV_CORE_H
#define MENUSRV_CORE_H

/* What mo_close_open does with the public AMARCHY.MENU port it found. */
enum { MO_NONE, MO_SIGNAL, MO_TAKE_OVER };
/* found: a port of that name exists; alive: its owner task is a live task of the expected name. */
int mo_verdict(int found, int alive);

/* What an open does. */
enum { MS_SEND, MS_SPAWN_RESTART };
/* outstanding: a message sent earlier is still unreplied (a wedged server). */
int ms_route(int found, int alive, int outstanding);

/* A restart of the server at most once per MS_RESTART_SECS: a server that dies at start must not respawn
 * on every key. */
#define MS_RESTART_SECS 10

typedef struct {
    int kind;                   /* menu or viewer: opaque here, copied into the message by send */
    const char *node;           /* the start node, 0 for the root */
} ms_req;

typedef struct {
    long last_start;            /* now() of the last server start; meaningful when started */
    int started;
} ms_state;

typedef struct {
    void *ctx;
    /* the state of the server port, in one look (the Amiga side does it under Forbid) */
    void (*find)(void *ctx, int *found, int *alive, int *outstanding);
    void (*take_over)(void *ctx);                   /* found but dead: remove the stale port */
    void (*send)(void *ctx, const ms_req *r);       /* PutMsg to the live server */
    void (*spawn)(void *ctx, const ms_req *r);      /* the fallback: a menu of its own */
    void (*start_server)(void *ctx);
    long (*now)(void *ctx);                         /* seconds, monotonic */
    ms_state *st;
} ms_ops;

/* The only open path for the text menu and viewer. Returns MS_SEND or MS_SPAWN_RESTART. */
int ms_open(const ms_ops *ops, const ms_req *r);

#endif
