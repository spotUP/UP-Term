/* barconf_core: the Amarchy bar's own settings file (ENV:Amarchy/bar.conf, else ENVARC:Amarchy/bar.conf; plan
 * 2026-10-29-amarchy-p2-status-bar.md R2.13a). One row a line, a row whose first character is ';' or '#' is a
 * comment, words split at blanks:
 *   start on|off                    the bar at start (default on)
 *   clock <fmt>                     the clock segment's format (bar_fmt_time subset; default BAR_CLOCK_FMT)
 *   clockalt <fmt>                  the format after a click (default BAR_DATE_FMT)
 *   left|centre|right <seg> ...     the segment order of that group (names: screens clock cpu ram network agents system tray)
 *   segment <name> on|off           one segment shown or not (default on)
 *   run <label> <command line>      a tray entry: label (one word) and the command a click runs
 * A missing file is the defaults; a bad row is ignored and counted. No OS calls: the file read comes in by the caller. */
#ifndef AMARCHY_BARCONF_CORE_H
#define AMARCHY_BARCONF_CORE_H

#define BC_FMT 32
#define BC_ORDER 8              /* segments named in one group row */
#define BC_RUNS 8
#define BC_LABEL 16
#define BC_CMD 96
enum { BC_G_LEFT = 0, BC_G_CENTRE, BC_G_RIGHT, BC_NGROUPS };
enum { BC_SEG_SCREENS = 0, BC_SEG_CLOCK, BC_SEG_CPU, BC_SEG_RAM, BC_SEG_NETWORK, BC_SEG_AGENTS, BC_SEG_SYSTEM, BC_SEG_TRAY, BC_NSEGS };

typedef struct bc_run {
    char label[BC_LABEL];
    char cmd[BC_CMD];
} bc_run;

typedef struct barconf {
    int start;                          /* 1 on (default), 0 off */
    char clock[BC_FMT];                 /* empty = the built-in format */
    char clockalt[BC_FMT];
    int order[BC_NGROUPS][BC_ORDER];    /* BC_SEG_* in the row's order */
    int norder[BC_NGROUPS];             /* 0 = the built-in order of the group */
    int seg_on[BC_NSEGS];
    bc_run run[BC_RUNS];
    int nrun;
    int bad;                            /* rows ignored */
} barconf;

void barconf_defaults(barconf *c);
/* Parses len bytes of buf over the defaults. Always 1; c->bad counts the rows ignored. */
int barconf_parse(const char *buf, long len, barconf *c);
/* BC_SEG_* of a segment name, -1 when unknown. */
int barconf_seg(const char *name);

#endif
