/* The Amarchy job view core (amarchy/job_core.c; plan 2026-10-27-amarchy-job-view.md RJ.6a, RJ.6b). Pure C, no OS
 * calls: it turns the chunks a job writes into lines, finds the `@JOBRC n` end marker and the upkg
 * `[PROGRESS n/m] <phase> <name>` lines, keeps the footer state and the scroll position. */
#ifndef AMARCHY_JOB_CORE_H
#define AMARCHY_JOB_CORE_H

#define JOB_MAX_LINES 200      /* the ring keeps the last lines; older ones count in jc_dropped */
#define JOB_LINE_MAX  160      /* characters kept per line, the rest of a longer line is cut */
#define JOB_FOOT_MAX  64       /* room for a footer word */

enum jc_state { JC_RUNNING, JC_DONE, JC_FAILED, JC_CANCELLED };

typedef struct {
    char line[JOB_MAX_LINES][JOB_LINE_MAX + 1];
    long total;                /* lines completed so far, the absolute index of the next one */
    char cur[JOB_LINE_MAX + 1];
    int curlen, curcol;        /* kept characters, display column (tabs expanded) */
    int pending_cr;            /* the last byte was a CR, a following LF belongs to it */
    int state, ended, rc;      /* ended: the end marker was seen; rc is valid then */
    int prog_n, prog_m;        /* last [PROGRESS n/m], 0 when none */
    char prog_text[JOB_LINE_MAX + 1];
    int follow;                /* the view sticks to the last line */
    long scroll_abs;           /* absolute index of the top line while not following */
} jc;

void jc_init(jc *j);
/* Append output bytes; a partial last line waits for the next chunk. */
void jc_feed(jc *j, const char *buf, long n);
/* Stop at the user's request: running becomes cancelled, later calls and the end marker do not change it. */
void jc_cancel(jc *j);

/* The process of the job is gone. When no end marker was seen (a Ctrl-C can stop the rest of the vsh line, so
 * `echo @JOBRC $?` never ran) the job ends: ended is set, rc is -1 (unknown code) and a running job becomes
 * failed; a cancelled one stays cancelled. After the marker it changes nothing. Feed all remaining output first. */
void jc_gone(jc *j);

/* Lines held now (at most JOB_MAX_LINES), line 0 is the oldest still held. */
int jc_count(const jc *j);
const char *jc_line(const jc *j, int i);
long jc_dropped(const jc *j);          /* lines pushed out of the ring */

/* The footer word for the state ("Running...", "Done", "Nothing to do", "Failed (code 7)", "Cancelled"). */
void jc_footer(const jc *j, char *out, int cap);
/* Index of the first line shown in a view of h rows; scrolling by delta rows (negative is up). Scrolling to the
 * end follows the tail again; scrolling up stops following. */
int jc_top(const jc *j, int h);
void jc_scroll(jc *j, int delta, int h);
/* Header text such as "3 of 7 install Gamma"; empty when no progress line was seen. */
void jc_header(const jc *j, char *out, int cap);

/* Text views that keep their own line store: the absolute index of the first line they still have to append when
 * they already took `seen` lines (lines the ring dropped are skipped); jc_line(j, abs - jc_dropped(j)) is that line. */
long jc_first_new(const jc *j, long seen);

/* Footer colour by state: running is quiet, Done is good, Failed and Cancelled are bad. */
enum jc_tone { JT_DIM, JT_OK, JT_FAIL };
int jc_tone(const jc *j);

/* Keys the view reacts to (the style maps its own key codes to these) and what each one does. */
enum jc_key { JKEY_OTHER, JKEY_UP, JKEY_DOWN, JKEY_PGUP, JKEY_PGDN, JKEY_ESC, JKEY_ENTER };
enum jc_act { JA_NONE, JA_SCROLL, JA_CANCEL, JA_CLOSE };
/* The action for a key; for JA_SCROLL *delta is the rows to move in a body of h rows (negative is up).
 * Esc while running cancels, Esc or Enter after the end closes; Enter while running does nothing. */
int jc_key_act(const jc *j, int key, int h, int *delta);

#endif
