#ifndef VSH_CMDLINE_H
#define VSH_CMDLINE_H

/* argv as an AmigaDOS command line, malloc'd (0 when out of memory). */
char *vsh_command_line(char **argv, const char *quoted);
/* quoted: 0, or per word nonzero when the word was quoted in the source: it is then one quoted item,
 * never split into a bare KEY= keyword and a value (`Echo "rc=0"` keeps its `=`). */

#endif
