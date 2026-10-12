/* backend -- one provider API behind one interface (amiga-pi plan B3):
 * the request head with its credential, the body from pi-ai messages, and
 * the decoder from the API's stream events to the neutral ones
 * (llm/event.h). llm/be_anthropic.c (anthropic-messages, shared with
 * C:Claude) and llm/be_openai_chat.c (openai-completions) and llm/be_openai_responses.c
 * (openai-responses) implement it;
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
    int reasoning_effort;       /* supportsReasoningEffort: the reasoning_effort field */
    int thinking_as_text;       /* requiresThinkingAsText: a thinking block goes back as text */
    int strict_tools;           /* supportsStrictMode (openai-responses): tools carry "strict":false */
    /* anthropic-messages (pi-ai getAnthropicCompat; LC_UNSET: pi's default) */
    int mid_effort;             /* supportsMidConvoEffort: adaptive thinking bound to the level sent as system messages */
    int mid_system;             /* supportsMidConvoSystemMessages: later system messages stay in the transcript */
    int mid_tools;              /* supportsMidConvoToolChanges: tool changes as tool_addition / tool_removal blocks */
    int eager_tools;            /* supportsEagerToolInputStreaming (default on) */
    int cache_tools;            /* supportsCacheControlOnTools (default on) */
    int empty_signature;        /* allowEmptySignature: an unsigned thinking block keeps an empty signature */
} llm_compat;

/* pi's thinking levels (ModelThinkingLevel), in order; thinking_map[] is pi's
 * thinkingLevelMap: a provider value, null (the level is unsupported) or unset */
enum { LT_OFF, LT_MINIMAL, LT_LOW, LT_MEDIUM, LT_HIGH, LT_XHIGH, LT_MAX, LT_N };
enum { LTM_UNSET, LTM_STR, LTM_NULL };
typedef struct llm_tmap {
    int kind;                   /* LTM_* */
    const char *val;            /* LTM_STR */
} llm_tmap;

typedef struct llm_model {
    const char *id;             /* "claude-opus-5-5", "qwen2.5-coder:7b" */
    const char *api;            /* "anthropic-messages", "openai-completions" */
    const char *provider;       /* "anthropic", "openai", "ollama", ... */
    const char *base_url;       /* "https://api.anthropic.com", "http://192.168.1.10:11434/v1" */
    int reasoning;              /* a reasoning model */
    long max_tokens;            /* the answer's cap */
    long context_window;
    long cost[4];               /* micro-dollars per million tokens: input, output, cacheRead, cacheWrite */
    llm_compat compat;          /* explicit values (LC_UNSET: detect) */
    /* the registry's fields (llm/registry.h); a model made by hand leaves them 0 */
    const char *name;           /* display name, 0: the id */
    int image;                  /* input includes images */
    int adaptive;               /* compat.forceAdaptiveThinking (anthropic-messages) */
    llm_tmap thinking_map[LT_N];
    const char *headers;        /* a JSON object {"Name":"value"} of extra request headers, 0 none */
    const char *key;            /* models.json apiKey, unresolved (a config value), 0 none */
    int auth_header;            /* models.json authHeader: also send "Authorization: Bearer <key>" */
} llm_model;

/* a model with nothing set: the compat flags LC_UNSET (detect), the rest 0 */
void llm_model_init(llm_model *m);

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
    int thinking;               /* the thinking level, LT_*; clamped to the model's here (llm_think_clamp) */
    const char *session_id;     /* the session's id (pi's sessionId: openai-responses prompt_cache_key and headers), 0 none */
} llm_ctx;

typedef struct llm_backend {
    const char *api;
    /* the header whose value is the credential (redacted in logs, never sent over plain http) */
    const char *secret_header;
    /* the URL path below base_url (pi: "/v1/messages" on anthropic-messages, "/chat/completions", "/responses") */
    const char *path;
    /* the request head: host and path from the URL, key 0 = none sent; its length, -1 too long */
    long (*head)(const char *host, int port, int tls, const char *path, const char *key,
                 const char *user_agent, long body_len, char *out, long cap);
    /* the request body: 0, -1 (out of memory, or a message it cannot send) */
    int (*body)(const llm_ctx *c, jw *out);
    /* extra header lines this request needs ("Name: value\r\n" ...) into out: its length, 0 none; 0 pointer none */
    long (*xhead)(const llm_ctx *c, char *out, long cap);
    /* a decoder: new state, one SSE event (event name, data) into the sink
     * (0, or -1 when the stream is malformed), free */
    void *(*dec_new)(void);
    int (*feed)(void *dec, const char *event, const char *data, long n, const llm_sink *k);
    void (*dec_free)(void *dec);
} llm_backend;

extern const llm_backend llm_be_anthropic;
extern const llm_backend llm_be_openai_chat;
extern const llm_backend llm_be_openai_responses;

/* the thinking level an anthropic-messages request records on its answer (pi: providerThinkingLevel, set when the
 * model's supportsMidConvoEffort is on): "low".."max", 0 when the model does not manage the effort */
const char *llm_ant_effort(const llm_model *m, int thinking);
/* the backend of an api name, 0 none */
const llm_backend *llm_backend_of(const char *api);
/* calls of each backend's feed since the start (the reachability tests' sentinel) */
extern long llm_feeds_anthropic, llm_feeds_openai, llm_feeds_responses;

#endif
