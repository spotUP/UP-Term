/* sigv4 -- AWS Signature Version 4 (Phase M5): the Authorization header of a request to an AWS API (Amazon Bedrock's
 * converse-stream), per "Signing AWS API requests" and checked against AWS's published test suite (the aws-sig-v4
 * suite as awslabs/aws-c-auth carries it, tests/pi/sigv4: all 38 cases, signed in the header form and in the query
 * (presigned) form).
 *
 * The request is given as it goes on the wire: the path (not encoded again by the caller: the canonical form encodes
 * every byte but the unreserved ones and "/" once more, so an id with a ":" written %3A on the wire is signed as %253A),
 * the query as written, the headers to sign (any case, values as sent, a header written twice joins with ","), the body.
 * HMAC-SHA256 is llm/sha256.c's (no library: the same code on the 68k and the host).
 * Portable C89, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_SIGV4_H
#define LLM_SIGV4_H

#include "../claude/json.h"
#include "event.h"
#include "llm.h"

typedef struct llm_sig_hdr {
    const char *name, *value;
} llm_sig_hdr;

typedef struct llm_sigv4 {
    const char *method;             /* "POST" */
    const char *path;               /* "/model/x%3Ay/converse-stream"; "" is "/" */
    const char *query;              /* "a=1&b=2" as written, 0 or "" none */
    const llm_sig_hdr *hdr;         /* the headers to sign (Host among them) */
    int nhdr;
    const char *body;
    long bn;
    const char *access, *secret;
    const char *token;              /* the session token, 0 none */
    const char *region, *service;   /* "us-east-1", "bedrock" */
    const char *amzdate;            /* "YYYYMMDDTHHMMSSZ" (llm_sigv4_date) */
    int normalize;                  /* remove "." and ".." segments and repeated "/" from the path (every service but S3) */
    int sign_body;                  /* header form: x-amz-content-sha256 is added and signed */
    int sign_token;                 /* the token is part of what is signed (1; 0: it travels beside the signature) */
    int query_form;                 /* presigned: X-Amz-* in the query, x-amz-date not a header */
    long expires;                   /* query form: X-Amz-Expires seconds */
} llm_sigv4;

typedef struct llm_sigout {
    jw creq;                        /* the canonical request */
    jw sts;                         /* the string to sign */
    jw shdr;                        /* the signed header names, ";" joined */
    jw auth;                        /* header form: the Authorization value; query form: the X-Amz-* query to append */
    char sig[65];                   /* the signature, 64 hex characters */
    char payload[65];               /* the body's SHA-256 */
} llm_sigout;

void llm_sigout_init(llm_sigout *o);
void llm_sigout_free(llm_sigout *o);
/* 0, -1 out of memory */
int llm_sigv4_sign(const llm_sigv4 *in, llm_sigout *o);
/* "YYYYMMDDTHHMMSSZ" for seconds since 1970 (out holds 17 bytes) */
void llm_sigv4_date(llm_ts t, char *out);

/* UriEncode (also encodeURIComponent plus !'()* as the SDK writes a path segment): every byte but A-Z a-z 0-9 - . _ ~ as %XX,
 * "/" kept when slash; appended to w */
void llm_uri_encode(jw *w, const char *s, long n, int slash);

#endif
