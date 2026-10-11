/* ignore -- the ignore files that ripgrep and fd honour (the Rust ignore
 * crate's rules), for amiga-pi's grep and find (pi runs rg and fd): per
 * directory .gitignore, .ignore, the tool's own file (.rgignore, .fdignore)
 * and the repository's .git/info/exclude, in gitignore syntax (comments,
 * '!' re-includes, a trailing '/' for directories, a '/' inside anchors to
 * the file's directory, '*' '?' [..] '**', the last match wins). The deepest
 * directory with a match decides; .rgignore/.fdignore, then .ignore, then
 * .gitignore, then exclude. .gitignore and exclude count only inside a git
 * repository unless require_git is 0 (fd outside one: --no-require-git),
 * and then a parent's stop at the nearest directory holding .git (measured
 * against rg 14: without require_git they reach through it). Names match
 * ignoring case, as AmigaDOS names do (glob.h).
 * Portable C89 over sys.h, host-tested against git check-ignore and rg
 * (tests/test_pi_ignore.py). */
#ifndef CL_IGNORE_H
#define CL_IGNORE_H

#include "sys.h"

typedef struct ig_rule {
    char *pat;
    unsigned char neg, dir_only, anchored;
} ig_rule;

typedef struct ig_set {
    ig_rule *r;
    int n;
} ig_set;

/* one directory's rules, a chain up to the search's parents */
typedef struct ig_dir {
    struct ig_dir *parent;
    char *path;                 /* the directory's full path */
    int has_git;                /* it holds .git (a repository's top) */
    ig_set custom, ignore, git, exclude;
} ig_dir;

/* d for the directory full below parent (0: the top); custom is the
 * tool's own file name (".rgignore", ".fdignore"). 0, -1 out of memory. */
int ig_load(ig_dir *d, cl_sys *sys, const char *full, ig_dir *parent, const char *custom);
void ig_free(ig_dir *d);
/* the chain for full and every directory above it (the parents rg and fd
 * read), malloc'ed; 0 out of memory. ig_free_chain frees it. */
ig_dir *ig_chain(cl_sys *sys, const char *full, const char *custom);
void ig_free_chain(ig_dir *d);
/* is the object full (in directory d) ignored? 1 ignored, 0 not */
int ig_ignored(const ig_dir *d, const char *full, int is_dir, int require_git);
/* the rules of one ignore file's text into s (its directory's rules) */
int ig_parse(ig_set *s, const char *text, long n);
/* one rule set against rel (relative to its directory): 1 ignore, -1
 * whitelist (a '!' rule), 0 no match */
int ig_match(const ig_set *s, const char *rel, int is_dir);

#endif
