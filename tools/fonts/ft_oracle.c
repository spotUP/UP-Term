/* ft_oracle: FreeType reference coverage for the TrueType rasterizer tests.
 * usage: ft_oracle <font.ttf> <cw> <ch> <baseline> <hexcp>...
 * Prints "<cp hex> <cw*ch coverage bytes as hex>", row 0 = top row.
 * Built by tools/fonts/make_ttf_testdata.py. No hinting, non-zero winding. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

int main(int argc, char **argv)
{
    FT_Library lib;
    FT_Face face;
    int cw, ch, baseline, i, k;
    if (argc < 6) { fprintf(stderr, "usage: ft_oracle font cw ch baseline hexcp...\n"); return 2; }
    cw = atoi(argv[2]); ch = atoi(argv[3]); baseline = atoi(argv[4]);
    if (FT_Init_FreeType(&lib) || FT_New_Face(lib, argv[1], 0, &face)) { fprintf(stderr, "open failed\n"); return 1; }
    if (FT_Set_Pixel_Sizes(face, (FT_UInt)cw, (FT_UInt)ch)) { fprintf(stderr, "size failed\n"); return 1; }
    for (i = 5; i < argc; i++) {
        unsigned long cp = strtoul(argv[i], NULL, 16);
        unsigned char *buf = (unsigned char *)calloc((size_t)cw * ch, 1);
        FT_Bitmap bm;
        if (FT_Load_Char(face, cp, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP)) { fprintf(stderr, "load %lx failed\n", cp); return 1; }
        if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) { fprintf(stderr, "no outline %lx\n", cp); return 1; }
        FT_Outline_Translate(&face->glyph->outline, 0, (FT_Pos)(ch - baseline) * 64);
        memset(&bm, 0, sizeof bm);
        bm.rows = (unsigned)ch; bm.width = (unsigned)cw; bm.pitch = cw;
        bm.buffer = buf; bm.pixel_mode = FT_PIXEL_MODE_GRAY; bm.num_grays = 256;
        if (FT_Outline_Get_Bitmap(lib, &face->glyph->outline, &bm)) { fprintf(stderr, "render %lx failed\n", cp); return 1; }
        printf("%lX ", cp);
        for (k = 0; k < cw * ch; k++) printf("%02x", buf[k]);
        printf("\n");
        free(buf);
    }
    FT_Done_Face(face); FT_Done_FreeType(lib);
    return 0;
}
