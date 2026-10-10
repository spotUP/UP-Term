/* event -- the provider-neutral stream events (amiga-pi plan B2), after
 * pi-ai's AssistantMessageEvent: a start; for each content block a start,
 * deltas and an end, keyed by contentIndex (blocks may interleave); the
 * stop; done or error. A backend's decoder (llm/be_*.c) turns its wire
 * events into these; consumers are llm/accum.c (pi-ai's assistant message)
 * and C:Claude's stream.c (its sblocks). Failures travel in the stream
 * (LE_ERROR), never as a return path a consumer must special-case.
 * Portable C89. */
#ifndef LLM_EVENT_H
#define LLM_EVENT_H

enum {
    LE_START,                   /* the answer begins: model, id (p/n) */
    LE_BLOCK_START,             /* a content block: kind, index, raw/rn, first bytes in p/n */
    LE_DELTA,                   /* text / thinking / toolCall argument JSON / a raw block's input JSON */
    LE_SIG_DELTA,               /* thinking: signature bytes */
    LE_BLOCK_END,
    LE_USAGE,                   /* token counts so far (-1: not in this event) */
    LE_STOP,                    /* why the answer stopped: stop, stop_raw, stop_detail */
    LE_DONE,                    /* the answer is complete */
    LE_ERROR                    /* the answer failed: stop LS_ERROR or LS_ABORTED, err_type, p/n message */
};

/* a block's kind; LB_RAW is a provider block pi-ai does not model */
enum { LB_TEXT, LB_THINKING, LB_TOOLCALL, LB_RAW };

typedef struct llm_event {
    int type;                   /* LE_* */
    long index;                 /* contentIndex */
    int kind;                   /* LB_*, at LE_BLOCK_START */
    int redacted;               /* LB_THINKING that came redacted */
    const char *p;              /* the event's bytes (see the types) */
    long n;
    const char *sig;            /* LE_BLOCK_START of thinking: its first signature bytes */
    long sn;
    const char *raw;            /* LE_BLOCK_START: the provider's block as it came (JSON), 0 none */
    long rn;
    const char *raw_type;       /* LB_RAW: the provider's type ("fallback", "server_tool_use", ...) */
    const char *id, *name;      /* LB_TOOLCALL (and a raw tool call): the call's id and name */
    const char *model;          /* LE_START */
    int stop;                   /* LE_STOP, LE_DONE, LE_ERROR: LS_* (llm.h) */
    const char *stop_raw;       /* LE_STOP: the provider's own reason ("end_turn", "tool_calls", ...) */
    const char *stop_detail;    /* LE_STOP: a refusal's category, "" none */
    const char *err_type;       /* LE_ERROR: "overloaded_error", "aborted", "http_429", ... */
    long in, out, cache_r, cache_w;     /* LE_USAGE */
} llm_event;

typedef struct llm_sink {
    void *u;
    void (*ev)(void *u, const llm_event *e);
} llm_sink;

/* an event with every field empty (-1 counts) */
void llm_event_init(llm_event *e, int type);

#endif
