/* conv -- the conversation: an append-only list of messages, each kept as
 * the exact JSON content array that went to or came from the API, the
 * request body assembled from it, and the usage and cost so far.
 *
 * Nothing already answered is ever rewritten (Claude Opus 5.5 checks that
 * replayed thinking blocks sit in an unchanged history). The only change
 * to an existing message is to a trailing, unanswered user message: a new
 * prompt is added to it as one more text block, and conv_rollback can undo
 * that, or drop messages, back to a mark -- both only ever touch what
 * comes after the last assistant turn.
 * Portable C89, host-tested (tests/test_claude_stream.c). */
#ifndef CL_CONV_H
#define CL_CONV_H

#include "json.h"

typedef struct cl_msg {
    int user;                   /* 1 user, 0 assistant */
    char *json;                 /* the content array, "[...]" */
    long n;
} cl_msg;

typedef struct cl_conv {
    cl_msg *m;
    int n, cap;
    long requests;
    long in_tok, out_tok, cache_w, cache_r;
    unsigned long cost_micro;   /* US dollars * 1e6, the priced requests */
    int unpriced;               /* requests on a model without a price here */
} cl_conv;

typedef struct cl_mark {
    int n;
    long last;                  /* the length of message n-1 then */
} cl_mark;

typedef struct cl_opts {
    const char *model;
    const char *effort;         /* "" sends none */
    long max_tokens;
    const char *system;         /* the system prompt text */
    const char *tools;          /* the tools array JSON, "" none */
    int no_tools;               /* tool_choice none: the tools stay listed (the
                                 * cached prefix stays the same), none is called */
} cl_opts;

void conv_init(cl_conv *c);
void conv_free(cl_conv *c);
void conv_clear(cl_conv *c);
/* a whole message (content array JSON): 0, -1 out of memory */
int conv_add(cl_conv *c, int user, const char *json, long n);
/* a prompt: a new user message, or one more text block on a trailing
 * unanswered user message (a tool_result message left by a cancel) */
int conv_add_user_text(cl_conv *c, const char *s, long n);
cl_mark conv_mark(const cl_conv *c);
void conv_rollback(cl_conv *c, cl_mark m);
/* the request body */
int conv_body(const cl_conv *c, const cl_opts *o, jw *out);
/* what a model takes in the body: CAP_EFFORT output_config.effort,
 * CAP_ADAPTIVE thinking {type adaptive, display summarized}; a field a
 * model rejects is left out for it */
#define CAP_EFFORT   1
#define CAP_ADAPTIVE 2
int conv_caps(const char *model);
/* the anthropic-beta header for a model ("" none): server-side
 * fallbacks where the model takes them */
const char *conv_beta(const char *model);
/* the whole history as a JSON messages array (/save) */
int conv_messages(const cl_conv *c, jw *out);

/* US dollars per million tokens * 100 (input, output, cache read, cache
 * write). 0 when the model has no price here. */
typedef struct cl_price {
    long in, out, cread, cwrite;
} cl_price;
int conv_price(const char *model, cl_price *p);
/* one request's usage, priced on the model that answered */
void conv_usage(cl_conv *c, const char *model, long in, long out, long cache_w, long cache_r);
/* "$0.0123" (4 decimals) */
void conv_dollars(unsigned long micro, char *out, long cap);

#endif
