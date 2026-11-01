/* winprobe: the windows of the Workbench screen front to back, as the bar's front_layers sees them (BAR-ZOOMED-DRAWER):
 *   winprobe        one line per window: "W <title> flags=<hex> btop=<n> bbot=<n> l=<n> r=<n> t=<n> b=<n>" (layer bounds). */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/layers.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <stdio.h>

struct IntuitionBase *IntuitionBase;
struct Library *LayersBase;

int main(void)
{
    struct Screen *s;
    struct Layer *l;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    LayersBase = OpenLibrary("layers.library", 36);
    if (!IntuitionBase || !LayersBase)
        return 20;
    s = LockPubScreen("Workbench");
    if (!s)
        return 10;
    LockLayerInfo(&s->LayerInfo);
    for (l = s->LayerInfo.top_layer; l; l = l->back) {
        struct Window *w = (struct Window *)l->Window;
        printf("W \"%s\" flags=%08lx btop=%d bbot=%d l=%d r=%d t=%d b=%d\n", w && w->Title ? (char *)w->Title : "-",
               w ? (unsigned long)w->Flags : 0UL, w ? w->BorderTop : -1, w ? w->BorderBottom : -1,
               (int)l->bounds.MinX, (int)l->bounds.MaxX, (int)l->bounds.MinY, (int)l->bounds.MaxY);
    }
    UnlockLayerInfo(&s->LayerInfo);
    UnlockPubScreen(NULL, s);
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
