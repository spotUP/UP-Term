/* vw_plat -- what hl and mdv need from the system: files, the console,
 * the window's width, variables, Ctrl-C. vw_plat_amiga.c (AmigaDOS, the
 * UP-Term console packets) and vw_plat_posix.c (the host build). */
#ifndef VW_PLAT_H
#define VW_PLAT_H

typedef struct vw_file vw_file;

/* 0 or "-": standard input; 0 when it cannot be opened */
vw_file *vw_open(const char *name);
/* up to n bytes: how many, 0 at the end, -1 on an error */
long vw_read(vw_file *f, char *buf, long n);
void vw_close(vw_file *f);
/* standard output (a vw_sink); vw_write_failed says whether one failed */
void vw_write(void *u, const char *s, long n);
int  vw_write_failed(void);
/* a message to the user (the console, or standard error on the host) */
void vw_say(const char *s);
/* standard output is a console window or terminal */
int  vw_out_is_tty(void);
/* the window's columns, 0 when nothing says */
int  vw_columns(void);
/* a variable (local first, then global): 1 with buf filled */
int  vw_env(const char *name, char *buf, int n);
/* Ctrl-C was pressed (and is now taken) */
int  vw_break(void);

/* Hand the command to the real cat: argv[0] is the program's name, the rest the arguments (hl -p is the
 * cat of vsh, and an option it does not own is cat's). Returns only when cat could not be run (the return
 * code to exit with); never runs hl itself again. */
int  vw_exec_cat(int argc, char **argv);

/* the return code for a failure: 10 (RETURN_ERROR) on AmigaDOS, 1 on Unix */
extern const int vw_fail_code;

#endif
