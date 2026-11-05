/* vsh's AmigaDOS script mode (design: thoughts/shared/research/s58-evidence/vsh-script-mode-design.md).
 * A line-oriented interpreter for Execute files, run in the shell's own process over sh_exec(): every
 * executable line becomes one literal SH_CMD whose words are single-quoted, so the sh expander leaves
 * them alone (Amiga commands match #? themselves). Portable C89. */
#ifndef SH_AMISCRIPT_H
#define SH_AMISCRIPT_H
#include "sh_exec.h"

/* Run the text of an Execute file (file names it for messages) in sh. argv[0] is the file; argv[1..] are
 * the script's arguments. Returns the script's result code (the last command's status). */
long sh_amiga_execute(sh_shell *sh, const char *text, const char *file, int argc, char **argv, const sh_io *io);

#endif
