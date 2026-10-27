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


/* The Packages group (plan 2026-10-27-amarchy-packages-group.md PG.1): every row of `upkg list --tsv`, any installed
 * state, all nine fields, on the heap. Names, versions and the rest that do not fit are cut or the row is skipped
 * (name); the list is capped at PK_ROWS_MAX and `over` says rows were left out. */
#define PK_VER 24
#define PK_SEC 24
#define PK_SRC 8
#define PK_DESC 80
#define PK_ROWS_MAX 4000
typedef struct pk_row {
    char name[PK_NAME], version[PK_VER], section[PK_SEC], launch[PK_PATH], src[PK_SRC], desc[PK_DESC];
    unsigned char installed, upgradable;
} pk_row;
typedef struct pk_rows {
    int n, over;                /* rows kept, 1 when the list was longer than PK_ROWS_MAX */
    pk_row *r;
} pk_rows;

/* Parse len bytes of `upkg list --tsv` into l (freed first, sorted by name without case). Lines with fewer than
 * six fields, an empty name or a name that fails pk_name_ok are skipped; short fields are empty; a trailing CR is
 * ignored. Returns l->n, -1 when memory ran out. */
int pk_parse_all(const char *buf, long len, pk_rows *l);
void pk_rows_free(pk_rows *l);

/* A name upkg accepts on its command line: letters, digits and . - + _ only, 1..PK_NAME-1 long. */
int pk_name_ok(const char *name);

/* Distinct sections (field 6, "other" for an empty one) sorted without case: at most max names of PK_SEC bytes into
 * out; the count. */
int pk_sections(const pk_rows *l, char out[][PK_SEC], int max);
/* Row indexes (into l->r, name order) of the rows in section sec (without case; "other" = empty field); the count. */
int pk_by_section(const pk_rows *l, const char *sec, int *idx, int max);
/* Row indexes of the rows whose name or description contains text, without case; an empty text lists all. */
int pk_filter(const pk_rows *l, const char *text, int *idx, int max);
/* Row indexes of the installed rows (upgradable = 0) or of the upgradable rows (upgradable = 1). */
int pk_select(const pk_rows *l, int upgradable, int *idx, int max);

/* The state words: "Upgrade to <version>" for an installed upgradable row, "Installed", or "". */
void pk_state(const pk_row *r, char *out, int cap);
/* The menu label: "[upkg] name  version  state" or "[UHC] ..." (src uhc), blanks trimmed. */
void pk_label(const pk_row *r, char *out, int cap);

/* The actions one row offers (PK_ACT_*, in menu order, at most 4): not installed: Install; installed: Remove, Launch
 * (launch field not empty), Upgrade (upgradable). A row whose name fails pk_name_ok offers none. */
#define PK_ACT_INSTALL 'I'
#define PK_ACT_REMOVE  'R'
#define PK_ACT_LAUNCH  'L'
#define PK_ACT_UPGRADE 'U'
int pk_actions(const pk_row *r, char out[4]);
const char *pk_action_label(int act);
/* The job line of an action: "upkg install <name>" ("upkg install uhc:<name>" for src uhc), "upkg remove <name>",
 * "upkg upgrade <name>". 1 with out set; 0 for Launch (the row's pk_row_path runs), an unknown action, a bad name or
 * a line that does not fit cap. */
int pk_action_line(const pk_row *r, int act, char *out, int cap);

#endif
