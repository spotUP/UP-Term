#include "fontpair.h"

/* a and b the same font name, any case, ".font" optional on either */
static int same_font(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (x != y)
            break;
    }
    if (!*a && !*b)
        return 1;
    /* one ends where the other has ".font" left */
    if (!*a) {
        const char *f = ".font";
        for (; *b && *f; b++, f++)
            if ((*b >= 'A' && *b <= 'Z' ? *b + 32 : *b) != *f)
                return 0;
        return !*b && !*f;
    }
    if (!*b)
        return same_font(b, a);
    return 0;
}

static const struct {
    const char *tall;
    int tall_size;      /* 0: any size is the face */
    const char *square;
    int square_size;
} pairs[] = {
    { "topaz.font", 8, "TopazPro.font", 16 },   /* topaz 8 redrawn for square pixels (spot, 2022);
                                                 * topaz 9 and 11 are other designs, chosen as such */
    { "IBM.font", 8, "IBM.font", 16 },          /* the VGA 8x16 IBM PC font */
};

void fontpair_choose(const char *name, int size, int square, int square_fits, fontpair_choice *out)
{
    int i;
    out->name = name;
    out->size = size;
    out->changed = 0;
    for (i = 0; i < (int)(sizeof(pairs) / sizeof(pairs[0])); i++) {
        int is_tall = same_font(name, pairs[i].tall) && (!pairs[i].tall_size || size == pairs[i].tall_size);
        int is_square = same_font(name, pairs[i].square) && size == pairs[i].square_size;
        if (!is_tall && !is_square)
            continue;
        if (square && square_fits) {
            out->name = pairs[i].square;
            out->size = pairs[i].square_size;
            out->changed = !is_square;
        } else {
            out->name = pairs[i].tall;
            out->size = pairs[i].tall_size ? pairs[i].tall_size : 8;
            out->changed = !is_tall;
        }
        return;
    }
}

void fontpair_face(const char *face, int square, int square_fits, fontpair_choice *out)
{
    int i;
    for (i = 0; i < (int)(sizeof(pairs) / sizeof(pairs[0])); i++)
        if (same_font(face, pairs[i].tall)) {
            fontpair_choose(face, pairs[i].tall_size ? pairs[i].tall_size : 8, square, square_fits, out);
            return;
        }
    out->name = face;
    out->size = 0;
    out->changed = 0;
}
