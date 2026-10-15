/* The telnet protocol of uptelnet (ledger A1.1), portable C89 with no OS
 * calls: option negotiation (RFC 854/855, with RFC 1143's states so a
 * request is answered once and never loops), NAWS (RFC 1073), TERMINAL-TYPE
 * (RFC 1091), BINARY (RFC 856), SGA and ECHO; IAC escaping both ways; and
 * the key login below (NL6..NL8, plan 2026-10-14-nas-claude-local.md).
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
#define TN_AUTH 37                      /* RFC 2941 AUTHENTICATION, here with the one private type below */
#define TN_NOPTS 40                     /* options above this are refused */

/* ---- The key login (uptelnet to tools/uptelnetd.py), the protocol both ends keep ----
 *
 * Why: the password crossed the LAN in clear at every start. With a key per
 * Amiga the reusable secret never crosses again: a challenge-response over
 * the telnet link, HMAC-SHA256 (llm/sha256.c, the code SigV4 uses; no new
 * crypto). It protects the login only, not the session (plain TCP as before).
 *
 * Framing: RFC 2941 (option AUTHENTICATION 37, commands IS 0, SEND 1,
 * REPLY 2) with one private auth-type byte 0x75 ("UPT1"). Every binary field
 * is a plain byte string in the subnegotiation, so a 255 in it is doubled
 * (IAC IAC) on the wire and undoubled by the receiver like any SB data.
 * K = the 32-byte key, ID = 1 to 16 ASCII characters (no NUL, no blank),
 * sn = server nonce 16 bytes (from the OS random source), cn = client nonce
 * 16 bytes (unique per start, not secret: the caller hashes EClock, the
 * date and its task address). HMAC(K, m) is HMAC-SHA256.
 *
 *  1 server  IAC DO AUTH, beside its other offers. A client with no key
 *            answers WONT (as it does for any option it does not know) and
 *            gets "Password:" exactly as before.
 *  2 client  with a key (or asking to be paired): IAC WILL AUTH.
 *  3 server  SB AUTH SEND 0x75 sn[16]
 *  4 client  SB AUTH IS 0x75 ID 0x00 cn[16] mac[32],
 *            mac = HMAC(K, "upt1-client" | sn | cn | ID).
 *            A client asking to be paired (no key yet, the key file is
 *            empty) sends ID empty and no mac: SB AUTH IS 0x75 0x00 cn[16].
 *  5 server  mac good: SB AUTH REPLY 0x75 ACCEPT smac[32],
 *            smac = HMAC(K, "upt1-server" | sn | cn); no Password prompt.
 *            The client checks smac in constant time; a bad one ends the
 *            line (TN_AUTH_BAD_SERVER: "not the NAS that paired with this
 *            Amiga"), nothing typed is ever sent to it.
 *            mac bad or ID unknown (revoked): SB AUTH REPLY 0x75 REJECT, then
 *            "Password:" as before (TN_AUTH_REJECTED); the server counts the
 *            failure in its rate limiter.
 *  6 server  pairing: after a good password from a client that asked in
 *            step 4 (empty ID) the server makes ID and K, stores them and
 *            sends SB AUTH REPLY 0x75 PAIR ID 0x00 K[32]. The client writes
 *            its key file (the paired callback) and goes on; the next start
 *            is step 5. A client that has a key ignores PAIR.
 *  7 client  the session daemon (plan phase C; wasabid started by uptelnet): after ACCEPT
 *            the client derives T = tn_session_key(K, sn, cn), gives it to the daemon, and
 *            announces the daemon's TCP port: SB AUTH IS 0x75 0x01 port[2, big endian]
 *            (tn_announce_agent; 0x01 cannot start an ID, which is ASCII; port 0: no
 *            daemon). The server records (peer, port, T) for the NAS relay (uptelnetd
 *            --agent-relay), which connects to the peer's port as a wasabi client with T.
 *
 * Key files: Amiga ENVARC:Claude/nas-key, one line "ID hexkey" (copied to
 * ENV:); NAS ~/.config/uptelnetd/clients, 0600, lines "ID hexkey date peer".
 * The wire shows sn, cn, mac, smac (one-time values) and, once, ID and K at
 * the pairing, which is as exposed as the one password that authorised it. */
#define TN_AUTH_IS 0
#define TN_AUTH_AGENT 1                 /* the first byte of the session daemon announcement, after the type */
#define TN_AUTH_SEND 1
#define TN_AUTH_REPLY 2
#define TN_AUTH_TYPE 0x75
#define TN_AUTH_REJECT 0
#define TN_AUTH_ACCEPT 1
#define TN_AUTH_PAIR 2
#define TN_ID_MAX 16
#define TN_KEY_LEN 32

/* where the key login stands (tn_auth) */
enum {
    TN_AUTH_OFF,                        /* no key given, or the server never asked */
    TN_AUTH_ASKED,                      /* WILL AUTH sent, waiting for SEND */
    TN_AUTH_SENT,                       /* IS sent, waiting for the verdict */
    TN_AUTH_OK,                         /* accepted, and the server proved itself */
    TN_AUTH_REJECTED,                   /* the server does not know the ID or the mac: the password follows */
    TN_AUTH_BAD_SERVER,                 /* the server's proof is wrong: close the line */
    TN_AUTH_PAIRED                      /* a key was handed over after the password */
};

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
    unsigned char sb[128];              /* a subnegotiation's bytes */
    int sblen, sbover;                  /* sbover: it did not fit, ignore it */
    int cr;                             /* NVT: the last byte shown was CR */
    unsigned char buf[512];             /* data for show(), batched */
    int blen;
    int esc, nparm;                     /* the CSI ? ... h/l being read (alternate screen) */
    int parm[4];
    int alt;                            /* the far side is on the alternate screen */
    /* the key login (tn_set_key) */
    int auth;                           /* TN_AUTH_* */
    int want_key, pairing;              /* a key (or a pairing request) was given; it is a pairing request */
    char id[TN_ID_MAX + 1];
    unsigned char key[TN_KEY_LEN];
    unsigned char cn[16];               /* ours */
    unsigned char sn[16];               /* the server's (set at SEND) */
    void (*event)(void *user, int state, const char *id, const unsigned char *key);
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
/* Offer the key login (call before tn_start; the server's DO AUTH is then
 * answered with WILL). id/key: the key file's pair; id 0 or "" asks to be
 * paired (key ignored). cn: this start's 16-byte nonce. event (0: none) is
 * called as the login moves to TN_AUTH_OK, _REJECTED, _BAD_SERVER or _PAIRED,
 * before the bytes that follow in the same read are shown (so a message can
 * come before "Password:"); for _PAIRED id and key are the new pair, which
 * the caller must write to its key file, else both are 0. */
void tn_set_key(tn *t, const char *id, const unsigned char *key, const unsigned char *cn,
                void (*event)(void *user, int state, const char *id, const unsigned char *key));
/* The key file's one line "ID hexkey": 1 with id (TN_ID_MAX + 1 bytes) and key filled, 0 when it
 * is not one (empty, a bad ID character, a short or non-hex key). Trailing blanks and a newline
 * are fine. And the same line made: id, a blank, 64 lower-case hex digits, a newline, a NUL
 * (out holds TN_ID_MAX + TN_KEY_LEN * 2 + 3 bytes); its length. */
int tn_key_parse(const char *text, char *id, unsigned char *key);
int tn_key_format(const char *id, const unsigned char *key, char *out);
/* TN_AUTH_*; at TN_AUTH_BAD_SERVER the caller closes the line (tn_recv
 * already ignores everything after it) */
int tn_auth(const tn *t);
/* 1 when a == b over n bytes, in time that does not depend on where they differ */
int tn_ct_eq(const unsigned char *a, const unsigned char *b, int n);
/* 1 when that side of the option is on */
int tn_us(const tn *t, int opt);
int tn_him(const tn *t, int opt);

/* The session key of the login's session daemon (wasabid): hex (64 digits and a NUL) of
 * HMAC-SHA256(K, "wasabi-session" | sn | cn), K the pairing key (klen bytes), sn the server's
 * nonce, cn the client's. uptelnetd derives the same string (tn_session_key twin), so the NAS
 * relay can authenticate to the daemon the Amiga started. hex[0] = 0 when sn and cn together
 * exceed 64 bytes. */
void tn_session_key(const unsigned char *k, int klen, const unsigned char *sn, int snlen, const unsigned char *cn,
                    int cnlen, char hex[65]);

/* Step 7: tell the server the session daemon's port (after TN_AUTH_OK; else nothing is sent).
 * Port 0 says there is none. */
void tn_announce_agent(tn *t, int port);

#endif
