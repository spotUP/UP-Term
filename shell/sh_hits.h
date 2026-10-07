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
    X(GLOB)       /* a glob pattern was matched against a directory */

#ifdef SH_HITS
#define SH_HIT_ENUM(n) SH_HIT_##n,
enum { SH_HIT_LIST(SH_HIT_ENUM) SH_HIT_COUNT };
extern unsigned long sh_hits[SH_HIT_COUNT];
#define SH_HIT(n) (sh_hits[SH_HIT_##n]++)
#else
#define SH_HIT(n) ((void)0)
#endif

#endif
