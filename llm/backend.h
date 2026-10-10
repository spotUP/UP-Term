/* backend -- one provider API behind one interface (amiga-pi plan B3):
 * the request head with its credential, the body from pi-ai messages, and
 * the decoder from the API's stream events to the neutral ones
 * (llm/event.h). llm/be_anthropic.c (anthropic-messages, shared with
 * C:Claude) and llm/be_openai_chat.c (openai-completions) implement it;
 * llm/request.c drives any of them.
 * Portable C89. */
#ifndef LLM_BACKEND_H
#define LLM_BACKEND_H

#include "llm.h"
#include "event.h"

/* openai-completions compatibility (pi-ai's compat flags; detection order:
 * the defaults for the endpoint, then the model's explicit values) */
enum { LC_UNSET = -1 };
typedef struct llm_compat {
    int max_tokens_field;       /* 1: "max_tokens", 0: "max_completion_tokens" */
    int usage_in_streaming;     /* stream_options.include_usage */
    int developer_role;         /* "developer" for the system prompt of a reasoning model */
    int tool_result_name;       /* the tool message carries the tool's name */
} llm_compat;

typedef struct llm_model {
    const char *id;             /* "claude-opus-5-5", "qwen2.5-coder:7b" */
    const char *api;            /* "anthropic-messages", "openai-completions" */
    const char *provider;       /* "anthropic", "openai", "ollama", ... */
    const char *base_url;       /* "https://api.anthropic.com/v1", "http://192.168.1.10:11434/v1" */
    int reasoning;              /* a reasoning model */
    long max_tokens;            /* the answer's cap */
    long context_window;
    long cost[4];               /* micro-dollars per million tokens: input, output, cacheRead, cacheWrite */
    llm_compat compat;          /* explicit values (LC_UNSET: detect) */
} llm_model;

/* what one request carries */
typedef struct llm_ctx {
    const llm_model *model;
    const char *system;         /* the system prompt, 0 none */
    const char *const *msgs;    /* pi-ai messages (JSON text) */
    const long *msg_n;
    int nmsg;
    const char *tools;          /* a JSON array of pi tools {name, description, parameters}, 0 none */
    long tools_n;
    long max_tokens;            /* 0: the model's */
} llm_ctx;

typedef struct llm_backend {
    const char *api;
    /* the header whose value is the credential (redacted in logs, never sent over plain http) */
    const char *secret_header;
    /* the URL path below base_url's path ("/messages", "/chat/completions") */
    const char *path;
    /* the request head: host and path from the URL, key 0 = none sent; its length, -1 too long */
    long (*head)(const char *host, int port, int tls, const char *path, const char *key,
                 const char *user_agent, long body_len, char *out, long cap);
    /* the request body: 0, -1 (out of memory, or a message it cannot send) */
    int (*body)(const llm_ctx *c, jw *out);
    /* a decoder: new state, one SSE event (event name, data) into the sink
     * (0, or -1 when the stream is malformed), free */
    void *(*dec_new)(void);
    int (*feed)(void *dec, const char *event, const char *data, long n, const llm_sink *k);
    void (*dec_free)(void *dec);
} llm_backend;

extern const llm_backend llm_be_anthropic;
extern const llm_backend llm_be_openai_chat;

/* the backend of an api name, 0 none */
const llm_backend *llm_backend_of(const char *api);
/* calls of each backend's feed since the start (the reachability tests' sentinel) */
extern long llm_feeds_anthropic, llm_feeds_openai;

#endif
