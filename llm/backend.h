/* backend -- one provider API behind one interface (amiga-pi plan B3):
 * the request head with its credential, the body from pi-ai messages, and
 * the decoder from the API's stream events to the neutral ones
 * (llm/event.h). llm/be_anthropic.c (anthropic-messages, shared with
 * C:Claude) and llm/be_openai_chat.c (openai-completions) and llm/be_openai_responses.c
 * (openai-responses), llm/be_azure.c (azure-openai-responses) and llm/be_google.c
 * (google-generative-ai) and llm/be_bedrock.c (bedrock-converse-stream) implement it;
 * llm/request.c drives any of them.
 * Portable C89. */
#ifndef LLM_BACKEND_H
#define LLM_BACKEND_H

#include "llm.h"
#include "event.h"

/* compat.allowedFallbackModels (anthropic-messages; at most 3, pi's schema): the models the server may answer with
 * instead (server-side fallback), each with its own price for the usage cost */
#define LLM_FALLBACKS 3
typedef struct llm_fallback {
    const char *provider, *model;
    long cost[4];               /* micro-dollars per million tokens: input, output, cacheRead, cacheWrite */
} llm_fallback;

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
    int store;                  /* supportsStore: the body carries "store":false (pi: every host not listed as non-standard) */
    int strict_tools;           /* supportsStrictMode (openai-completions, openai-responses) / supportsStrictTools (anthropic-messages): tools carrying constrainedSampling go out strict (llm/constrain.h) */
    /* anthropic-messages (pi-ai getAnthropicCompat; LC_UNSET: pi's default) */
    int mid_effort;             /* supportsMidConvoEffort: adaptive thinking bound to the level sent as system messages */
    int mid_system;             /* supportsMidConvoSystemMessages: later system messages stay in the transcript (anthropic-messages, openai-responses) */
    int add_tools;              /* supportsAdditionalTools (openai-responses): later tool additions go in the request as additional_tools items */
    int mid_tools;              /* supportsMidConvoToolChanges: tool changes as tool_addition / tool_removal blocks */
    int eager_tools;            /* supportsEagerToolInputStreaming (default on) */
    int cache_tools;            /* supportsCacheControlOnTools (default on) */
    int empty_signature;        /* allowEmptySignature: an unsigned thinking block keeps an empty signature */
    llm_fallback fb[LLM_FALLBACKS];     /* allowedFallbackModels: the request names them ("fallbacks", the server-side-fallback beta) */
    int nfb;
} llm_compat;

/* pi's thinking levels (ModelThinkingLevel), in order; thinking_map[] is pi's
 * thinkingLevelMap: a provider value, null (the level is unsupported) or unset */
enum { LT_OFF, LT_MINIMAL, LT_LOW, LT_MEDIUM, LT_HIGH, LT_XHIGH, LT_MAX, LT_N };
enum { LTM_UNSET, LTM_STR, LTM_NULL };
typedef struct llm_tmap {
    int kind;                   /* LTM_* */
    const char *val;            /* LTM_STR */
} llm_tmap;

/* amazon-bedrock credentials and endpoint (llm_cfg_endpoint fills it from the environment: pi's api/bedrock-converse-stream.ts) */
typedef struct llm_aws {
    char region[48];            /* the signing region */
    char cfg_region[48];        /* AWS_REGION / AWS_DEFAULT_REGION as set, "" none (GovCloud's rule reads it) */
    char access[130], secret[130];  /* AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY; "" none */
    char token[4100];           /* AWS_SESSION_TOKEN, "" none */
    int skip_auth;              /* AWS_BEDROCK_SKIP_AUTH=1: sign with dummy keys (a proxy that checks nothing) */
} llm_aws;

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
    /* azure-openai-responses (llm_cfg_endpoint): the api-version query and the deployment named as the request's
     * model; 0: "v1" and the id */
    const char *api_version, *deployment;
    /* bedrock-converse-stream (llm_cfg_endpoint): the credentials and region; 0: none */
    const llm_aws *aws;
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
    long max_tokens;            /* 0: the model's; the request sends it clamped to the context window (llm/estimate.h) */
    long est_tokens;            /* estimateContextTokens of the transcript the request answers (llm_estimate_context), 0 when unknown */
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
    /* pi-ai's error text when the stream ends without the API's terminal event ("OpenAI Responses stream ended before
     * a terminal response event"); 0: the generic one */
    const char *cut;
    /* the request path below the base URL's own path, when it depends on the request (google: the model's id in it,
     * azure: the api-version query); into out: its length, -1 too long. 0 pointer: path */
    long (*route)(const llm_ctx *c, char *out, long cap);
    /* the stream's end, once the response ended cleanly: for an API whose stream carries no terminal event (google:
     * the answer is complete when the body ends). Emits the closing events; 0 pointer: none */
    void (*fin)(void *dec, const llm_sink *k);
    /* the headers of an authentication that needs the finished request (AWS Signature Version 4: the body's hash is in
     * it): into out as "Name: value\r\n" lines, their length, 0 none, -1 with the reason in err. host, port, tls and
     * path as sent in the head; bearer: a bearer credential is configured (the head sends it, or withholds it on plain
     * http): nothing is signed then. 0 pointer: none */
    long (*sign)(const llm_ctx *c, const char *host, int port, int tls, const char *path, const char *body, long bn,
                 int bearer, char *out, long cap, char *err, long errcap);
    /* the text of a failed HTTP answer (status, the x-amzn-errortype header or "", the body) for an API whose errors the
     * SDK words its own way; 0 pointer: "HTTP <status>: <message>" */
    void (*http_err)(int status, const char *errtype, const char *body, long bn, char *out, long cap);
} llm_backend;

extern const llm_backend llm_be_anthropic;
extern const llm_backend llm_be_openai_chat;
extern const llm_backend llm_be_openai_responses;
extern const llm_backend llm_be_azure;
extern const llm_backend llm_be_google;
extern const llm_backend llm_be_bedrock;

/* the thinking level an anthropic-messages request records on its answer (pi: providerThinkingLevel, set when the
 * model's supportsMidConvoEffort is on): "low".."max", 0 when the model does not manage the effort */
const char *llm_ant_effort(const llm_model *m, int thinking);
/* does the model take the transcript's system messages mid-conversation (pi-ai supportsMidConvoSystemMessages on
 * anthropic-messages or openai-responses): its requests carry them in place, the first as the initial one */
int llm_mid_system(const llm_model *m);
/* the pieces of the Responses backend that azure-openai-responses shares (llm/be_azure.c): the body for the model
 * named `name` on the wire, and the decoder; strict: the tools carry their strict flag */
int llm_responses_body(const llm_ctx *x, jw *w, const char *name, int strict);
void *llm_responses_new(void);
int llm_responses_feed(void *dec, const char *event, const char *data, long n, const llm_sink *k);
void llm_responses_free(void *dec);
/* google-shared.ts requiresToolCallId: the model's function calls and responses carry ids (their ids are normalised) */
int llm_google_call_ids(const char *model_id);
/* the backend of an api name, 0 none */
const llm_backend *llm_backend_of(const char *api);
/* calls of each backend's feed since the start (the reachability tests' sentinel) */
extern long llm_feeds_anthropic, llm_feeds_openai, llm_feeds_responses, llm_feeds_azure, llm_feeds_google, llm_feeds_bedrock;

/* the wall clock the generated tool-call ids read (google: pi's `${name}_${Date.now()}_${++counter}`); the host sets it
 * before a request (now: seconds and ms since 1970); unset, the ids read time() */
void llm_set_wall(llm_ts (*now)(void *u), void *u);
/* the clock as set (llm_set_wall), else time(): the wall clock now */
llm_ts llm_wall(void);
/* pi's module-level counter of generated ids: set to n (the tests start each case at 0) */
void llm_google_counter(long n);

#endif
