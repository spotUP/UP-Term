/* sha256 -- SHA-256 (FIPS 180-4) for the LLM layer: SigV4's hashes and HMAC (llm/sigv4.c, Bedrock), and amiga-pi's MCP
 * tool-name hash suffix (pi's createMcpToolName). It was amiga-pi/sha256.c until Phase M5. Portable C89 (32-bit
 * arithmetic in unsigned long, masked): no library to open on the Amiga, the same code on the host.
 * HMAC-SHA256 (RFC 2104) is here too. */
#ifndef LLM_SHA256_H
#define LLM_SHA256_H

typedef struct llm_sha256 {
    unsigned long h[8];
    unsigned long len_lo, len_hi;   /* bytes so far */
    unsigned char buf[64];
    int bn;
} llm_sha256;

void llm_sha256_init(llm_sha256 *c);
void llm_sha256_add(llm_sha256 *c, const void *p, long n);
void llm_sha256_end(llm_sha256 *c, unsigned char out[32]);
/* the digest of p[0..n) as 64 lower-case hex characters, terminated */
void llm_sha256_hex(const void *p, long n, char out[65]);

/* HMAC-SHA256 of msg[0..mn) under key[0..kn) (RFC 2104) */
void llm_hmac_sha256(const void *key, long kn, const void *msg, long mn, unsigned char out[32]);

#endif
