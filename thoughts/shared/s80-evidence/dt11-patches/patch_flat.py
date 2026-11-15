R='/Users/spot/Code/UP-Term/vtcon-s82-dt11/'
def sub(p,a,b):
    s=open(R+p).read()
    assert s.count(a)==1,(p,a[:60])
    open(R+p,'w').write(s.replace(a,b))
sub('patina/flat_shared.h','    ULONG d5draws;              /* sentinel: lock / iconify images drawn by flat */\n',
'''    ULONG d5draws;              /* sentinel: lock / iconify images drawn by flat */
    ULONG darrow;               /* DT-11 sentinel: lister toolbar "<<" arrows drawn by flat (d4_renderimage) */
''')
sub('patina/flat.c','static const LONG d4_lvo[FLAT_EL_COUNT] = { FLAT_LVO_DRAWBOX, FLAT_LVO_FIELDBOX, 0 };   /* the bevel has no LVO */',
'''/* DT-11: dopus5 RenderImage (LVO -654, a0 rp, a1 Image_Data, d0 left, d1 top, a2 tags). The lister toolbar "<<" arrow
 * (lister_toolbar.c: IM_Rectangle, IM_ClipBoundary, IM_NoIconRemap, on release IM_State 0 and IM_Erase 0) is told by the
 * basename of its path, Image_Data node.ln_Name (flat_dopus_arrow_name), and drawn as the flat glyph in the screen's
 * text pen, read on every call, so a theme switch shows at the next toolbar redraw. Every other call: the saved routine. */
typedef short (*renderfn)(__reg("a0") struct RastPort *, __reg("a1") APTR, __reg("d0") LONG, __reg("d1") LONG,
                          __reg("a2") struct TagItem *, __reg("a6") struct Library *);

#define D4_IM_RECTANGLE (TAG_USER + 3)
#define D4_IM_CLIP (TAG_USER + 6)
#define D4_IM_ERASE (TAG_USER + 7)

/* the data of tag in the list, def when absent; the TAG_IGNORE / MORE / SKIP walk of utility's FindTagItem */
static ULONG d4_tag(struct TagItem *t, ULONG tag, ULONG def, int *found)
{
    int guard = 64;
    while (t && guard-- > 0) {
        if (t->ti_Tag == TAG_DONE)
            break;
        if (t->ti_Tag == TAG_MORE) {
            t = (struct TagItem *)t->ti_Data;
            continue;
        }
        if (t->ti_Tag == TAG_SKIP) {
            t += t->ti_Data + 1;
            continue;
        }
        if (t->ti_Tag == tag) {
            if (found)
                *found = 1;
            return t->ti_Data;
        }
        t++;
    }
    return def;
}

static int d4_arrow(struct RastPort *rp, APTR image, struct TagItem *tags)
{
    flatshared *r = g_r;
    const flat_state *st;
    struct Rectangle *rc;
    int cb, erased = 0, erase, drawn, x0, y0, x1, y1;
    UBYTE fg;
    if (!rp || !image || !r->st.enabled || r->st.held || !flat_dopus_arrow_name((const char *)((struct Node *)image)->ln_Name))
        return 0;
    rc = (struct Rectangle *)d4_tag(tags, D4_IM_RECTANGLE, 0, 0);
    st = st_rp(r, rp);
    if (!rc || !st->enabled)
        return 0;
    cb = (int)d4_tag(tags, D4_IM_CLIP, 2, 0) >> 1;
    erase = (int)d4_tag(tags, D4_IM_ERASE, 0, &erased);
    x0 = rc->MinX + cb; y0 = rc->MinY + cb; x1 = rc->MaxX - cb; y1 = rc->MaxY - cb;
    if (x1 < x0 || y1 < y0)
        return 0;
    /* the stock image is opaque with colour 0 behind its arrow (IM_NoIconRemap: pen 0); IM_Erase names the pen itself */
    fg = rp->FgPen;
    c2_fill(rp, x0, y0, x1, y1, erased ? erase : 0);
    drawn = flat_glyph_dleft(x0, y0, x1 - x0 + 1, y1 - y0 + 1, rp_side(r, rp), st->pen_text, c2_fill, rp) > 0;
    SetAPen(rp, fg);                    /* the caller's pen as the stock routine leaves it for its own text */
    if (drawn)
        r->darrow++;
    return drawn;
}

static short d4_renderimage(__reg("a0") struct RastPort *rp, __reg("a1") APTR image, __reg("d0") LONG left, __reg("d1") LONG top,
                            __reg("a2") struct TagItem *tags)
{
    if (d4_arrow(rp, image, tags))
        return 1;
    return ((renderfn)g_r->dsw[FLAT_EL_RENDERIMAGE].saved)(rp, image, left, top, tags, g_r->dlib);
}

static const LONG d4_lvo[FLAT_EL_COUNT] = { FLAT_LVO_DRAWBOX, FLAT_LVO_FIELDBOX, 0, FLAT_LVO_RENDERIMAGE };   /* the bevel has no LVO */''')
sub('patina/flat.c','''            void *ours = el == FLAT_EL_DRAWBOX ? (void *)d4_drawbox : el == FLAT_EL_FIELDBOX ? (void *)d4_fieldbody : (void *)d4_bevelbody;''',
'''            void *ours = el == FLAT_EL_DRAWBOX ? (void *)d4_drawbox : el == FLAT_EL_FIELDBOX ? (void *)d4_fieldbody : (void *)d4_bevelbody;   /* RenderImage: no body prologue (flat_body_prologue 0) */''')
sub('patina/flat.c','''            uintptr_t ours = el == FLAT_EL_DRAWBOX ? (uintptr_t)d4_drawbox : (uintptr_t)d4_fieldbox;''',
'''            uintptr_t ours = el == FLAT_EL_DRAWBOX ? (uintptr_t)d4_drawbox : el == FLAT_EL_FIELDBOX ? (uintptr_t)d4_fieldbox
                                                                                                   : (uintptr_t)d4_renderimage;''')
sub('patina/flat.c','    uint32_t dsum[FLAT_EL_COUNT] = { 0, 0, 0 };\n    unsigned long dbevel = 0, dface = 0, dnopix = 0, d5draws = 0;',
'    uint32_t dsum[FLAT_EL_COUNT] = { 0, 0, 0, 0 };\n    unsigned long dbevel = 0, dface = 0, dnopix = 0, d5draws = 0, darrow = 0;')
sub('patina/flat.c','d5draws = r->d5draws;\n        memcpy(dsum,','d5draws = r->d5draws; darrow = r->darrow;\n        memcpy(dsum,')
sub('patina/flat.c','''        put_word(line, &n, " d5=");                 put_num(line, &n, d5draws);\n''','''        put_word(line, &n, " d5=");                 put_num(line, &n, d5draws);
        put_word(line, &n, " arrow=");              put_num(line, &n, darrow);
''')
print('ok')
