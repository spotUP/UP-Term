/* The Amarchy notice queue, the portable core (roadmap 2026-10-29-amarchy-p4-capture-notify-power.md R4.8, decision
 * D4 corner, D5 timeout and stacking). No OS calls and no clock of its own: every call that needs the time gets
 * it as seconds from the caller, so tests/test_note_core.c runs without real time. note.c (the port and the
 * window) feeds it the tick's seconds and draws the rectangles it gives.
 *
 * A notice has a title, a body and a timeout in seconds: NOTE_SECS_DEFAULT when the sender gives none, 0 = stays
 * until clicked. At most NOTE_MAX_SHOWN are shown; a newer one pushes the oldest out. The newest sits nearest the
 * corner, older ones stack away from it. */
#ifndef AMARCHY_NOTE_CORE_H
#define AMARCHY_NOTE_CORE_H

#define NOTE_MAX_SHOWN 3
#define NOTE_TITLE 48
#define NOTE_BODY 160
#define NOTE_SECS_DEFAULT 5
#define NOTE_SECS_MAX 3600
#define NOTE_GAP 4                  /* pixels between two stacked notices and from the box edge */

enum { NOTE_TOP_RIGHT = 0, NOTE_TOP_LEFT, NOTE_BOTTOM_RIGHT, NOTE_BOTTOM_LEFT };   /* D4 default: top right */

/* note_parse results */
enum { NOTE_OK = 0, NOTE_ERR_EMPTY = -1, NOTE_ERR_SECS = -2 };

typedef struct note {
    long id;                        /* 1, 2, ... in post order, never reused */
    char title[NOTE_TITLE];
    char body[NOTE_BODY];
    long expires;                   /* absolute second it goes; meaningless when sticky */
    int sticky;                     /* timeout 0: stays until note_dismiss */
} note;

typedef struct note_q {
    note item[NOTE_MAX_SHOWN];      /* item[0] oldest ... item[n-1] newest */
    int n;
    long posted;                    /* every notice ever accepted: STATUS notes= */
    long dropped;                   /* pushed out by a newer one while shown */
    long expired;                   /* gone by timeout */
    long next_id;
} note_q;

typedef struct note_box {           /* the area notices may use (bar_usable(), or the whole screen), inclusive */
    int x0, y0, x1, y1;
} note_box;

void note_init(note_q *q);

/* One request line "title|body|seconds". A literal | in title or body is written \| and a literal \ is \\.
 * body and seconds may be absent ("title", "title|body"); an empty seconds field means the default. Over-long
 * title or body are cut to fit. seconds is a decimal 0..NOTE_SECS_MAX, written alone (no sign, no letters), and
 * out_secs is -1 when absent. Returns NOTE_OK, NOTE_ERR_EMPTY (no title) or NOTE_ERR_SECS. */
int note_parse(const char *line, char *title, char *body, long *out_secs);

/* Add a notice at time now. secs < 0 = NOTE_SECS_DEFAULT, 0 = sticky. A full queue drops the oldest first.
 * Returns the new id. */
long note_post(note_q *q, const char *title, const char *body, long secs, long now);

/* Remove every notice whose time is up: expires <= now (so a 5 s notice posted at t0 is gone at t0 + 5 exactly).
 * Returns how many went. */
int note_tick(note_q *q, long now);

/* Remove the notice at queue index idx (0 = oldest); the rest keep their order. Returns 0, or -1 for a bad index. */
int note_dismiss(note_q *q, int idx);

/* Remove everything (the CLEAR verb). Returns how many were shown. */
int note_clear(note_q *q);

/* The top left corner of a w by h notice, slot 0 = nearest the corner, slot 1 = next away from it, ... inside box.
 * Returns 0, or -1 when the slot would leave the box (the caller shows nothing there). */
int note_slot(int corner, const note_box *box, int w, int h, int slot, int *x, int *y);

/* Which shown notice (queue index) a point hits, -1 for none. The newest has slot 0. */
int note_hit(const note_q *q, int corner, const note_box *box, int w, int h, int px, int py);

#endif
