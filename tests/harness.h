/* Minimal host test harness for vtengine suites. */
#ifndef HARNESS_H
#define HARNESS_H
#include <stdio.h>
#include <string.h>
#include "../engine/vtengine.h"

extern int h_failures, h_checks;
extern char h_reply[1024];
extern int h_reply_len;
extern int h_damage_calls, h_scroll_calls, h_bells, h_layout_which, h_layout_value;

#define CHECK(cond) do { h_checks++; if (!(cond)) { h_failures++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_STR(got, want) do { const char *g_ = (got), *w_ = (want); h_checks++; \
    if (strcmp(g_, w_)) { h_failures++; printf("  FAIL %s:%d:\n    got  [%s]\n    want [%s]\n", \
    __FILE__, __LINE__, g_, w_); } } while (0)
#define CHECK_INT(got, want) do { long g_ = (long)(got), w_ = (long)(want); h_checks++; \
    if (g_ != w_) { h_failures++; printf("  FAIL %s:%d: %s = %ld, want %ld\n", \
    __FILE__, __LINE__, #got, g_, w_); } } while (0)

vt_term *h_new(int cols, int rows, enum vt_personality p);
void h_put(vt_term *t, const char *s);
/* Row as UTF-8 text with trailing blanks cut. */
const char *h_row(vt_term *t, int row);
/* The whole screen, rows joined with '|', trailing blank rows cut. */
const char *h_screen(vt_term *t);
const vt_cell *h_cell(vt_term *t, int x, int y);
void h_reply_clear(void);

typedef void (*h_suite_fn)(void);
typedef struct { const char *name; h_suite_fn fn; } h_suite;
#endif
