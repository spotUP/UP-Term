/* sse -- server-sent events, read incrementally: "event:" and "data:"
 * lines (several data lines join with LF), comments (":") and other fields
 * ignored, an event dispatched at the blank line that ends it. Lines end
 * in LF, CR LF or CR; the byte stream may be split anywhere.
 * Portable C89, host-tested (tests/test_claude_json.c). */
#ifndef CL_SSE_H
#define CL_SSE_H

typedef void (*sse_fn)(void *u, const char *event, const char *data, long n);

typedef struct sse {
    char event[64];
    char *line;                 /* the line being read */
    long ll, lcap;
    char *data;                 /* the event's data so far */
    long dl, dcap;
    int has_data;
    int cr;                     /* the last byte was CR (a following LF is the same line end) */
    int oom;
    sse_fn fn;
    void *u;
} sse;

void sse_init(sse *s, sse_fn fn, void *u);
void sse_feed(sse *s, const char *p, long n);
void sse_free(sse *s);

#endif
