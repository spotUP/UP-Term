/* The telnet protocol of uptelnet (ledger A1.1), portable C89 with no OS
 * calls: option negotiation (RFC 854/855, with RFC 1143's states so a
 * request is answered once and never loops), NAWS (RFC 1073), TERMINAL-TYPE
 * (RFC 1091), BINARY (RFC 856), SGA and ECHO; IAC escaping both ways.
 * Host-tested (tests/test_telnet.c); net/uptelnet.c is the Amiga side
 * (bsdsocket.library and the console).
 *
 * The caller hands in what the socket received (tn_recv) and what was
 * typed (tn_send); the module calls `out` with bytes for the socket and
 * `show` with bytes for the terminal. Its state survives any split of the
 * stream: an IAC sequence may arrive one byte per call. */
#ifndef TN_H
#define TN_H

#define TN_IAC 255
#define TN_DONT 254
#define TN_DO 253
#define TN_WONT 252
#define TN_WILL 251
#define TN_SB 250
#define TN_GA 249
#define TN_NOP 241
#define TN_SE 240

#define TN_BINARY 0
#define TN_ECHO 1
#define TN_SGA 3
#define TN_TTYPE 24
#define TN_NAWS 31
#define TN_NOPTS 40                     /* options above this are refused */

#define TN_TTYPE_IS 0
#define TN_TTYPE_SEND 1

/* RFC 1143 option states, for each side */
#define TN_NO 0
#define TN_YES 1
#define TN_WANTNO 2
#define TN_WANTYES 3

typedef struct tn {
    void (*out)(void *user, const unsigned char *b, int n);  /* to the socket */
    void (*show)(void *user, const unsigned char *b, int n); /* to the terminal */
    void *user;
    char term[41];                      /* TERMINAL-TYPE answer */
    int cols, rows;                     /* the window, for NAWS */
    unsigned char us[TN_NOPTS];         /* our side: WILL/WONT */
    unsigned char him[TN_NOPTS];        /* the server's side: DO/DONT */
    int st;                             /* receive parser state */
    int verb;                           /* DO/DONT/WILL/WONT being read */
    unsigned char sb[64];               /* a subnegotiation's bytes */
    int sblen;
    int cr;                             /* NVT: the last byte shown was CR */
    unsigned char buf[512];             /* data for show(), batched */
    int blen;
    int esc, nparm;                     /* the CSI ? ... h/l being read (alternate screen) */
    int parm[4];
    int alt;                            /* the far side is on the alternate screen */
} tn;

void tn_init(tn *t, const char *term, int cols, int rows,
             void (*out)(void *, const unsigned char *, int),
             void (*show)(void *, const unsigned char *, int), void *user);
/* Our opening offers: WILL NAWS, WILL TTYPE, WILL BINARY, DO BINARY, DO SGA. */
void tn_start(tn *t);
/* Bytes from the socket: data goes to show(), commands are answered. */
void tn_recv(tn *t, const unsigned char *b, int n);
/* Typed bytes: IAC doubled; CR NUL for a CR unless we send BINARY; echoed
 * through show() while the server does not echo (ECHO not on). */
void tn_send(tn *t, const unsigned char *b, int n);
/* The window's size: sent (NAWS) when it changed and NAWS is on. */
void tn_size(tn *t, int cols, int rows);
/* What a remote program may have left switched on when the connection
 * ends without its own goodbye (tmux, Claude Code, vim): the alternate
 * screen, a scroll region, hidden cursor, mouse and focus reports,
 * bracketed paste, application keys, synchronized output. Written to the
 * console after the session so the Shell gets its screen back, as ssh and
 * mosh clients leave it. The cursor stays where it is; the main screen
 * comes back only when the far side switched to the alternate one. */
const char *tn_goodbye(const tn *t);
/* 1 when that side of the option is on */
int tn_us(const tn *t, int opt);
int tn_him(const tn *t, int opt);

#endif
