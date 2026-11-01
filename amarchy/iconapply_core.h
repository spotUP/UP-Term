/* iconapply_core -- every decision of amarchy-icons APPLY / RESTORE (plan 2026-10-31-amarchy-lucide-icons.md rows L8, L9).
 * Pure: file access goes through the ia_io callbacks, so tests/test_iconapply.c drives the same ia_apply / ia_restore the
 * Amiga program runs, over an in-memory file system. amarchy/icons.c only implements ia_io with Lock / Open / Rename.
 *
 * Backup (owner Q2): SYS:Storage/Icons/Amarchy/<volume>/<path>.info, manifest SYS:Storage/Icons/Amarchy/manifest, one line
 *   path ; crc of the original file (8 hex digits, or "absent" when the icon file did not exist) ; crc of the written images
 * Order of one target (a crash between any two steps leaves the original recoverable): backup file, manifest line, icon file.
 * Idempotence: an icon whose current image bytes have the manifest's written crc is skipped, so a second APPLY never backs
 * the Lucide icon up over the original. A different crc means the user changed it: the backup is refreshed first.
 * Atomic write (ia_atomic_names, ia_recover): write <name>.amt, Close checked, Rename name to <name>.amo, Rename .amt to
 * name, Delete .amo; a leftover .amt / .amo is resolved on the next run by ia_recover. */
#ifndef AMARCHY_ICONAPPLY_CORE_H
#define AMARCHY_ICONAPPLY_CORE_H

#include <stddef.h>
#include <stdint.h>
#include "iconfile_core.h"

#define IA_BACKUP_DIR "SYS:Storage/Icons/Amarchy"
#define IA_MANIFEST   IA_BACKUP_DIR "/manifest"
#define IA_DAT        "UP-Term:Amarchy/Icons/lucide.dat"
#define IA_MAXPATH    160
#define IA_MAXENT     64
#define IA_FILECAP    65536     /* biggest .info file handled (the size of each buffer the caller gives ia_ctx) */

typedef struct ia_entry {
    char     path[IA_MAXPATH];
    int      absent;            /* the original did not exist: RESTORE deletes the file */
    uint32_t ocrc;              /* crc of the original file */
    uint32_t wcrc;              /* crc of the image bytes written (img_off .. img_end of the written file) */
} ia_entry;

typedef struct ia_manifest {
    int      n;
    ia_entry e[IA_MAXENT];
} ia_manifest;

uint32_t ia_crc32(const uint8_t *p, size_t n);
/* crc of the image region of a parsed file: what idempotence compares */
uint32_t ia_imgcrc(const li_file *f);

/* manifest text: "# amarchy-icons manifest 1" then one line per entry. parse returns the entries read (bad lines dropped) */
int    ia_man_parse(ia_manifest *m, const char *text, size_t len);
size_t ia_man_format(const ia_manifest *m, char *out, size_t cap);
int    ia_man_find(const ia_manifest *m, const char *path);       /* index or -1 */
void   ia_man_put(ia_manifest *m, const ia_entry *e);            /* replace by path, else append */
void   ia_man_del(ia_manifest *m, int i);

/* "name.info" -> "name.info.amt" / "name.info.amo"; 0 ok */
int ia_atomic_names(const char *path, char *tmp, char *old, size_t cap);
/* What to do with a leftover of an interrupted atomic write. has_* : the file exists, ok_* : it parses as an icon. */
enum { IA_REC_NONE, IA_REC_DEL_TMP, IA_REC_TMP_TO_NAME, IA_REC_OLD_TO_NAME };
int ia_recover(int has_name, int ok_name, int has_tmp, int ok_tmp, int has_old, int ok_old);

typedef struct ia_io {
    void *ctx;
    /* the whole file in buf: its length, -1 when it does not exist, -2 on error or when it is longer than cap */
    long (*read)(void *ctx, const char *path, uint8_t *buf, size_t cap);
    /* make the directories, write atomically (see above); 0 ok */
    int  (*write)(void *ctx, const char *path, const uint8_t *buf, size_t len);
    int  (*remove)(void *ctx, const char *path);
    /* the system default object for a defaults match when its file is absent ("def_drawer"); length or -1 */
    long (*dflt)(void *ctx, const char *match, uint8_t *buf, size_t cap);
} ia_io;

typedef struct ia_ctx {
    const ia_io *io;
    const li_dat *dat;
    int      variant;           /* li_pick_variant of the Workbench screen */
    uint8_t *cur, *out, *bak;   /* three IA_FILECAP byte buffers */
    ia_manifest man;
    /* counters (STATUS prints them; the sentinel) */
    unsigned written, skipped_done, skipped_bad, skipped_nodat, skipped_nodefault, skipped_io, restored, restore_failed;
} ia_ctx;

/* the Workbench defaults: i-th target, its icon file ("ENVARC:Sys/def_disk.info") and its lucide.dat match ("def_disk"). 0 past the end */
int ia_target(int i, char *path, size_t cap, const char **match);

/* APPLY the defaults (L8). Loads the manifest, then per target: backup, manifest, icon. Returns the number written. */
int ia_apply(ia_ctx *c);
/* RESTORE (L9): the originals back from the backup, entries and backup files removed. No manifest: nothing happens. */
int ia_restore(ia_ctx *c);

/* the command line: "APPLY", "RESTORE", "STATUS", "?" or "" -> IA_CMD_* */
enum { IA_CMD_NONE, IA_CMD_APPLY, IA_CMD_RESTORE, IA_CMD_STATUS, IA_CMD_HELP };
int ia_parse_cmd(const char *args);

#endif
