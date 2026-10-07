/* Reachability sentinels (plan 2026-10-07-vsh-bash, V11). Host builds that
 * define SH_HITS count each feature hook the executor and expander pass; the
 * host driver (tests/vsh_host.c --hits) prints the counters to stderr, so a
 * probe run through the top-level entry can show the new code RAN. Without
 * SH_HITS (the Amiga build) SH_HIT expands to nothing: zero bytes. Add a hook
 * as an enum entry here, its name in SH_HIT_NAMES, and SH_HIT(id) in the code. */
#ifndef SH_HITS_H
#define SH_HITS_H

#define SH_HIT_LIST(X) \
    X(FUNC_CALL)  /* a function body ran */ \
    X(SUBST)      /* a $( ) ran */ \
    X(SPAWN)      /* a subshell or non-final pipeline stage was spawned */ \
    X(HEREDOC)    /* a here-document temp file was made */ \
    X(EXIT_TRAP)  /* the EXIT trap ran */ \
    X(GLOB)       /* a glob pattern was matched against a directory */ \
    X(ERREXIT)    /* set -e ended the shell */ \
    X(PIPEFAIL)   /* a pipeline's status came from pipefail */ \
    X(TEST_BINARY) /* test evaluated a binary operator */ \
    X(INVOKE_CLUSTER) /* the command line had a cluster of option letters */ \
    X(PARAM_OP)   /* a value-rewriting parameter operator ran: # % / ^ , ~ */ \
    X(PARAM_SLICE) /* a substring ${x:o:l}, ${@:o:l} or ${a[@]:o:l} ran */ \
    X(PARAM_TRANSFORM) /* a ${x@Q} style transform ran */ \
    X(ARRAY_ELEM_SET) /* an array element was assigned */ \
    X(PARAM_VALUES_ARRAY) /* the expander took the values of an array through the shared seam */ \
    X(LOCAL_RESTORE_ARRAY) /* a local array was put back as it was */

#ifdef SH_HITS
#define SH_HIT_ENUM(n) SH_HIT_##n,
enum { SH_HIT_LIST(SH_HIT_ENUM) SH_HIT_COUNT };
extern unsigned long sh_hits[SH_HIT_COUNT];
#define SH_HIT(n) (sh_hits[SH_HIT_##n]++)
#else
#define SH_HIT(n) ((void)0)
#endif

#endif
