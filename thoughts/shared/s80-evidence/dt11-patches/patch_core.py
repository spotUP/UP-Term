import sys
R='/Users/spot/Code/UP-Term/vtcon-s82-dt11/'
def sub(p,a,b):
    s=open(R+p).read()
    assert s.count(a)==1,(p,a[:60])
    open(R+p,'w').write(s.replace(a,b))
sub('patina/flat_core.h','enum { FLAT_EL_DRAWBOX = 0, FLAT_EL_FIELDBOX = 1, FLAT_EL_BEVEL = 2, FLAT_EL_COUNT = 3 };',
'''/* FLAT_EL_RENDERIMAGE (DT-11): RenderImage (LVO -654), LVO swap only. Only the lister toolbar "<<" arrow
 * (flat_dopus_arrow_name) is drawn flat; every other image goes to the saved routine. */
enum { FLAT_EL_DRAWBOX = 0, FLAT_EL_FIELDBOX = 1, FLAT_EL_BEVEL = 2, FLAT_EL_RENDERIMAGE = 3, FLAT_EL_COUNT = 4 };''')
sub('patina/flat_core.h','#define FLAT_LVO_FIELDBOX (-702)\n','''#define FLAT_LVO_FIELDBOX (-702)
#define FLAT_LVO_RENDERIMAGE (-654) /* dopus5.library RenderImage(rp a0, image a1, left d0, top d1, tags a2) */
''')
sub('patina/flat_core.h','int flat_body_prologue(int el, const unsigned char *body);\n','''int flat_body_prologue(int el, const unsigned char *body);

/* DT-11: 1 when an Image_Data path (node.ln_Name) names the lister toolbar arrow: its basename (after the last ':' or '/')
 * is "ToolbarArrow.image", any case. NULL or any other name: 0 (the image draws stock). */
int flat_dopus_arrow_name(const char *name);
''')
sub('patina/flat_core.c','''    { 73, 8, { 0x780fd46bu, 0xba2cf172u, 0xbdaa116au }, 0xde34u },''','''    { 73, 8, { 0x780fd46bu, 0xba2cf172u, 0xbdaa116au, 0xf8b63678u }, 0xde34u },''')
sub('patina/flat_core.c','''(0x2450c - 0x166d8) */''','''(0x2450c - 0x166d8); DT-11 RenderImage 0x10d88 (LVO -654 jump target): sum from the same hunk, same tool that reproduces the DrawBox and FieldBox sums */''')
sub('patina/flat_core.c','''uint32_t flat_bevel_body(''','''int flat_dopus_arrow_name(const char *name)
{
    static const char want[] = "toolbararrow.image";
    const char *b = name, *p;
    size_t i;
    if (!name)
        return 0;
    for (p = name; *p; p++)
        if (*p == ':' || *p == '/')
            b = p + 1;
    for (i = 0; i < sizeof want - 1; i++) {
        char c = b[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != want[i])
            return 0;
    }
    return b[i] == 0;
}

uint32_t flat_bevel_body(''')
sub('patina/flat_core.c','''" sums=%08lx,%08lx,%08lx", (unsigned long)sums[0], (unsigned long)sums[1],
                     (unsigned long)sums[2]);''','''" sums=%08lx,%08lx,%08lx,%08lx", (unsigned long)sums[0], (unsigned long)sums[1],
                     (unsigned long)sums[2], (unsigned long)sums[3]);''')
sub('patina/flat_glyph.h','int flat_glyph_tick(int ox, int oy, int w, int h, int pen, flat_fill_fn fill, void *ctx);\n','''int flat_glyph_tick(int ox, int oy, int w, int h, int pen, flat_fill_fn fill, void *ctx);
/* DT-11: the DOpus lister toolbar "<<" arrow, two solid left triangles side by side, centred in the w x h box at (ox, oy)
 * with a 1 px margin, in pen. side = columns per visual row (flat_side_px), so the triangles keep their shape on hires.
 * Returns the rectangles drawn, 0 when the box is too small for a 3-row triangle (the caller draws stock). */
int flat_glyph_dleft(int ox, int oy, int w, int h, int side, int pen, flat_fill_fn fill, void *ctx);
''')
sub('patina/flat_glyph.c','int flat_glyph_plan(unsigned long which,','''int flat_glyph_dleft(int ox, int oy, int w, int h, int side, int pen, flat_fill_fn fill, void *ctx)
{
    int hw = (w - 2) / 2, rows, tw, x0, y0, i, k;
    if (!fill || side < 1)
        return 0;
    rows = imin(h - 2, 2 * (hw / side) - 1);    /* a triangle rows high is side * (rows + 1) / 2 wide: both fit the box */
    if (!(rows & 1))
        rows--;
    if (rows < 3)
        return 0;
    tw = side * ((rows + 1) / 2);
    x0 = ox + (w - 2 * tw) / 2;
    y0 = oy + (h - rows) / 2;
    for (i = 0; i < 2; i++)
        for (k = 0; k < rows; k++) {
            int len = side * (imin(k, rows - 1 - k) + 1), xe = x0 + i * tw + tw - 1;
            fill(ctx, xe - len + 1, y0 + k, xe, y0 + k, pen);
        }
    return 2 * rows;
}

int flat_glyph_plan(unsigned long which,''')
print('ok')
