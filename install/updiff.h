/* updiff -- the Installer's update: the installed kit's manifest against the
 * new kit's (tools/mkmanifest.py writes them on the host; nothing is hashed
 * on the Amiga). Pure C, host-tested (tests/test_updiff.c); install/upupdate.c
 * is the Amiga command around it.
 *
 * A manifest line:  <part> <size> <crc32 hex> <path>   sorted by path.
 *   path "install.dos#<part>": that part's script text ("all": the text
 *   before the first part, which every part reads);
 *   path "copy#<kit drawer>#<drawer>": a copy part's two drawers;
 *   path "handed#<kit path>": a path of the old single kit that an add-on
 *   archive installs now, not this kit (kit-split plan D7). The installed
 *   copies of those files are left alone: not deleted, not counted.
 * The result: which parts run again and how many bytes each copies, and
 * for the copy parts the script lines that copy their new and changed
 * files and delete the ones gone. */
#ifndef UPDIFF_H
#define UPDIFF_H

#define UD_MAXPARTS 32
#define UD_NAME 32
#define UD_DRAWER 96
/* handed# lines in one manifest; the 1.0 kit writes 9 (nvim, Claude, python3,
 * amiga-pi, git, vim, perl and their share drawers). dist-check runs every
 * kit's own manifest through ud_compare (updiff_kit_drv), so a kit that
 * outgrows a limit here fails the build, not the owner's Update page. */
#define UD_MAXHANDED 32

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
    int nhanded;
    char handed[UD_MAXHANDED][UD_DRAWER];   /* the handed paths whose old files were left alone */
} ud_result;

/* One line of a copy part's script (no newline). */
typedef void (*ud_emit)(void *ctx, const char *part, const char *line);

/* Compare the manifests; both texts are changed in place (split into lines).
 * kit: the new kit's drawer, the copies' source (VTC:distkit, Work:UP-Term).
 * Returns 0, or -1 for a manifest that is not one (a bad line, not sorted,
 * more than UD_MAXPARTS parts). */
int ud_compare(char *oldtext, char *newtext, const char *kit, ud_result *r, ud_emit emit, void *ctx);

/* Keep only the lines of a manifest that are under one of the comma separated
 * kit paths (Files/nvim,Files/python3): file lines by their path, copy# lines by
 * their kit drawer; install.dos# and handed# lines are dropped. The order stays.
 * The text is changed in place. Returns the number of lines kept. An add-on's
 * Install uses it on the single kit's manifest (upupdate ... ONLY=<paths>). */
int ud_only(char *text, const char *paths);

/* The KB a part's progress counts (at least 1 for a part that runs). */
unsigned long ud_kb(const ud_part *p);

#endif
