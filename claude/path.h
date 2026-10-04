/* path -- AmigaOS path names for the Claude client's tools.
 *
 * "Work:Projects" is absolute (a volume or an assign before the colon);
 * anything else is relative to a base. '/' separates directories, and an
 * empty part means the parent: a leading "/" or "a//b". The Unix habits
 * "." and ".." are understood too, since Claude may write them. A path
 * that climbs above a volume's root is refused.
 * Portable C89, host-tested (tests/test_claude_tools.c). */
#ifndef CL_PATH_H
#define CL_PATH_H

/* rel against base into out: 0, or -1 (above the root, or too long) */
int path_join(const char *base, const char *rel, char *out, long cap);
/* is p root itself or below it? (case-insensitive, as AmigaDOS names are) */
int path_inside(const char *root, const char *p);
/* the parent directory of p into out: 0, -1 at a root */
int path_parent(const char *p, char *out, long cap);

#endif
