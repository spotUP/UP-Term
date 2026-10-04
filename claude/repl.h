/* repl -- the Claude client's core: the prompt loop, slash commands, one
 * turn (request, streamed answer, tool rounds until Claude is done), the
 * transport with keep-alive, retries and Ctrl+C. Everything machine-
 * specific comes in through cl_io, cl_net and cl_sys, so the host tests
 * drive this exact code against recorded streams (tests/test_claude_repl.c
 * is the reachability test); claude/main_amiga.c is only the wiring.
 *
 * A turn that does not complete -- Ctrl+C, a refusal, an error, a cut
 * tool call -- leaves the history as it was before the prompt (or before
 * the unanswered request), so the conversation always stays valid. */
#ifndef CL_REPL_H
#define CL_REPL_H

#include "net.h"
#include "sys.h"
#include "http.h"
#include "sse.h"
#include "stream.h"
#include "conv.h"
#include "tools.h"
#include "ui.h"

#define CL_DEFAULT_URL    "https://api.anthropic.com/v1/messages"
#define CL_DEFAULT_MODEL  "claude-opus-5-5"
#define CL_DEFAULT_EFFORT "medium"
#define CL_MAX_TOKENS     64000L
#define CL_TRIES          5

typedef struct cl_repl {
    cl_io *io;
    cl_net *net;
    cl_sys *sys;
    cl_ui ui;
    cl_render render;           /* the answer's renderer (ui_plain by default) */
    http_url url;
    const char *key;            /* never shown, never logged */
    char model[64];
    char effort[16];
    long max_tokens;
    char *system;
    cl_conv conv;
    cl_tools tools;
    int connected;
    int debug;
    /* the request in flight */
    http_resp resp;
    sse sse;
    cl_stream st;
    jw errbody;
    int shown;                  /* answer text went to the screen */
    /* the screen of its own (ledger A3), 0 in the line mode */
    struct cl_tui *tui;
    struct cl_show *show;
    long ctx_used;              /* the last request's tokens: the context in use */
    long turn_out;              /* output tokens of the turn's finished requests */
    long chars;                 /* answer bytes of the request in flight */
    char session[256];          /* saved after every turn (/resume), "" none */
    unsigned long t_open, t_first;  /* ping: connect and first-byte times */
    char head[1024];
    char buf[4096];
} cl_repl;

/* 0, or -1 with the reason shown */
int repl_init(cl_repl *r, cl_io *io, cl_net *net, cl_sys *sys, const char *url, const char *key,
              const char *root);
void repl_free(cl_repl *r);
/* one line typed at the prompt: 1 when it was /exit */
int repl_line(cl_repl *r, const char *line);
/* the loop: until /exit or the end of input */
void repl_run(cl_repl *r);
/* The screen of its own (Claude Code's look, ledger A3): raw mode, the
 * input box, the status line. 0 when it started; -1 (out of memory, no
 * io->read, or the console refused raw mode): the line mode stays. */
int repl_screen(cl_repl *r);
/* the model's context window in tokens */
long repl_window(const char *model);
/* the transport check: a one-token request, its status and times; 0 when
 * the API answered 200 */
int repl_ping(cl_repl *r);
/* the start of a key read from a file or a variable, cleaned in place:
 * white space trimmed; 0 when it is usable */
int cl_key_clean(char *key);

#endif
