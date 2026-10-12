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
/* pi-ai's StopReason */
enum { LS_NONE, LS_STOP, LS_LENGTH, LS_TOOLUSE, LS_ERROR, LS_ABORTED };

typedef struct llm_ts {
    long sec;                   /* seconds since 1970-01-01 UTC */
    int ms;                     /* 0..999 */
} llm_ts;

typedef struct llm_usage {
    long input, output, cache_read, cache_write;
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
} llm_meta;

/* ---- content blocks, each appended to w as one JSON object ---- */
void llm_b_text(jw *w, const char *s, long n, const char *sig);
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
/* details: JSON value or 0 */
void llm_m_toolresult(jw *w, const char *id, const char *name, const char *content, long cn,
                      const char *details, long dn, int is_error, llm_ts ts);
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
