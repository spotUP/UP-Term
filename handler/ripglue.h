/* ripglue.h -- RIP-7b glue between the engine's rip_block callback and ripxfer_receive. Pure C89,
 * no OS calls: the handler's worker supplies the chunk and send functions (ripglue_io), the host
 * tests supply sockets. The glue adds no protocol logic: ripglue_run is ripxfer_receive on a
 * zm_line whose getc drains the chunks the handler held. */
#ifndef RIPGLUE_H
#define RIPGLUE_H

#include "ripxfer.h"

#define RIPGLUE_NAME 64

typedef struct ripglue_io {
    /* The next chunk of the stream after the one used up (wait at most timeout_ms). Return 1 with
     * p and n set (the chunk stays valid until the next call), 0 on timeout, -1 when the line is
     * gone (cancel, window closing). */
    int (*next)(void *user, int timeout_ms, const unsigned char **p, long *n);
    /* Bytes to send to the BBS (the client reads them from the console input). 0 when sent. */
    int (*send)(void *user, const unsigned char *b, long n);
    void *user;
} ripglue_io;

typedef struct ripglue {
    ripglue_io io;
    const unsigned char *cur; /* the chunk being read */
    long len;
    char name[RIPGLUE_NAME];  /* the name the BBS gave in the RIP command */
    long armed, refused;
} ripglue;

void ripglue_init(ripglue *g, const ripglue_io *io);
/* The rip_block callback fired. 1 = receive this block (mode 0, the BBS sends: the handler stops the
 * engine and starts the worker); 0 = refuse (mode 1 is an upload, out of scope). The protocol id is
 * not checked here: ids are unverified (plan U2); zm_receive rejects a stream that is not ZMODEM. */
int ripglue_arm(ripglue *g, int mode, int proto, const char *name);
/* The bytes of the triggering buffer after the command (vtwin_write returned fed < len). */
void ripglue_begin(ripglue *g, const unsigned char *rest, long n);
/* The zm_line of this glue; l->user is g. */
void ripglue_line(ripglue *g, zm_line *l);
/* The worker body. Returns zm_receive's ZM_* result. */
int ripglue_run(ripglue *g, vt_dl_sink sink, const char *host, int *delivered);

#endif
