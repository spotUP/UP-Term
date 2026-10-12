/* io_amiga -- the console a program of C:Claude's core runs in (cl_io, ui.h)
 * on AmigaOS: lines through the window's own editing, Ctrl+C, a clock; and
 * the screen of its own when the console is UP-Term's (raw mode through its
 * line discipline, the window's size, Ctrl+G's editor). C:Claude and
 * amiga-pi both use it (moved from claude/main_amiga.c unchanged). */
#ifndef CL_IO_AMIGA_H
#define CL_IO_AMIGA_H

#include "ui.h"

/* once at the start, before anything is read from Input(): the argument line the Shell left in its buffer is read out
 * (cl_runcommand_args decides), so a redirected file is read from its start */
void io_amiga_args(void);
/* read_line, write, brk, sleep, ms (log is the program's) */
void io_amiga_line(cl_io *io);
/* read, size, raw, edit when Input() and Output() are UP-Term consoles: 1, else 0 (io unchanged) */
int io_amiga_screen(cl_io *io);
/* raw mode off again if it was on (at the program's end) */
void io_amiga_end(void);
/* Ctrl+C since the last call (cleared) */
int io_amiga_brk(void *u);

#endif
