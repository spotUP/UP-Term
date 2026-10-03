/* fontpair: one face drawn twice -- for the Amiga's tall pixels (PAL /
 * NTSC hires) and for square ones (graphics cards, AGA hires interlaced)
 * -- and which of the two a screen gets (plan
 * 2026-10-03-screens-and-dctelnet.md P2; DCTelnet's src/screenfont.c).
 * Pairs: topaz 8 <-> TopazPro 16, IBM 8 <-> IBM 16. A font in no
 * pair is used as it is. Portable C89, host-tested (tests/test_fontpair.c). */
#ifndef FONTPAIR_H
#define FONTPAIR_H

typedef struct fontpair_choice {
    const char *name;   /* "topaz.font", "TopazPro.font", "IBM.font" or the input name */
    int size;
    int changed;        /* 1 when it is the other font of a pair */
} fontpair_choice;

/* name, size: the font asked for ("topaz", "topaz.font"; any case).
 * square: the screen's pixels are square (resolution x == y).
 * square_fits: the square font's cells give the screen 25 rows; when not,
 * the tall one stays (a 16-pixel font on a 256-line screen). */
void fontpair_choose(const char *name, int size, int square, int square_fits, fontpair_choice *out);

/* The pair the face belongs to (any size of "topaz": topaz 8 /
 * TopazPro 16), chosen as fontpair_choose does for its paired size; a
 * face in no pair: changed 0, its name, size 0. For stepping sizes
 * through a face (vtwin_font_step). */
void fontpair_face(const char *face, int square, int square_fits, fontpair_choice *out);

#endif
