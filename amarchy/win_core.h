/* The pure part of the win: action kind (config/keymap.c win_arg_ok is its text check): the verb parse and all the
 * window geometry arithmetic. No OS headers, no Amiga types: the commodity (amarchy/win.c) copies a window's box
 * into a win_box, calls these, and hands the result to Intuition. Host-tested by tests/test_win.c. */
#ifndef AMARCHY_WIN_CORE_H
#define AMARCHY_WIN_CORE_H

#define WIN_STEP      32   /* move / size without a pixel count */
#define WIN_RING_MAX  8    /* windows win:save remembers; the oldest is dropped */

typedef enum {
    WV_NONE = 0, WV_CYCLE, WV_FRONT, WV_BACK, WV_ZIP, WV_FULLWIDTH, WV_FULLSCREEN, WV_SAVE, WV_RESTORE,
    WV_MOVE, WV_SIZE, WV_FOCUS, WV_CLOSEDRAWERS, WV_SCREEN, WV_TITLE
} win_verb;

typedef enum {
    WA_NONE = 0, WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN, WA_NEXT, WA_PREV, WA_WORKBENCH, WA_BACK,
    WA_TOGGLE, WA_SHOW, WA_HIDE
} win_arg;

typedef struct win_cmd {
    win_verb verb;
    win_arg arg;
    int pixels;            /* move / size: the step, WIN_STEP when none was given */
} win_cmd;

/* The text after "win:". 1 and *c filled when text is one of the closed forms, else 0 and *c cleared. */
int win_parse(const char *text, win_cmd *c);

typedef struct win_box {
    int left, top, width, height;
    int minw, minh;        /* Intuition's MinWidth / MinHeight */
    int maxw, maxh;        /* MaxWidth / MaxHeight; 0 or less means the screen is the limit */
    long screen;           /* opaque id of the screen the window sits on */
} win_box;

typedef struct win_screen {
    int width, height;
    int title;             /* height of the screen's title bar: fullscreen starts below it */
} win_screen;

/* Each returns 1 when the box changed, 0 when it did not (a no-op is not an error). */
int win_move(win_box *b, win_arg dir, int pixels, const win_screen *s);   /* clamped inside the screen */
int win_size(win_box *b, win_arg dir, int pixels, const win_screen *s);   /* left/up shrink, right/down grow; min, max, screen */
int win_fullwidth(win_box *b, const win_screen *s);                       /* left 0, the screen's width */
int win_fullscreen(win_box *b, const win_screen *s);                      /* the area below the title bar */

/* 1 when a window is a full-screen terminal's: backdrop and borderless (the handler's FULLSCREEN window,
 * create_window in handler/vtcon_handler.c) and exactly the screen's size at 0,0. */
int win_is_fullscreen_terminal(int backdrop_borderless, int left, int top, int width, int height, const win_screen *s);

/* win:save / win:restore: a ring of the last WIN_RING_MAX windows, keyed by an opaque id (the window pointer). */
typedef struct win_ring {
    long id[WIN_RING_MAX];
    win_box box[WIN_RING_MAX];
    int n;                 /* used slots, oldest first */
} win_ring;

void win_ring_clear(win_ring *r);
void win_save(win_ring *r, long id, const win_box *b);          /* saving an id again replaces it and makes it the newest */
int win_restore(const win_ring *r, long id, win_box *out);      /* 1 and *out when id is in the ring (left kept there) */

/* win:focus: the index in boxes[0..n-1] of the nearest window in dir (WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN), measured
 * between centres; WA_NEXT / WA_PREV step by array index and wrap. Never the target itself, never a box on another
 * screen than boxes[target]. -1 when there is none. */
/* K5a: a target ticket stands only while its window is still a usable target and its box (left, top, width,
 * height) is the one recorded when the menu opened; anything else and the verb must refuse. */
int win_ticket_valid(int usable, const win_box *taken, const win_box *now);

int win_pick(const win_box *boxes, int n, int target, win_arg dir);

#endif
