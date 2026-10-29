/* netprobe: P2 R2.1. Does bsdsocket.library open, what does it say, and what does QueryInterfaceTagList (LVO -468,
 * Roadshow) answer for a few interface names? The Roadshow tag numbers are not in the vendored headers, so the
 * probe scans TAG_USER+1..+TAG_SCAN for each name and prints every tag that returns nonzero or changes its slot.
 * Not a user command. OpenLibrary of an absent library returns at once (printed with the elapsed ticks). */
#include <exec/libraries.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

long __stack = 16384;
const char netprobe_stack_cookie[] = "$STACK: 16384";

struct Library *SocketBase;

#define TAG_SCAN 24

static unsigned long ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (unsigned long)d.ds_Minute * 3000UL + (unsigned long)d.ds_Tick;
}

int main(int argc, char **argv)
{
    int stage = argc > 1 ? atoi(argv[1]) : 9; /* 0 open only, 1 one call with no tags, 9 full scan */
    static const char *names[] = {"eth0", "ether0", "en0", "ppp0", "lo0", "uae0", "nonexist9", 0};
    unsigned long t0 = ticks();
    int i, t;
    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 0);
    printf("OpenLibrary: %s after %lu ticks\n", SocketBase ? "OK" : "FAILED", ticks() - t0);
    if (!SocketBase)
        return 5;
    printf("version %u.%u id \"%s\"\n", (unsigned)SocketBase->lib_Version, (unsigned)SocketBase->lib_Revision,
           SocketBase->lib_IdString ? (const char *)SocketBase->lib_IdString : "");
    fflush(stdout);
    if (stage == 0) {
        CloseLibrary(SocketBase);
        return 0;
    }
    if (stage == 1) {
        struct TagItem e = {TAG_DONE, 0};
        printf("call eth0 no tags...\n");
        fflush(stdout);
        printf("ret=%ld\n", (long)QueryInterfaceTagList((STRPTR)"eth0", &e));
        fflush(stdout);
        CloseLibrary(SocketBase);
        return 0;
    }
    for (i = 0; names[i]; i++) {
        for (t = argc > 2 ? atoi(argv[2]) : 1; t <= (argc > 3 ? atoi(argv[3]) : TAG_SCAN); t++) {
            unsigned long slot[4] = {0, 0, 0, 0};
            struct TagItem tags[2];
            LONG r;
            tags[0].ti_Tag = TAG_USER + t;
            tags[0].ti_Data = (ULONG)&slot[0];
            tags[1].ti_Tag = TAG_DONE;
            tags[1].ti_Data = 0;
            printf("try %s tag+%d\n", names[i], t);
            fflush(stdout);
            r = QueryInterfaceTagList((STRPTR)names[i], tags);
            fflush(stdout);
            if (r || slot[0] || slot[1]) {
                printf("%-9s tag+%-2d ret=%ld slot0=0x%08lx slot1=0x%08lx", names[i], t, (long)r, slot[0], slot[1]);
                if (slot[0] > 0x1000 && slot[0] < 0x10000000UL && ((char *)slot[0])[0] >= 'a' && ((char *)slot[0])[0] <= 'z')
                    printf(" str=\"%.16s\"", (char *)slot[0]);
                printf("\n");
            }
        }
        printf("%-9s scanned\n", names[i]);
        fflush(stdout);
    }
    CloseLibrary(SocketBase);
    return 0;
}
