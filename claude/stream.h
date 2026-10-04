/* stream -- the Messages API's streamed answer, event by event
 * (message_start, content_block_start / _delta / _stop, message_delta,
 * message_stop, ping, error) into the assistant turn.
 *
 * Text deltas go to the screen at once (ui.text). Tool inputs collect per
 * block from input_json_delta and are parsed strictly when the block
 * stops. Thinking text and signatures are kept byte for byte (Claude
 * Opus 5.5 checks replayed thinking). Blocks of a type the client does not
 * know are kept verbatim from content_block_start. The turn for the
 * history (stream_content) follows the API's echo rule for a server-side
 * fallback: thinking, redacted thinking, tool_use and unknown blocks that
 * came before the last `fallback` block are left out.
 * Portable C89, host-tested (tests/test_claude_stream.c). */
#ifndef CL_STREAM_H
#define CL_STREAM_H

#include "json.h"

enum { B_TEXT, B_THINKING, B_REDACTED, B_TOOL, B_FALLBACK, B_OTHER };

typedef struct sblock {
    int type;
    long index;
    int done;                   /* content_block_stop seen */
    int input_ok;               /* a tool's input parsed as a JSON object */
    jw a;                       /* text / thinking / a tool's input JSON */
    jw sig;                     /* thinking: the signature */
    jw start;                   /* the content_block object of content_block_start */
    char id[96];
    char name[64];
} sblock;

typedef struct cl_stream_ui {
    void *u;
    void (*text)(void *u, const char *s, long n);
    /* a block starts: its type, and a tool's name ("" otherwise) */
    void (*block)(void *u, int type, const char *name);
} cl_stream_ui;

enum { ST_WAIT, ST_OPEN, ST_DONE, ST_ERROR, ST_BAD };

typedef struct cl_stream {
    cl_stream_ui ui;
    int state;
    char model[64];
    char id[96];
    char stop_reason[32];       /* end_turn, tool_use, max_tokens, refusal, ... */
    char stop_category[32];     /* stop_details.category of a refusal */
    char err_type[48];          /* an error event: overloaded_error, ... */
    char err_msg[256];
    long in_tok, out_tok, cache_w, cache_r;
    sblock *b;
    int nb, cap;
    int oom;
} cl_stream;

void stream_init(cl_stream *s, const cl_stream_ui *ui);
void stream_free(cl_stream *s);
/* One event (sse.h's callback shape). 0, or -1 when the event is not valid
 * JSON or breaks the order (state ST_BAD). */
int stream_event(cl_stream *s, const char *event, const char *data, long n);
/* the turn's content array for the history ("[...]"); 0, -1 out of memory */
int stream_content(cl_stream *s, jw *out);
/* tool_use blocks, in order */
int stream_tools(cl_stream *s);
sblock *stream_tool(cl_stream *s, int i);

#endif
