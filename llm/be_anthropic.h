/* be_anthropic -- the anthropic-messages backend (amiga-pi plan B5): the
 * Messages API's request head (x-api-key, anthropic-version,
 * anthropic-beta), its redaction, the body from pi-ai messages, and the
 * decoder from its SSE events (message_start, content_block_start / _delta
 * / _stop, message_delta, message_stop, ping, error) to the neutral events.
 * C:Claude's stream.c and repl.c use the same decoder and head, so there is
 * one Anthropic implementation for both programs.
 * Portable C89, host-tested (tests/test_claude_stream.c, test_claude_http.c,
 * test_pi_llm.c). */
#ifndef LLM_BE_ANTHROPIC_H
#define LLM_BE_ANTHROPIC_H

#include "backend.h"

/* the decoder's view of one block: enough to check the stream's order */
typedef struct llm_anth_blk {
    long index;
    int type;                   /* AK_* */
    int done;
    long got;                   /* input bytes taken from deltas */
    jw start;                   /* the content_block object (a tool's input when no delta comes) */
} llm_anth_blk;

enum { AK_TEXT, AK_THINKING, AK_REDACTED, AK_TOOL, AK_FALLBACK, AK_OTHER, AK_SERVER };
enum { AS_WAIT, AS_OPEN, AS_DONE, AS_ERROR, AS_BAD };

typedef struct llm_anth_dec {
    int state;                  /* AS_* */
    llm_anth_blk *b;
    int nb, cap;
    int oom;
} llm_anth_dec;

void llm_anth_init(llm_anth_dec *d);
void llm_anth_free(llm_anth_dec *d);
/* one event's data (the data's own "type" decides; the event name is not
 * needed) into k: 0, or -1 when it is not valid JSON or breaks the order
 * (the decoder is then AS_BAD for good) */
int llm_anth_feed(llm_anth_dec *d, const char *data, long n, const llm_sink *k);

/* the user agent C:Claude sends */
#define LLM_UA_CLAUDE "UP-Term-Claude/1.0 (AmigaOS)"

#endif
