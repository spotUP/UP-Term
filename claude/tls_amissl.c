/* tls_amissl -- tls.h on AmiSSL 5 (OpenSSL 3 API through
 * amisslmaster.library), AmigaOS 3.x, 68020+.
 *
 * Built only with the AmiSSL SDK: make amiga AMISSL_SDK=<dir>, <dir> the
 * SDK's top directory (with include/ inside). NOT YET COMPILED FOR THE
 * AMIGA: the SDK was not on the build machine when this was written. The
 * OpenSSL half is syntax-checked on the host against OpenSSL 3
 * (CL_TLS_HOSTCHECK, see the Makefile's claude-tls-check); the AmiSSL
 * library setup follows the SDK's documented OpenAmiSSLTags() sequence and
 * is the part to check first when the SDK is in place.
 *
 * Verification is ON: SSL_VERIFY_PEER against AmiSSL's CA store
 * (AmiSSL:Certs, SSL_CTX_set_default_verify_paths), the host name checked
 * with SSL_set1_host, TLS 1.2 or later, SNI sent. A failed check ends the
 * handshake with the reason; there is no fallback to an unchecked link. */
#ifndef CL_TLS_HOSTCHECK
#include <exec/types.h>
#include <proto/exec.h>
#include <utility/tagitem.h> /* Tag: clib/amisslmaster_protos.h uses it unincluded */
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "tls.h"
#include "util.h"
#include "../tty/bmsg.h"

struct cl_tls {
    SSL *ssl;
};

static SSL_CTX *ctx;

#ifndef CL_TLS_HOSTCHECK
struct Library *AmiSSLMasterBase;
struct Library *AmiSSLBase;
struct Library *AmiSSLExtBase;
#endif

static void ssl_reason(char *err, int cap, const char *what)
{
    char buf[160];
    unsigned long e = ERR_get_error();
    cl_copy(err, what, cap);
    if (e) {
        ERR_error_string_n(e, buf, sizeof(buf));
        cl_cat(err, ": ", cap);
        cl_cat(err, buf, cap);
    }
}

int tls_init(void *socketbase, char *err, int cap)
{
#ifndef CL_TLS_HOSTCHECK
    AmiSSLMasterBase = OpenLibrary((STRPTR)"amisslmaster.library", AMISSLMASTER_MIN_VERSION);
    if (!AmiSSLMasterBase) {
        cl_copy(err, bmsg_amissl_missing(), cap);
        return TLS_ERROR;
    }
    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
                       AmiSSL_UsesOpenSSLStructs, FALSE,
                       AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                       AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                       AmiSSL_SocketBase, (ULONG)socketbase,
                       AmiSSL_ErrNoPtr, (ULONG)&errno,
                       TAG_DONE) != 0) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = 0;
        cl_copy(err, "AmiSSL is too old or did not open (AmiSSL 5 needed)", cap);
        return TLS_ERROR;
    }
#else
    (void)socketbase;
#endif
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        ssl_reason(err, cap, "TLS setup failed");
        tls_exit();
        return TLS_ERROR;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        ssl_reason(err, cap, "the certificate store (AmiSSL:Certs) did not load");
        tls_exit();
        return TLS_ERROR;
    }
    return 0;
}

void tls_exit(void)
{
    if (ctx)
        SSL_CTX_free(ctx);
    ctx = 0;
#ifndef CL_TLS_HOSTCHECK
    if (AmiSSLMasterBase) {
        if (AmiSSLBase)
            CloseAmiSSL();
        CloseLibrary(AmiSSLMasterBase);
    }
    AmiSSLMasterBase = 0;
    AmiSSLBase = 0;
    AmiSSLExtBase = 0;
#endif
}

cl_tls *tls_new(long sock, const char *host, char *err, int cap)
{
    cl_tls *t = (cl_tls *)malloc(sizeof(*t));
    if (!t || !ctx) {
        cl_copy(err, t ? "TLS is not set up" : "out of memory", cap);
        free(t);
        return 0;
    }
    t->ssl = SSL_new(ctx);
    if (!t->ssl || !SSL_set_fd(t->ssl, (int)sock) ||
        !SSL_set_tlsext_host_name(t->ssl, host) || !SSL_set1_host(t->ssl, host)) {
        ssl_reason(err, cap, "TLS session setup failed");
        tls_free(t);
        return 0;
    }
    return t;
}

static int want(cl_tls *t, int rc)
{
    int e = SSL_get_error(t->ssl, rc);
    if (e == SSL_ERROR_WANT_READ)
        return TLS_WANT_READ;
    if (e == SSL_ERROR_WANT_WRITE)
        return TLS_WANT_WRITE;
    return TLS_ERROR;
}

int tls_handshake(cl_tls *t, char *err, int cap)
{
    int rc = SSL_connect(t->ssl), w;
    long v;
    if (rc == 1) {
        v = SSL_get_verify_result(t->ssl);
        if (v != X509_V_OK) {
            cl_copy(err, "the server's certificate did not verify: ", cap);
            cl_cat(err, X509_verify_cert_error_string(v), cap);
            return TLS_ERROR;
        }
        return 0;
    }
    w = want(t, rc);
    if (w == TLS_ERROR) {
        v = SSL_get_verify_result(t->ssl);
        if (v != X509_V_OK) {
            cl_copy(err, "the server's certificate did not verify: ", cap);
            cl_cat(err, X509_verify_cert_error_string(v), cap);
        } else
            ssl_reason(err, cap, "the TLS handshake failed");
    }
    return w;
}

long tls_send(cl_tls *t, const char *b, long n)
{
    int rc = SSL_write(t->ssl, b, (int)n);
    return rc > 0 ? (long)rc : (long)want(t, rc);
}

long tls_recv(cl_tls *t, char *b, long cap)
{
    int rc = SSL_read(t->ssl, b, (int)cap);
    if (rc > 0)
        return rc;
    if (SSL_get_error(t->ssl, rc) == SSL_ERROR_ZERO_RETURN)
        return 0;
    return want(t, rc);
}

int tls_pending(cl_tls *t)
{
    return SSL_pending(t->ssl);
}

void tls_free(cl_tls *t)
{
    if (!t)
        return;
    if (t->ssl) {
        SSL_shutdown(t->ssl);
        SSL_free(t->ssl);
    }
    free(t);
}
