/* The Amarchy bar, the portable core (roadmap 2026-10-26-amarchy-roadmap.md phase 2, R2.4): the segment list,
 * the layout (left group from the left edge, right group from the right edge) and the hit-test. No OS calls,
 * so tests/test_bar.c covers it; bar.c draws into the rectangles it gives.
 *
 * A segment is an id, a side, a text and the pixel width of that text. The clock, RAM, CPU, network and
 * workspace segments of the roadmap add ids and a bar_add call; nothing else here changes. */
#ifndef AMARCHY_BAR_CORE_H
#define AMARCHY_BAR_CORE_H

#define BAR_MAX_SEGS 24                 /* screens list (up to 10), marker, clock, CPU, RAM, network, Agents, system, tray */
#define BAR_TEXT 32

enum { BAR_LEFT = 0, BAR_RIGHT, BAR_CENTRE };
enum { BAR_SEG_AGENTS = 1 };
/* The screens list (left group): one segment per public screen, ids BAR_SEG_SCREEN0 .. +BAR_MAX_SEGS-1 in list order,
 * then BAR_SEG_MORE, the "+N" marker bar_layout fills in when it cuts entries from the list's right end. */
enum { BAR_SEG_MORE = 199, BAR_SEG_SCREEN0 = 200 };
#define BAR_IS_SCREEN(id) ((id) >= BAR_SEG_SCREEN0 && (id) < BAR_SEG_SCREEN0 + 100)                   /* later: BAR_SEG_CLOCK, BAR_SEG_RAM, BAR_SEG_CPU, ... */
enum { BAR_BTN_LEFT = 1, BAR_BTN_RIGHT };

/* The keymap row of the agents panel that shows limit meters and token use (OMARCHY-AI part E). Part E is not
 * built: the row does not exist yet, so the panel shows no number. */
#define BAR_AGENT_LIMITS_ROW "agents.limits"
/* The terminal action a right click on the agents segment fires: the hotkey super+l's (dist/keymap), so the bar and
 * the key start the agent the same way. */
#define BAR_LAUNCH_MENUID "MENU_AGENT"
/* The bar window's title: win_apply.c finds the bar on a screen by it (bar_usable), win_is_amarchy skips it. */
#define BAR_TITLE "Amarchy bar"
/* The node bar.c opens for the agents panel. */
#define BAR_AGENT_NODE "agents"

/* What a click on a segment does; bar.c carries it out. */
enum { BAR_ACT_NONE = 0, BAR_ACT_PANEL, BAR_ACT_LAUNCH_AGENT };

typedef struct bar_seg {
    int id;
    int side;
    char text[BAR_TEXT];
    int tw;                     /* pixel width of text (TextLength) */
    int x, w;                   /* placed by bar_layout: left edge and width incl. padding */
    int dirty;                  /* text or place changed since the last bar_take_dirty */
    int hidden;                 /* cut by bar_layout (w = 0, never hit) */
} bar_seg;

typedef struct bar {
    bar_seg seg[BAR_MAX_SEGS];
    int n;
    int width;                  /* the bar's width, pixels */
    int pad;                    /* inside a segment, each side */
    int gap;                    /* between segments */
    int cw;                     /* pixel width of one character, for the "+N" marker (bar.c sets it; default 8) */
} bar;

void bar_init(bar *b, int width, int pad, int gap);
/* Appends a segment; its index, or -1 when full. */
int bar_add(bar *b, int id, int side, const char *text, int tw);
/* New text for segment `id`; 1 when it changed (the segment is marked dirty and the bar must be laid out
 * again), else 0 and nothing is touched: a tick that sampled the same value costs no redraw. */
int bar_set_text(bar *b, int id, const char *text, int tw);
/* Places every segment: the left group from x = 0 in order, the right group ending at the width, in order, the
 * centre group centred on the width and pushed right of the left group when they would touch. When the left group
 * and the centre do not fit between the left edge and the right group, the screens list loses entries from its
 * right end (at least one stays) and the BAR_SEG_MORE marker shows "+N"; the centre and right never cut.
 * A segment whose place changed is dirty. */
void bar_layout(bar *b, int width);
/* The index of the segment under x, or -1. */
int bar_hit(const bar *b, int x);
/* What a click at x does: BAR_ACT_*, with the segment's id in *id (0 when none). */
int bar_click(const bar *b, int x, int button, int *id);
/* A button press seen on the input stream, not through the window (the window only hears the active window's presses):
 * the pointer at (px, py) in screen pixels, the bar's rectangle at y = top, height h, x from 0 over the bar's width.
 * BAR_ACT_* of the segment under the pointer when the pointer is inside the rectangle, else BAR_ACT_NONE. */
int bar_press_act(const bar *b, int px, int py, int top, int h, int button);
/* Index of the next dirty segment at or after `from`, clearing its flag; -1 when none. */
int bar_take_dirty(bar *b, int from);

/* Re-raise (Intuition 3.x has no stay-on-top flag): the layers in front of the bar's, front first, as plain
 * rectangles (inclusive edges, screen pixels). `ours` marks Amarchy's own windows (the menu, the agents panel), which
 * stay above the bar and never trigger a raise. */
#define BAR_RAISE_POLL_MICROS 500000UL
typedef struct bar_lyr {
    int l, t, r, b;
    int ours;
} bar_lyr;

/* 1 when a layer that is not ours, in front of the bar, overlaps the bar's rectangle (l, t, r, b inclusive): the
 * bar must come to the front. */
int bar_covered(int l, int t, int r, int b, const bar_lyr *front, int n);

/* The point the agents panel hangs from: its right edge at the screen's right edge, its bottom edge on the
 * bar's top (mg_screen.anchored; mg_layout clamps it on screen). */
void bar_panel_anchor(int screen_w, int bar_top, int *ax, int *ay);

/* Formatters (pure). Output is always NUL terminated inside `cap`.
 * Bytes: "512K" below 1 MB (rounded), "1.2M" below 10 MB, "61M" above; exact to 4 GB - 1 ("4096M"). */
void bar_fmt_bytes(char *out, int cap, unsigned long n);
/* The RAM line: "C 1.2M F 61M". */
void bar_fmt_ram(char *out, int cap, unsigned long chip, unsigned long fast);
/* strftime subset %H %M %a %d %b %Y %%; weekday 0 = Sunday, mon 0 = January; any other %x is copied as is. */
void bar_fmt_time(char *out, int cap, const char *fmt, int wday, int day, int mon, int year, int hour, int min);
#define BAR_CLOCK_FMT "%a %H:%M"            /* "Mon 14:05" */
#define BAR_DATE_FMT "%a %d %b %Y"          /* "Mon 29 Oct 2026" */
/* Busy percent 0..100 from idle and total counts (total 0 gives 0). */
int bar_fmt_percent(unsigned long idle, unsigned long total);
/* Screen short name: the title's first word, at most 8 characters. */
void bar_fmt_screen(char *out, int cap, const char *title);

#endif
