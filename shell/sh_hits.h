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
    X(HASH_RUN)   /* a command run found its entry in the hash table */ \
    X(EXEC_MISSING_EXIT) /* exec of a command that is not found ended a shell that is not interactive (127) */ \
    X(POSIX_FATAL) /* posix mode ended a shell on the error of a special builtin or an assignment */ \
    X(POSIX_FORMAT) /* posix mode printed export -p, readonly -p or type in its own words */ \
    X(POSIX_SUBST) /* a command substitution inherited set -e in posix mode */ \
    X(POSIX_VAR)  /* POSIXLY_CORRECT turned posix mode on or off */ \
    X(POSIX_ENV)  /* an interactive posix shell sourced $ENV */ \
    X(LOGIN_PROFILE) /* a login shell read its profile files */ \
    X(DIRSTACK_WRITE) /* DIRSTACK[n]=dir set a directory of the stack */ \
    X(REALPATH)   /* cd -P or pwd -P resolved a path through the OS layer */ \
    X(COPROC)     /* coproc started a command with its two pipes */ \
    X(POSIX_PERSIST) /* a special builtin kept its prefix assignments (posix mode) */ \
    X(SELECT_PASS) /* select ran its body for a menu choice */ \
    X(CDPATH_USED) /* cd found its directory through CDPATH */ \
    X(BASH_ENV_RUN) /* BASH_ENV was sourced at startup */ \
    X(TIME_RUN)   /* the time keyword timed a pipeline */ \
    X(SPAWN)      /* a subshell or non-final pipeline stage was spawned */ \
    X(FD_HIGH)    /* a descriptor above 2 was resolved through the shell's fd table */ \
    X(FDVAR_ALLOC) /* {var}> allocated a descriptor */ \
    X(HEREDOC)    /* a here-document temp file was made */ \
    X(EXIT_TRAP)  /* the EXIT trap ran */ \
    X(EXTGLOB)    /* an @( ) ?( ) *( ) +( ) !( ) pattern was matched */ \
    X(GLOBSTAR)   /* a ** component was expanded over directories */ \
    X(SHOPT_SET)  /* shopt -s or -u changed an option */ \
    X(TRAP_ERR)   /* an ERR trap fired */ \
    X(TRAP_DEBUG) /* a DEBUG trap fired */ \
    X(TRAP_RETURN) /* a RETURN trap fired */ \
    X(GLOB)       /* a glob pattern was matched against a directory */ \
    X(ERREXIT)    /* set -e ended the shell */ \
    X(PIPEFAIL)   /* a pipeline's status came from pipefail */ \
    X(TEST_BINARY) /* test evaluated a binary operator */ \
    X(INVOKE_CLUSTER) /* the command line had a cluster of option letters */ \
    X(PARAM_OP)   /* a value-rewriting parameter operator ran: # % / ^ , ~ */ \
    X(PARAM_SLICE) /* a substring ${x:o:l}, ${@:o:l} or ${a[@]:o:l} ran */ \
    X(BRACE) /* a brace expansion produced words */ \
    X(PROCSUB)    /* a <( ) or >( ) made its temp file */ \
    X(HERESTRING) /* a <<< here-string was set up */ \
    X(PARAM_TRANSFORM) /* a ${x@Q} style transform ran */ \
    X(ARRAY_ELEM_SET) /* an array element was assigned */ \
    X(PARAM_VALUES_ARRAY) /* the expander took the values of an array through the shared seam */ \
    X(LOCAL_RESTORE_ARRAY) /* a local array was put back as it was */ \
    X(ARITHCMD) /* a (( )) command ran */ \
    X(FORARITH) /* a for (( ; ; )) loop ran */ \
    X(REGEX_CAPTURE) /* a [[ =~ ]] matched and set BASH_REMATCH */ \
    X(DBRACK) /* a [[ ]] ran */ \
    X(ARITH_ASSIGN) /* an arithmetic assignment, ++ or -- stored a value */ \
    X(VAR_SHARED) /* a subshell read its parent's variables in place (sh_shell_clone, share) */ \
    X(VAR_COPY_UP) /* a subshell copied one of its parent's variables before writing it */ \
    X(FUNC_SHARED) /* a subshell read its parent's functions and aliases in place (sh_shell_clone, share) */ \
    X(QUIT_SIGNAL) /* the OS layer reported a QUIT (Amiga: break bit E) */

#ifdef SH_HITS
#define SH_HIT_ENUM(n) SH_HIT_##n,
enum { SH_HIT_LIST(SH_HIT_ENUM) SH_HIT_COUNT };
extern unsigned long sh_hits[SH_HIT_COUNT];
#define SH_HIT(n) (sh_hits[SH_HIT_##n]++)
#else
#define SH_HIT(n) ((void)0)
#endif

#endif
