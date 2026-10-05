/* checkpoint -- file snapshots for /rewind (A4 WP3, row 3.9).
 *
 * Before Claude writes or edits a file, its contents (or the fact that it
 * did not exist) are copied to a snapshot under T:Claude-cp/, tagged with
 * the turn -- the index of the user message that started it. One snapshot
 * per file per turn (the first write of the turn is the one to undo). The
 * snapshots are capped (CP_BYTES together): the oldest turns go first, and
 * a file too large to keep is noted as such. cp_restore puts every file
 * written since a turn back as it was before it (a file created since is
 * deleted). Commands run by Bash are not tracked, as in Claude Code.
 *
 * A4 gaps: each session's snapshots are kept under <dir>/<session>/ with
 * an index, so a resumed session can still be rewound (Claude Code keeps
 * checkpoints across sessions); a session that saved nothing leaves
 * nothing behind.
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c). */
#ifndef CL_CHECKPOINT_H
#define CL_CHECKPOINT_H

#include "sys.h"

#define CP_BYTES (256L * 1024)

typedef struct cl_cpent {
    int turn;
    int existed;                /* the file was there before */
    int kept;                   /* its contents are in the snapshot */
    long size;
    char path[300];
    char snap[320];
} cl_cpent;

typedef struct cl_checkpoints {
    cl_sys *sys;
    char base[300];             /* T:Claude-cp */
    char dir[300];              /* base/<session>: this session's snapshots and index */
    int keep;                   /* cp_free leaves the files (the session was saved) */
    cl_cpent *e;
    int n, cap;
    int turn;                   /* the turn being run */
    long bytes;
    long seq;
    long n_snaps;               /* snapshots taken (the tests' sentinel) */
} cl_checkpoints;

void cp_init(cl_checkpoints *c, cl_sys *sys, const char *dir);
/* the snapshots deleted */
void cp_free(cl_checkpoints *c);
void cp_turn(cl_checkpoints *c, int turn);
/* The session whose snapshots these are (id: its session file's path, its
 * directory named by a hash of it): the entries of another session
 * forgotten (its files stay), this one's read from its index. */
void cp_session(cl_checkpoints *c, const char *id);
/* Before a write or an edit of path (resolved): 0 (also: one taken this
 * turn already), -1 the snapshot could not be made (the write still may
 * go on; /rewind will say it cannot restore that file). */
int cp_before_write(cl_checkpoints *c, const char *path);
/* Every file written in turn >= turn back as before it: the count put
 * back; *lost the count that could not be. The snapshots of those turns
 * are dropped. report lists the files, one per line. */
int cp_restore(cl_checkpoints *c, int turn, int *lost, char *report, long cap);
/* how many files were written in turn >= turn */
int cp_files_since(const cl_checkpoints *c, int turn);

/* The checkpoints of the running REPL, for tools that write (WP2's Write,
 * Edit, MultiEdit): repl.c already calls this for every write_file /
 * edit_file / Write / Edit / MultiEdit call before it runs, so calling it
 * again from the tool is harmless (one snapshot per file per turn). */
void checkpoint_use(cl_checkpoints *c);
int checkpoint_before_write(const char *path);

#endif
