/* llm -- pi-ai's provider-neutral message model in C (amiga-pi plan B1).
 *
 * A message is kept as its JSON text in pi-ai's shape (packages/ai types:
 * user, assistant, toolResult, system), so sessions, RPC and handoff read
 * and write the same bytes. Content blocks: text{text, textSignature?},
 * thinking{thinking, thinkingSignature?, redacted?}, image{data, mimeType},
 * toolCall{id, name, arguments, thoughtSignature?}, and raw{api, json} for a
 * provider block pi-ai does not model (Anthropic's server tools, fallback),
 * sent back only to the same api.
 *
 * The writers emit one canonical key order; llm_msg_canon reads any
 * pi-shaped message and writes it in that order (the round trip).
 * Timestamps are milliseconds since 1970 as pi-ai writes them, held as
 * seconds + milliseconds (a 68k long cannot hold the milliseconds); cost is
 * in micro-dollars and written as a decimal number of dollars.
 * Portable C89 over claude/json.h, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_H
#define LLM_H

#include "../claude/json.h"

enum { LR_BAD = -1, LR_USER, LR_ASSISTANT, LR_TOOLRESULT, LR_SYSTEM };
/* pi-ai's StopReason; LS_PENDING is the in-flight message's (an assistant message while it streams: message_start of
 * the JSON and RPC modes). llm_m_assistant writes it, a stored message never has it: llm_stop_of does not read it */
enum { LS_NONE, LS_STOP, LS_LENGTH, LS_TOOLUSE, LS_ERROR, LS_ABORTED, LS_PENDING };

typedef struct llm_ts {
    long sec;                   /* seconds since 1970-01-01 UTC */
    int ms;                     /* 0..999 */
} llm_ts;

typedef struct llm_usage {
    long input, output, cache_read, cache_write;
    long cache_write_1h;        /* cacheWrite1h: the 1 hour part of cache_write; written only when has_1h */
    int has_1h;                 /* anthropic streams set it, other providers' usage has no such key */
    long total;                 /* totalTokens as the API reported it (bedrock: its own count, which leaves the cache tokens out),
                                 * written when has_total; else the sum of the four counts */
    int has_total;
    long reasoning;             /* usage.reasoning (pi-ai 1.1.0): a subset of output, written only when has_reasoning */
    int has_reasoning;          /* openai-completions and openai-responses always set it, anthropic when the stream carries
                                 * output_tokens_details.thinking_tokens; written before totalTokens, anthropic's after
                                 * cacheWrite1h behind the cost */
    int combined;               /* the sum of two usages (pi's combineUsage, a compaction of a split turn): cacheWrite1h
                                 * stands after cacheWrite there, not after the cost */
    /* cost in micro-dollars: input, output, cacheRead, cacheWrite (total is their sum) */
    long cost[4];
} llm_usage;

typedef struct llm_meta {       /* an assistant message's own fields */
    const char *api;            /* "anthropic-messages", "openai-completions", ... */
    const char *provider;       /* "anthropic", "openai", "ollama", ... */
    const char *model;
    const char *response_model; /* responseModel: the model the server says it used, when not model; 0 none */
    llm_usage usage;
    int stop;                   /* LS_* */
    const char *error;          /* errorMessage, 0 none */
    const char *level;          /* providerThinkingLevel: the effort a managed-effort anthropic request carried, 0 none */
    const char *response_id;    /* responseId: the provider's id of the answer (anthropic message id, chat completion id, response id), 0 none */
    const char *raw_stop;       /* rawStopReason: the provider's stop_reason / finish_reason / status as sent, 0 none */
    const char *thinking_level; /* thinkingLevel: the session's level the request was made at, "off" included; 0 none (the
                                 * agent loop adds it to the finished message, pi-agent-core agent-loop.js) */
    const char *usage_json;     /* the usage object as the server sent it (pi-messages: verbatim, its own cost and extra keys), 0: written from usage */
    long usage_json_n;
    const char *rewrite;        /* pi-messages: the gateway's rewrite impact object, kept as the diagnostic pi_messages_rewrite; 0 none */
    long rewrite_n;
    llm_ts rewrite_ts;          /* the diagnostic's timestamp */
    int pi_order;               /* a pi-messages answer, its keys in the order api/pi-messages.ts assigns them: 2 the server's terminal event (done:
                                 * responseId, providerThinkingLevel, diagnostics; error: errorMessage first), 1 a failure made anew (errorMessage before
                                 * timestamp); 0 any other API */
    long duration_ms;           /* durationMs: ms from the request's start to the final message (pi-ai 1.1.0
                                 * AssistantMessageEventStream #time, rounded, never below 0); written only when has_duration */
    int has_duration;
} llm_meta;

/* ---- content blocks, each appended to w as one JSON object ---- */
void llm_b_text(jw *w, const char *s, long n, const char *sig);
/* a temperature (millionths) as JavaScript writes the number: 0.7, 1, 0 (transform.c) */
void llm_temp_text(jw *w, long micro);
void llm_b_thinking(jw *w, const char *s, long n, const char *sig, long sn, int redacted);
void llm_b_image(jw *w, const char *data, const char *mime);
/* args: the arguments as JSON object text */
void llm_b_toolcall(jw *w, const char *id, const char *name, const char *args, long an, const char *tsig);
/* json: one JSON value, kept byte for byte */
void llm_b_raw(jw *w, const char *api, const char *json, long n);

/* ---- messages; content: a JSON array of blocks, or for user/toolResult
 * also a plain string (pi-ai allows both for user) ---- */
void llm_m_user(jw *w, const char *content, long cn, llm_ts ts);
void llm_m_assistant(jw *w, const char *content, long cn, const llm_meta *m, llm_ts ts);
/* details: JSON value or 0; duration_ms: durationMs (pi-agent-core 1.1.0: the tool's execute() time, rounded), -1 when the
 * tool did not run (blocked, validation failure, aborted before the start): no key */
void llm_m_toolresult(jw *w, const char *id, const char *name, const char *content, long cn,
                      const char *details, long dn, int is_error, long duration_ms, llm_ts ts);
void llm_m_system(jw *w, const char *text, long n, llm_ts ts);
/* pi-ai's whole SystemMessage: sections a JSON object of named prompt
 * sections (a string, or null removing one), added/removed JSON arrays of
 * tool declarations / {name} references; each 0 when absent. Key order as
 * pi-agent-core writes it: role, content, sections, timestamp, toolsAdded,
 * toolsRemoved. */
void llm_m_system_ex(jw *w, const char *text, long n, const char *sections, long sn, const char *added, long an,
                     const char *removed, long rn, llm_ts ts);

/* any pi-shaped message re-written in the canonical order into out (appended):
 * its role, or LR_BAD (not a message this model knows; out unchanged) */
int llm_msg_canon(const char *json, long n, jw *out);

/* what the loop needs of a message without walking it again */
typedef struct llm_info {
    int role;
    int stop;                   /* assistant: LS_* */
    int ntools;                 /* assistant: toolCall blocks */
    llm_ts ts;
} llm_info;
int llm_msg_info(const char *json, long n, llm_info *i);

/* ---- small pieces the adapters share ---- */
const char *llm_stop_name(int stop);            /* "stop", "length", "toolUse", ... ("" for LS_NONE) */
int llm_stop_of(jv v);                          /* the reverse; LS_NONE when unknown */
void llm_w_micro(jw *w, long micro);            /* 12960 -> 0.01296 */
long llm_r_micro(jv v);                         /* a JSON number of dollars -> micro-dollars */
void llm_w_ts(jw *w, llm_ts ts);
/* pi-ai's Usage object {input, output, cacheRead, cacheWrite, totalTokens, cost{...}} */
void llm_w_usage(jw *w, const llm_usage *u);
void llm_r_usage(jv v, llm_usage *o);
llm_ts llm_r_ts(jv v);

#endif
