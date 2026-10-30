/* icprobe -- FLAT-R1: IControl ic_GUIGeometry probe (plan 2026-10-29-amarchy-flat-ui-patch.md, research note section R1).
 *   icprobe                 print the ENV:Sys/icontrol.prefs ICTL fields, the Workbench screen address and the borders of a new window
 *   icprobe b0 b1 b2 b3     first write ic_GUIGeometry = b0..b3 (ICB_VERSIONED set, ic_Version 2, ic_VersionMagic 0, every other
 *                           field kept from the file now on disk), wait 4 s for IPrefs, then print as above
 * Output lines: FILE <size>, ICTL flags=<hex> version=<n> hover=<hex> geom=<b0> <b1> <b2> <b3>, SCREEN <addr> bar=<h> wbortop=<n>
 * wborleft=.., WIN left=.. top=.. right=.. bottom=.. w=.. h=. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
long __stack = 8192;
const char icprobe_stack_cookie[] = "$STACK: 8192";

#define PATH "ENV:Sys/icontrol.prefs"
#define ICTL_SIZE 38
static unsigned char buf[256];

static unsigned long be32(const unsigned char *p) { return ((unsigned long)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static void put32(unsigned char *p, unsigned long v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static long load(void)
{
    BPTR f = Open((STRPTR)PATH, MODE_OLDFILE);
    long n = 0;
    if (f) { n = Read(f, buf, sizeof buf); Close(f); }
    return n;
}

/* offset of the ICTL body in buf, 0 when none */
static long ictl(long n)
{
    long p = 12;
    while (p + 8 <= n) {
        unsigned long sz = be32(buf + p + 4);
        if (!memcmp(buf + p, "ICTL", 4)) return p + 8;
        p += 8 + sz + (sz & 1);
    }
    return 0;
}

int main(int argc, char **argv)
{
    long n = load(), o;
    struct Screen *s;
    struct Window *w;
    if (argc == 5) {
        if (!(o = ictl(n))) { /* no file: build FORM PREF + PRHD + ICTL */
            memset(buf, 0, sizeof buf);
            memcpy(buf, "FORM", 4); put32(buf + 4, 4 + 14 + 8 + ICTL_SIZE); memcpy(buf + 8, "PREF", 4);
            memcpy(buf + 12, "PRHD", 4); put32(buf + 16, 6);
            memcpy(buf + 26, "ICTL", 4); put32(buf + 30, ICTL_SIZE);
            n = 34 + ICTL_SIZE;
            o = 34;
        }
        if (be32(buf + o - 4) < 38) { printf("ICTL too short\n"); return 10; }
        put32(buf + o + 20, be32(buf + o + 20) | 0x80000000UL); /* ic_Flags at 16+2+2 = 20 */
        buf[o + 28] = 0; buf[o + 29] = 2; buf[o + 30] = 0; buf[o + 31] = 0; /* ic_Version 2, magic 0 */
        buf[o + 34] = (unsigned char)atoi(argv[1]); buf[o + 35] = (unsigned char)atoi(argv[2]);
        buf[o + 36] = (unsigned char)atoi(argv[3]); buf[o + 37] = (unsigned char)atoi(argv[4]);
        { BPTR f = Open((STRPTR)PATH, MODE_NEWFILE); if (!f) { printf("cannot write\n"); return 10; } Write(f, buf, n); Close(f); }
        Delay(200);
        n = load();
    }
    printf("FILE %ld\n", n);
    if ((o = ictl(n)) && be32(buf + o - 4) >= 38)
        printf("ICTL flags=%08lx version=%ld hover=%02lx geom=%ld %ld %ld %ld\n", be32(buf + o + 20), (long)((buf[o + 28] << 8) | buf[o + 29]),
               (long)buf[o + 32], (long)buf[o + 34], (long)buf[o + 35], (long)buf[o + 36], (long)buf[o + 37]);
    else if (o)
        printf("ICTL size %lu (no versioned tail)\n", be32(buf + o - 4));
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase) return 20;
    printf("INTUITION %ld.%ld\n", (long)IntuitionBase->LibNode.lib_Version, (long)IntuitionBase->LibNode.lib_Revision);
    s = LockPubScreen((STRPTR)"Workbench");
    if (!s) return 10;
    printf("SCREEN %08lx bar=%ld wbortop=%ld wborleft=%ld wborright=%ld wborbottom=%ld\n", (unsigned long)s, (long)s->BarHeight,
           (long)s->WBorTop, (long)s->WBorLeft, (long)s->WBorRight, (long)s->WBorBottom);
    w = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 60, WA_Width, 200, WA_Height, 100, WA_Title, (ULONG)"R1 probe",
                       WA_PubScreen, (ULONG)s, WA_CloseGadget, TRUE, WA_DepthGadget, TRUE, WA_SizeGadget, TRUE,
                       WA_DragBar, TRUE, WA_Activate, FALSE, TAG_END);
    if (w) {
        printf("WIN left=%ld top=%ld right=%ld bottom=%ld w=%ld h=%ld\n", (long)w->BorderLeft, (long)w->BorderTop, (long)w->BorderRight,
               (long)w->BorderBottom, (long)w->Width, (long)w->Height);
        { struct Gadget *g;
          for (g = w->FirstGadget; g; g = g->NextGadget)
              printf("GAD type=%04lx l=%ld t=%ld w=%ld h=%ld flags=%04lx\n", (long)g->GadgetType, (long)g->LeftEdge, (long)g->TopEdge,
                     (long)g->Width, (long)g->Height, (long)g->Flags); }
        CloseWindow(w);
    } else printf("WIN none\n");
    UnlockPubScreen(NULL, s);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
