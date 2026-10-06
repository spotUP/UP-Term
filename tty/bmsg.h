/* Boundary messages: what a kit program says when something outside it is
 * missing (a TCP/IP stack, AmiSSL, the XCON: handler, a 68020, a command
 * that lives in another drawer). Pure text selection with no OS calls, so
 * the host tests pin each wording and every program prints the same one.
 * Each message names the missing piece and the fix. */
#ifndef BMSG_H
#define BMSG_H

/* the kit's drawer of commands (coreutils, ssh, sz, rz, hl, mdv) */
#define BMSG_KIT_BIN "SYS:UP-Term/bin"

/* uptelnet without bsdsocket.library */
const char *bmsg_no_stack(void);

/* The meaning of a failed connect()/socket errno as bsdsocket.library
 * numbers it (BSD values): 65 EHOSTUNREACH, 61 ECONNREFUSED, 60 ETIMEDOUT.
 * 0 for any other number (the caller then prints "error N"). */
const char *bmsg_net_errno(long err);

/* C:Claude without AmiSSL (the library, or a build without it) */
const char *bmsg_amissl_missing(void);

/* ENVARC:Claude/remote is set but C:uptelnet is not there */
const char *bmsg_uptelnet_missing(void);

/* a program that needs the XCON: window and finds no such handler */
const char *bmsg_xcon_missing(void);

/* A 68000 or 68010 start: one line, "needs a 68020 or better". The program
 * prints its own name first. */
const char *bmsg_need_68020(void);

/* The CPU check: attn is ExecBase->AttnFlags. 1 when a 68020 or better
 * (AFF_68020 is bit 1). */
int bmsg_cpu_ok(unsigned attn);

/* A command that is in the kit's bin drawer: BMSG_KIT_BIN, or 0 when the
 * name is not one of the kit's commands. Case-sensitive, as vsh is. */
const char *bmsg_kit_drawer(const char *name);

#endif
