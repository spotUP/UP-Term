/* updiff -- the Installer's update: the installed kit's manifest against the
 * new kit's (tools/mkmanifest.py writes them on the host; nothing is hashed
 * on the Amiga). Pure C, host-tested (tests/test_updiff.c); install/upupdate.c
 * is the Amiga command around it.
 *
 * A manifest line:  <part> <size> <crc32 hex> <path>   sorted by path.
 *   path "install.dos#<part>": that part's script text ("all": the text
 *   before the first part, which every part reads);
 *   path "copy#<kit drawer>#<drawer>": a copy part's two drawers.
 * The result: which parts run again and how many bytes each copies, and
 * for the copy parts the script lines that copy their new and changed
 * files and delete the ones gone. */
#ifndef UPDIFF_H
#define UPDIFF_H

#define UD_MAXPARTS 32
#define UD_NAME 32
#define UD_DRAWER 96

typedef struct {
    char name[UD_NAME];
    unsigned long bytes;            /* the new and changed files' sizes */
    int run;                        /* this part runs again */
    int copy;                       /* a copy part: its files go by script */
    char from[UD_DRAWER];           /* a copy part's kit drawer (Files/...) */
    char drawer[UD_DRAWER];         /* and its drawer in UP-Term: */
} ud_part;

typedef struct {
    ud_part part[UD_MAXPARTS];
    int nparts;
    unsigned long bytes;            /* all parts' */
    int files;                      /* new and changed files */
    int gone;                       /* files gone */
} ud_result;

/* One line of a copy part's script (no newline). */
typedef void (*ud_emit)(void *ctx, const char *part, const char *line);

/* Compare the manifests; both texts are changed in place (split into lines).
 * kit: the new kit's drawer, the copies' source (VTC:distkit, Work:UP-Term).
 * Returns 0, or -1 for a manifest that is not one (a bad line, not sorted,
 * more than UD_MAXPARTS parts). */
int ud_compare(char *oldtext, char *newtext, const char *kit, ud_result *r, ud_emit emit, void *ctx);

/* The KB a part's progress counts (at least 1 for a part that runs). */
unsigned long ud_kb(const ud_part *p);

#endif
