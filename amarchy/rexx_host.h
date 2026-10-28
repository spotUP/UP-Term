/* rexx_host: the one ARexx module of Amarchy and AmTile. The server side is a named public message port that
 * answers RXCOMM messages through a callback; the client side sends one command line to a named port and waits for
 * the reply. Both programs link this file; neither carries a copy. */
#ifndef REXX_HOST_H
#define REXX_HOST_H

#define REXX_HOST_OUT 96   /* longest result string a callback may write (including the NUL) */

struct MsgPort;
struct Library;

/* One command: cmd in, the result string in out[REXX_HOST_OUT] (left empty for none), the return code returned. */
typedef long (*rexx_host_fn)(void *ud, const char *cmd, char *out);

struct rexx_host {
    struct MsgPort *port;   /* 0 when the name was taken or no port could be made */
    struct Library *lib;    /* rexxsyslib.library, 0 when absent */
};

/* 1 when a public port of that name exists now. */
int rexx_port_exists(const char *name);

/* Open rexxsyslib.library and the public port `name` (the string must outlive the host). Returns 1 when this
 * host owns the port, 0 when the name already exists (single instance) or the port could not be made. The
 * library stays open either way until rexx_host_close. */
int rexx_host_open(struct rexx_host *h, const char *name);

/* Signal mask to Wait() on; 0 when the host owns no port. */
unsigned long rexx_host_sigmask(const struct rexx_host *h);

/* Answer every pending message: fn runs once per command, its rc goes in rm_Result1 and a non-empty result string
 * goes back as an argstring when the sender asked for one (RXFF_RESULT). */
void rexx_host_serve(struct rexx_host *h, rexx_host_fn fn, void *ud);

/* Remove the port, answer what was still queued (through fn), delete the port, close the library. */
void rexx_host_close(struct rexx_host *h, rexx_host_fn fn, void *ud);

/* Client: send `line` to the port `portname` as an RXCOMM message and wait for the reply. Returns the return code,
 * or -1 when the port or rexxsyslib.library is absent. When result is set, the reply's argstring is copied into
 * result[cap] (else ""). */
long rexx_send(const char *portname, const char *line, char *result, int cap);

#endif
