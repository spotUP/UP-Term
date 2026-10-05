/* json -- a small JSON reader and writer for the Claude client.
 *
 * Reader: no tree is built. A value is a slice of the text (jv); the
 * functions walk it in place. json_parse validates a whole text strictly
 * (RFC 8259 grammar; nesting deeper than JSON_DEPTH is refused, so there is
 * no recursion and no stack surprise on a 68k); the navigation functions
 * expect validated text. Strings decode to UTF-8 (\u escapes and
 * surrogate pairs; a lone surrogate becomes U+FFFD).
 *
 * Writer: a growing buffer. Strings are escaped; input bytes that are not
 * valid UTF-8 are taken as Latin-1 (the Amiga's own text) and converted,
 * so what the client sends is always valid UTF-8 JSON.
 * Portable C89, host-tested (tests/test_claude_json.c). */
#ifndef CL_JSON_H
#define CL_JSON_H

#define JSON_DEPTH 64

typedef struct jv {
    const char *p;
    long n;
} jv;

enum { J_BAD, J_OBJ, J_ARR, J_STR, J_NUM, J_TRUE, J_FALSE, J_NULL };

/* The length of the one value at p (no leading white space), validated;
 * -1 when malformed. */
long json_value(const char *p, long n);
/* p[0..n) is exactly one value with optional white space around it. */
int json_parse(const char *p, long n, jv *v);
int json_type(jv v);
/* The member of an object (the first with that key). 1 found, 0 not. */
int json_get(jv obj, const char *key, jv *out);
/* walking an object (key set) or an array (key 0) */
typedef struct jit {
    const char *p, *e;
} jit;
void json_iter(jv v, jit *it);
int json_next(jit *it, jv *key, jv *val);
/* the element count of an array, members of an object */
long json_count(jv v);

/* A string's decoded bytes into out (cap bytes, terminated, cut to fit):
 * the full decoded length, or -1 when v is not a string. */
long json_str(jv v, char *out, long cap);
/* the decoded string in malloc'ed memory (terminated, *len its length);
 * 0 when not a string or out of memory */
char *json_strdup(jv v, long *len);
/* does the string equal z (decoded)? */
int json_streq(jv v, const char *z);
long json_long(jv v, long def);

/* The length of the valid UTF-8 character at s (n > 0 bytes left), or 0
 * when the bytes are not one; *cp its code point. */
int json_utf8(const char *s, long n, unsigned long *cp);

typedef struct jw {
    char *p;                    /* always terminated */
    long n, cap;
    int oom;
} jw;

void jw_init(jw *w);
void jw_free(jw *w);
void jw_reset(jw *w);
void jw_raw(jw *w, const char *s, long n);
void jw_rawz(jw *w, const char *s);
/* a quoted, escaped string */
void jw_str(jw *w, const char *s, long n);
void jw_strz(jw *w, const char *s);
void jw_long(jw *w, long v);

#endif
