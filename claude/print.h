/* print -- C:Claude's print mode (A4 WP4, rows 4.1 4.2): `Claude -p`,
 * Claude Code's non-interactive run. One prompt (the command line's, with
 * text piped in after it), or with --input-format stream-json one user
 * message per line of the input; each answered through the REPL core
 * (repl_line: the same turns, tools, rules and hooks as at the screen,
 * nobody to ask), then the result in the output format:
 *
 *   text         the answer's text
 *   json         one result object
 *   stream-json  a JSON object a line: system/init, every assistant
 *                message, every round's tool results (user), with
 *                --include-partial-messages the raw stream events
 *                (stream_event), and the result
 *
 * The shapes are Claude Code's (Agent SDK's SDKSystemMessage,
 * SDKAssistantMessage, SDKUserMessage, SDKPartialAssistantMessage,
 * SDKResultMessage; fetched 2026-10-05). The transcript (tool lines,
 * notes) never reaches the output: with --verbose it goes to the error
 * stream.
 * Portable C89, host-tested (tests/test_claude_cli.c). */
#ifndef CL_PRINT_H
#define CL_PRINT_H

#include "cli.h"

typedef struct cl_pout {
    void *u;
    /* the output (stdout) */
    void (*out)(void *u, const char *s, long n);
    /* the error stream (0: none) */
    void (*err)(void *u, const char *s, long n);
    /* what was piped in: bytes, 0 at its end, -1 failed; 0 when nothing is
     * piped (the input is the console) */
    long (*in)(void *u, char *buf, long cap);
} cl_pout;

#define PRINT_IN_MAX (10L * 1024 * 1024)   /* piped input, as Claude Code's cap */

/* The run, after repl_init and cli_apply: the exit code (0, 10 when the
 * result is an error, 20 when there was nothing to do). */
int print_run(cl_repl *r, cl_cli *c, cl_pout *p);

#endif
