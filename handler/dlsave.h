/* dlsave.h -- the one writer of downloaded files: ENVARC:UP-Term/Downloads/<host>/<name>
 * (RIP-7b level-9 ZMODEM receive and the iTerm2 inline=0 download share it). The path rules
 * and the create-each-level walk are portable and host-tested (tests/test_dlsave.c); the AmigaDOS
 * binding dl_save is compiled with -DDLSAVE_DOS in the handler only. */
#ifndef DLSAVE_H
#define DLSAVE_H

#include "../engine/vtengine.h"

#define DL_ROOT "ENVARC:UP-Term/Downloads"
#define DL_LEAF_MAX 40 /* host and name are cut to this many characters in the path */

/* out = "ENVARC:UP-Term/Downloads/<host>/<name>" ("" host = "local"). Host and name are cut
 * to DL_LEAF_MAX; a '/', ':' or control character in either turns into '_' (the engine has
 * already sanitised them, asserted again here). Returns 1 when it fits in n, else 0 with out "". */
int dl_path(char *out, int n, const char *host, const char *name);

/* What the core needs from the OS. */
typedef struct dl_os {
    /* make one directory level; 1 when it exists afterwards (already there counts) */
    int (*mkdir)(const char *path);
    /* write the whole file, replacing an existing one; 1 when every byte went */
    int (*write)(const char *path, const vt_u8 *data, long len);
} dl_os;

/* Create ENVARC:UP-Term, .../Downloads and .../<host> as needed, then write the file.
 * Returns 1 on success. */
int dl_save_with(const dl_os *os, const char *host, const char *name, const vt_u8 *data, long len);

/* vt_dl_sink for the handler (AmigaDOS binding; only with -DDLSAVE_DOS) */
int dl_save(const char *host, const char *name, const vt_u8 *data, long len);

#endif
