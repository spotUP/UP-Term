/* tls -- TLS over a connected bsdsocket socket, for net_amiga.c.
 *
 * tls_amissl.c is AmiSSL 5 (OpenSSL 3 through amisslmaster.library): SNI,
 * the certificate chain verified against AmiSSL's CA store and the name
 * checked; a failed check is an error, never a silent downgrade. It is
 * built only when the AmiSSL SDK is given (make amiga AMISSL_SDK=<dir>).
 * tls_none.c, the default, refuses: an https URL then fails with a
 * message that says the build has no TLS. The socket is non-blocking;
 * TLS_WANT_READ / TLS_WANT_WRITE ask the caller to wait on it (WaitSelect,
 * with Ctrl+C) and call again. */
#ifndef CL_TLS_H
#define CL_TLS_H

#define TLS_ERROR      (-1)
#define TLS_WANT_READ  (-4)
#define TLS_WANT_WRITE (-5)
#define TLS_BREAK      (-6)     /* the idle hook asked to stop */

typedef struct cl_tls cl_tls;

/* The libraries, once per run: 0, or TLS_ERROR with err filled.
 * socketbase is the open bsdsocket.library. */
int tls_init(void *socketbase, char *err, int cap);
void tls_exit(void);

/* A session on sock for host (SNI and the name check). 0 when out of
 * memory or without TLS (err says which). */
cl_tls *tls_new(long sock, const char *host, char *err, int cap);
/* One step of the handshake: 0 done, TLS_WANT_*, or TLS_ERROR (err says
 * why: the certificate, the name, the protocol). idle (0 none) is called
 * between the steps the library computes in one go -- the key exchange,
 * each certificate of the chain, the server's signature: over a second
 * each on a 68020 -- nonzero from it ends the handshake with TLS_BREAK. */
int tls_handshake(cl_tls *t, int (*idle)(void *iu), void *iu, char *err, int cap);
/* > 0 bytes, 0 closed, TLS_WANT_*, TLS_ERROR */
long tls_send(cl_tls *t, const char *b, long n);
long tls_recv(cl_tls *t, char *b, long cap);
/* bytes already decrypted inside the session (no need to wait) */
int tls_pending(cl_tls *t);
void tls_free(cl_tls *t);
/* n secure random bytes (AmiSSL's generator; tls_init must have succeeded): 0, or TLS_ERROR (no TLS in this build) */
int tls_random(unsigned char *buf, long n);

#endif
