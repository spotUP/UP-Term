/* session -- conversations kept on disk as Claude Code keeps them (A4 WP3,
 * row 3.7): one JSONL file per session under
 * ENVARC:Claude/projects/<root's slug>/<id>.jsonl.
 *
 * The file is append-only and written once per completed turn (never per
 * key): one line per message,
 *   {"type":"user","index":3,"message":{"role":"user","content":[...]}}
 * the content array byte for byte as it went to the API (replayed thinking
 * must be unchanged). A line's index says where the message goes; a line
 * whose index is not past the end replaces that message and drops what
 * followed -- so a prompt added to a trailing tool_result message, a
 * /compact, or a /rewind of the conversation is one more line, never an
 * edit of the file. The first line is the session's head
 * ({"type":"session",...}).
 *
 * <slug>/sessions is the index the /resume picker reads (not the session
 * files): one JSON line per session created, resumed, renamed or branched;
 * the last line of an id wins, so its order is the order of use.
 * IDs are 8 hex digits of the clock's seconds (FFS names stay short).
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c). */
#ifndef CL_SESSION_H
#define CL_SESSION_H

#include "conv.h"
#include "sys.h"

#define SESS_LIST 32

typedef struct cl_session {
    cl_sys *sys;
    char dir[300];              /* <home>/projects/<slug> */
    char id[16];
    char file[340];
    char title[96];
    int saved;                  /* messages in the file */
    long last_n;                /* the length of message saved-1 as written */
    int started;                /* the file exists (its head written) */
    int off;                    /* no persistence (--no-session-persistence) */
    long n_appends;             /* writes (the tests' sentinel: one per turn) */
} cl_session;

typedef struct cl_sess_info {
    char id[16];
    char title[96];
    char first[96];             /* the first prompt */
} cl_sess_info;

/* the slug of a root: "Work:Projects/x" -> "Work-Projects-x", at most 30
 * characters (a long one keeps its end and a hash) */
void sess_slug(const char *root, char *out, long cap);
void sess_init(cl_session *s, cl_sys *sys, const char *home, const char *root);
/* a new session (nothing is written until the first save); clock in ms */
void sess_new(cl_session *s, unsigned long ms);
/* the messages from k on are no longer what the file has (a compact, a
 * rewind): the next save writes them again from k */
void sess_truncate(cl_session *s, int k);
/* Appends what the file lacks: 0 (also when there is nothing to do), -1. */
int sess_save(cl_session *s, const cl_conv *c);
/* /rename: the title, in the index (and the file) */
int sess_rename(cl_session *s, const char *title);
/* /branch: the conversation so far into a new session, which becomes the
 * current one: 0, -1 */
int sess_branch(cl_session *s, const cl_conv *c, unsigned long ms);
/* The index, most recently used first: the count (at most max). */
int sess_list(cl_session *s, cl_sess_info *out, int max);
/* A session's messages into c (cleared first), byte-exact: 0, -1 (not
 * there, or no message in it). The session becomes the current one. */
int sess_load(cl_session *s, const char *id, cl_conv *c);
/* An id or a title (exact, else a unique prefix of either) to an id:
 * 0, -1 none, -2 more than one. */
int sess_find(cl_session *s, const char *name, char *id, long cap);

#endif
