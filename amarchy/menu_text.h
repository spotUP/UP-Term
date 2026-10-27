/* menu_text -- the Amarchy menu's text: one object that measures and draws a string with the configured font,
 * TrueType (antialiased through render/aa_ink) or an Amiga bitmap font (Text()). Every draw and every measure of
 * amarchy-gui goes through it, so truncation uses the metrics the drawing uses (plan
 * 2026-10-26-amarchy-gui-aa-font.md, A2). The layout is amarchy/menu_text_core.c, host-tested. */
#ifndef MENU_TEXT_H
#define MENU_TEXT_H

#include <exec/types.h>

struct Screen;
struct RastPort;
struct TextFont;
typedef struct menu_text menu_text;

/* The font of spec "name:size" (the AM_FONT value) on scr: TrueType when the name resolves to a .ttf in FONTS:
 * (the lookup the terminal uses, render/outline.c vo_ttf_spec), else the bitmap font menu_font_open picks
 * (square: the screen's pixels are square). 0 when there is no memory. */
menu_text *mt_open(struct Screen *scr, int square, const char *spec);
void mt_close(menu_text *m);

int mt_is_ttf(const menu_text *m);
/* the line: the height of a row of text, and the rows from its top to the baseline */
int mt_height(const menu_text *m);
int mt_baseline(const menu_text *m);
/* the bitmap font to SetFont for drawing that is not text (the caller's own), 0 for TrueType */
struct TextFont *mt_bitmap_font(const menu_text *m);

int mt_width(menu_text *m, const char *text);
/* pixels the first len bytes take; the bytes of the first len that fit maxw pixels (whole code points) */
int mt_width_n(menu_text *m, const char *text, int len);
int mt_prefix(menu_text *m, const char *text, int len, int maxw);
/* antialiased glyphs mt_draw has drawn since mt_open (0 for a bitmap font): the rig's proof the TrueType path ran */
long mt_aa_glyphs(const menu_text *m);
/* text cut with an ellipsis to fit maxw pixels into out (n bytes); the pixel width of out */
int mt_fit(menu_text *m, const char *text, int maxw, char *out, int n);
/* mt_fit that keeps text[keep..] whole (see mtc_fit_tail) */
int mt_fit_tail(menu_text *m, const char *text, int keep, int maxw, char *out, int n);
/* text with its line top at x, y of rp, fg over bg (0xRRGGBB). The caller has not filled the background of
 * TrueType text: it is drawn over bg. */
void mt_draw(menu_text *m, struct RastPort *rp, int x, int y, const char *text, ULONG fg, ULONG bg);

#endif
