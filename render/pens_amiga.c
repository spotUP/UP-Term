/* pens_amiga -- see pens_amiga.h. The ObtainBestPen call behind pen_rule.h's exact-then-nearest rule. */
#include "pens_amiga.h"
#include "pen_rule.h"
#include <exec/types.h>
#include <graphics/view.h>
#include <utility/tagitem.h>
#include <proto/graphics.h>

/* the screen's ObtainBestPen for pen_rule.h: exact (a free entry gets the colour) or the nearest */
long vr_pen_best(void *u, unsigned long rgb, int exact)
{
    static const struct TagItem exact_tags[] = {
        { OBP_Precision, (ULONG)PRECISION_EXACT },
        { OBP_FailIfBad, TRUE },
        { TAG_DONE, 0 }
    };
    static const struct TagItem near_tags[] = {
        { OBP_Precision, PRECISION_IMAGE },
        { TAG_DONE, 0 }
    };
    ULONG cr = (rgb >> 16) & 0xFF, cg = (rgb >> 8) & 0xFF, cb = rgb & 0xFF;
    return ObtainBestPenA((struct ColorMap *)u, cr * 0x01010101UL, cg * 0x01010101UL, cb * 0x01010101UL,
                          (struct TagItem *)(exact ? exact_tags : near_tags));
}

long vr_pen_find(void *u, unsigned long rgb, int exact)
{
    struct ColorMap *cm = (struct ColorMap *)u;
    const struct ViewPort *vp = cm ? cm->cm_vp : 0;
    /* the screen's own pens only: a 16-colour AGA map has more entries (sprite colours) than the planes reach */
    LONG max = vp && vp->RasInfo && vp->RasInfo->BitMap ? (1L << vp->RasInfo->BitMap->Depth) - 1 : -1;
    if (!cm || exact < 0) /* FindColor is always nearest; exact is kept for vr_pen_best's signature */
        return -1;
    return FindColor(cm, ((rgb >> 16) & 0xFF) * 0x01010101UL, ((rgb >> 8) & 0xFF) * 0x01010101UL, (rgb & 0xFF) * 0x01010101UL, max);
}

long vr_pen_obtain(struct ColorMap *cm, unsigned long rgb)
{
    if (!cm)
        return -1;
    return vr_obtain_rule(vr_pen_best, cm, rgb);
}
