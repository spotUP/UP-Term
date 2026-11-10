/* gadprobe [TITLE] -- list every string and boopsi gadget of every window (and of each window's requesters), to learn what the
 * text input gadgets are (input-ground map). One line per gadget into a static buffer under LockIBase; the buffer is printed
 * only after UnlockIBase (printing under the lock deadlocks the console). No allocation and no printing under the lock.
 * Line: W"title" L=<list> T=<type hex> F=<flags hex> A=<activation hex> box=l,t,w,h SI=<0/1> [EXT=<0/1> SE=<0/1>
 *       P=a,b AP=a,b] [CL=id/id/...]  where list is FG (win->FirstGadget) or RQ (a requester's ReqGadget).
 * Optional TITLE: only windows whose title contains that substring. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/sghooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
long __stack = 16384;
const char gadprobe_stack_cookie[] = "$STACK: 16384";

#define MAXLINES 60
#define LINELEN 200
static char g_buf[MAXLINES][LINELEN];
static int g_n, g_over;
static char *g_p;

static void put(const char *s) { while (*s && g_p < g_buf[g_n] + LINELEN - 2) *g_p++ = *s++; }
static void puthex(unsigned long v)
{
    static const char hx[] = "0123456789abcdef";
    char t[9];
    int i = 0;
    do { t[i++] = hx[v & 15]; v >>= 4; } while (v && i < 8);
    while (i && g_p < g_buf[g_n] + LINELEN - 2) *g_p++ = t[--i];
}
static void putcpy(const char *s, int cap)
{
    int i;
    if (!s) { put("(null)"); return; }
    for (i = 0; s[i] && i < cap; i++) if (g_p < g_buf[g_n] + LINELEN - 2) *g_p++ = s[i];
}
static int contains(const char *h, const char *n)
{
    int i, j;
    if (!n || !*n) return 1;
    if (!h) return 0;
    for (i = 0; h[i]; i++) { for (j = 0; n[j] && h[i + j] == n[j]; j++) ; if (!n[j]) return 1; }
    return 0;
}

static void gadget(struct Window *w, const char *list, struct Gadget *g)
{
    unsigned type = g->GadgetType & GTYP_GTYPEMASK;
    if (type != GTYP_STRGADGET && type != GTYP_CUSTOMGADGET) return;
    if (g_n >= MAXLINES) { g_over++; return; }
    g_p = g_buf[g_n];
    put("W\""); putcpy((const char *)w->Title, 16); put("\" L="); put(list);
    put(" T="); puthex(g->GadgetType); put(" F="); puthex(g->Flags); put(" A="); puthex(g->Activation);
    put(" box="); puthex((unsigned short)g->LeftEdge); put(","); puthex((unsigned short)g->TopEdge); put(",");
    puthex((unsigned short)g->Width); put(","); puthex((unsigned short)g->Height);
    put(" SI="); put(g->SpecialInfo ? "1" : "0");
    if (type == GTYP_STRGADGET && g->SpecialInfo) {
        struct StringInfo *si = (struct StringInfo *)g->SpecialInfo;
        put(" EXT="); put(si->Extension ? "1" : "0");
        put(" SE="); put((g->Activation & GACT_STRINGEXTEND) ? "1" : "0");
        if (si->Extension) {
            struct StringExtend *se = si->Extension;
            put(" P="); puthex(se->Pens[0]); put(","); puthex(se->Pens[1]);
            put(" AP="); puthex(se->ActivePens[0]); put(","); puthex(se->ActivePens[1]);
        }
    }
    if (type == GTYP_CUSTOMGADGET) {
        struct IClass *cl = OCLASS((Object *)g);
        int d;
        put(" CL=");
        for (d = 0; cl && d < 8; d++) {
            if (d) put("/");
            if (cl->cl_ID) putcpy((const char *)cl->cl_ID, 24); else put("(private)");
            cl = cl->cl_Super;
        }
    }
    *g_p = 0;
    g_n++;
}

static void list(struct Window *w, const char *name, struct Gadget *g)
{
    int n;
    for (n = 0; g && n < 128; g = g->NextGadget, n++) gadget(w, name, g);
}

int main(int argc, char **argv)
{
    const char *filter = argc > 1 ? argv[1] : "";
    ULONG lock;
    struct Screen *s;
    int i;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase) return 20;
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen) {
        struct Window *w;
        for (w = s->FirstWindow; w; w = w->NextWindow) {
            struct Requester *r;
            if (!contains((const char *)w->Title, filter)) continue;
            list(w, "FG", w->FirstGadget);
            for (r = w->FirstRequest; r; r = r->OlderRequest) list(w, "RQ", r->ReqGadget);
        }
    }
    UnlockIBase(lock);
    for (i = 0; i < g_n; i++) printf("%s\n", g_buf[i]);
    printf("gadprobe: %d lines%s\n", g_n, g_over ? " (cap hit)" : "");
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
