/* accum -- neutral stream events (llm/event.h) into pi-ai's assistant
 * message (amiga-pi plan B2): blocks in contentIndex order whatever order
 * their events interleave in, text and thinking byte for byte, signatures
 * reassembled from their deltas, a tool call's arguments parsed strictly
 * when its block ends (a malformed one makes the message an error, never
 * a crash -- except when the answer stopped at its length: then, as pi-ai's
 * salvage parser does, the arguments are completed as far as they go and
 * the call kept, so the loop can tell the model it was cut), usage and the
 * stop reason.
 * Portable C89, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_ACCUM_H
#define LLM_ACCUM_H

#include "llm.h"
#include "backend.h"
#include "event.h"

typedef struct llm_ablock {
    long index;
    int kind;                   /* LB_* */
    int redacted, done;
    int bad;                    /* LB_TOOLCALL: its arguments did not parse (settled at the stop) */
    jw a;                       /* text / thinking / argument JSON / raw input JSON */
    jw sig;
    jw raw;                     /* the provider's block (LB_RAW) */
    char id[96], name[64], raw_type[32];
} llm_ablock;

typedef struct llm_acc {
    const char *api, *provider;
    char model[64];             /* the model the server named (LE_START) */
    char cfg_model[96];         /* the configured model's id (llm_acc_model), "" unknown */
    llm_fallback fb[LLM_FALLBACKS];     /* compat.allowedFallbackModels (llm_acc_fallbacks) */
    int nfb;
    char level[16];             /* providerThinkingLevel the message records (llm_acc_level), "" none */
    char tlevel[16];            /* thinkingLevel the message records (llm_acc_thinking), "" none */
    long duration_ms;           /* durationMs the message records (llm_acc_duration) */
    int has_duration;
    long price[4];              /* its price per million tokens in micro-dollars: input, output, cacheRead, cacheWrite */
    char id[96];
    char level_srv[16];         /* providerThinkingLevel the server sent (LE_META), "" none */
    jw usage_raw;               /* the usage object as the server sent it (LE_META), empty none */
    jw rewrite;                 /* the rewrite impact object (LE_META), empty none */
    llm_ts rewrite_ts;          /* when it arrived (llm_wall): the diagnostic's timestamp */
    jw diag;                    /* the diagnostics LE_META events carried, JSON objects joined by commas */
    int have_cost;              /* usage.cost is the server's (LE_USAGE has_cost) */
    int meta_seen;              /* LE_META came: the server's own terminal event follows (pi-messages) */
    int stop;                   /* LS_* */
    char stop_raw[32];
    char *err;                  /* the error text (LLM_ERR_MAX), allocated by the first failure, "" before */
    llm_usage usage;
    int started, done, failed;
    int args_failed;            /* the error is a finished call's bad arguments (settle), not the stream's */
    llm_ablock *b;
    int nb, cap;
    int oom;
    long events;                /* every event taken (a sentinel for the tests) */
} llm_acc;

void llm_acc_init(llm_acc *a, const char *api, const char *provider);
void llm_acc_free(llm_acc *a);
/* the model the request was made to (pi: the message's `model` is the configured id, the server's own name only
 * `responseModel` when it differs) and its price, for the usage cost (pi-ai calculateCost). Without it the
 * message names the server's model and carries no cost. */
void llm_acc_model(llm_acc *a, const char *id, const long price[4]);
/* compat.allowedFallbackModels of the model: when the server answers with another model that this list names (same
 * provider), the usage is priced with that entry's cost (anthropic-messages.ts:671-677) */
void llm_acc_fallbacks(llm_acc *a, const llm_fallback *fb, int n);
/* the thinking level the request carried, kept on the message as pi-ai's providerThinkingLevel (llm_ant_effort);
 * 0 none */
void llm_acc_level(llm_acc *a, const char *level);
/* the session's thinking level the request was made at ("off" included), kept on the finished message as pi-agent-core's
 * thinkingLevel (agent-loop.js adds it to the stream's result); not set: the message has none */
void llm_acc_thinking(llm_acc *a, const char *level);
/* the request's duration, kept on the finished message as pi-ai 1.1.0's durationMs (ms from the request's start to the
 * final message); not set: the message has none. A streaming message carries none either: the caller clears it. */
void llm_acc_duration(llm_acc *a, long ms);
/* the message's own fields so far (api, provider, model, responseId, responseModel, rawStopReason, providerThinkingLevel,
 * thinkingLevel, usage with its cost, as pi-ai's calculateCost keeps them on every update); stop and error left 0.
 * A streaming message (message_start, message_update) has no thinkingLevel yet: the caller clears it. */
void llm_acc_meta(const llm_acc *a, llm_meta *m);
/* the sink's callback: u is the llm_acc */
void llm_acc_event(void *u, const llm_event *e);
/* a sink feeding a */
llm_sink llm_acc_sink(llm_acc *a);
/* the assistant message so far (complete after LE_DONE / LE_ERROR; an
 * unfinished one says aborted) appended to out: 0, -1 out of memory */
int llm_acc_message(llm_acc *a, llm_ts ts, jw *out);
/* tool calls of the finished message, in order */
int llm_acc_tools(const llm_acc *a);
const llm_ablock *llm_acc_tool(const llm_acc *a, int i);

#endif
