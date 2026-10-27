/* pkgs_core: the rows of the Amarchy Packages menu (plan 2026-10-27-amarchy-packages-submenu.md). They come from the
 * output of `upkg --tsv`: one package per line, nine tab-separated fields, no header (upterm-ports
 * pkgs/opkg/doc/upkg-tsv.txt): name, version, installed (1/0), upgradable (1/0), size, section, launch, src
 * (upkg/uhc), description. A row is kept only when the package is installed and has a launch path: the menu runs
 * that path, nothing else. No OS calls: the DOS side (pkgs_dos.c) runs upkg and hands the text in. */
#ifndef PKGS_CORE_H
#define PKGS_CORE_H

#define PK_NAME 40
#define PK_PATH 160
#define PK_MAX 128
#define PK_ROW_PREFIX "pkg:"

typedef struct pk_list {
    int n;
    char name[PK_MAX][PK_NAME];
    char launch[PK_MAX][PK_PATH];
} pk_list;

/* Parse len bytes of `upkg --tsv` text into l (cleared first). Lines with fewer than seven fields, not installed, an
 * empty launch field, or a name or path that does not fit are skipped; a trailing CR is ignored. Sorted by name
 * without case. Returns l->n. */
int pk_parse(const char *buf, long len, pk_list *l);

/* A package row's path in the menu index ("pkg:<launch>"); 0 when it does not fit cap. pk_path_launch gives the
 * launch path back, 0 when the path is not a package row. */
int pk_row_path(const char *launch, char *out, int cap);
int pk_path_launch(const char *path, char *out, int cap);

#endif
