/* sh_path: a Unix-style name as AmigaDOS writes it. Shared by vsh (its opens, cd, test) and the
 * console's completion worker (handler/complete.c), so a word like "./up" means the same file to
 * the line editor as to the command that runs it. No OS calls: the host tests cover it
 * (tests/test_sh_exec.c). Portable C89. */
#ifndef SH_PATH_H
#define SH_PATH_H

/* How many slashes follow the volume's colon in "Vol:/x" (0 when the name has no volume before its
 * first slash). To AmigaDOS that slash is the parent of the volume's root, always an error; POSIX
 * code writes "$HOME/x" with HOME a volume ("System:"), so vsh drops them: "Vol:/x" is Vol:x, a run
 * of slashes too, as in POSIX (sh_amiga_name). */
int sh_vol_slashes(const char *in);

/* in as AmigaDOS names it, into out (max bytes, NUL-terminated): each leading "../" is a "/" (the
 * parent), "./" goes, "." and ".." alone are "" and "/"; "/dev/null" is NIL: and "/dev/tty" the
 * console ("*"); "Vol:/x" is Vol:x. Anything else is copied as it is ("/x" keeps its Amiga meaning,
 * the parent's x). */
void sh_amiga_name(const char *in, char *out, int max);

#endif
