/* The pure half of the AMARCHY port's STATUS and BAR replies: the key tables, the line writer and the BAR verb
 * parser. No OS types; host-tested by tests/test_status_core.c. STATUS must fit REXX_HOST_OUT with every counter at
 * its largest (cl_cat cuts what does not fit, and the last key is the sentinel the rig reads). */
#ifndef AMARCHY_STATUS_CORE_H
#define AMARCHY_STATUS_CORE_H

enum { STATUS_NVALS = 12, BARSTATUS_NVALS = 6 };

/* One reply line "key=N key=N ..." from val[], STATUS_NVALS values for STATUS (bar_status 0), BARSTATUS_NVALS for BAR
 * STATUS (1). Written into out of cap bytes (cap includes the NUL); returns the length the whole line needs, so a
 * result >= cap means it was cut. */
long status_line(int bar_status, const long *val, char *out, long cap);

enum { BARV_BAD = 0, BARV_STATUS, BARV_ON, BARV_OFF, BARV_TOGGLE };

/* The word after BAR in a command ("BAR OFF"; cmd points at the text after "BAR"), any case, blanks around it. */
int bar_verb(const char *args);

#endif
