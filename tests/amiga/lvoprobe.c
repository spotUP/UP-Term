/* lvoprobe -- test only (WBRESET-EXT step 1): SetFunction a counting pass-through stub on every intuition.library
 * vector (-30 downward, JMP entries only, LockIBase/UnlockIBase skipped), then on Ctrl-C restore them and print every
 * vector called at least once, plus whether the Amarchy.WBReset record exists. Shows which LVO IPrefs really calls. */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>

extern struct ExecBase *SysBase;
#define MAXV 256

int main(void)
{
    struct Library *ib;
    unsigned char *stubs;
    static ULONG cnt[MAXV];
    static APTR old[MAXV];
    static int done[MAXV], skipped[MAXV];
    int i, n, jmp;
    ib = OpenLibrary((STRPTR)"intuition.library", 0);
    if (!ib)
        return 20;
    n = ib->lib_NegSize / 6;
    if (n > MAXV)
        n = MAXV;
    stubs = AllocMem(16 * n, MEMF_PUBLIC | MEMF_CLEAR);
    if (!stubs)
        return 20;
    Forbid();
    jmp = FindSemaphore((STRPTR)"Amarchy.WBReset") != 0;
    for (i = 5; i < n; i++) {
        unsigned char *e = (unsigned char *)ib - 6 * (i + 1), *s = stubs + 16 * i;
        ULONG c = (ULONG)&cnt[i], t;
        if (e[0] != 0x4E || e[1] != 0xF9 || i == 68 || i == 69) {  /* -414 LockIBase, -420 UnlockIBase */
            skipped[i] = 1;
            continue;
        }
        t = *(ULONG *)(e + 2);
        s[0] = 0x52; s[1] = 0xB9;                    /* addq.l #1,abs.l */
        memcpy(s + 2, &c, 4);
        s[6] = 0x4E; s[7] = 0xF9;                    /* jmp abs.l */
        memcpy(s + 8, &t, 4);
        CacheClearU();
        old[i] = SetFunction(ib, -6 * (i + 1), (APTR)s);
        done[i] = 1;
    }
    Permit();
    printf("lvoprobe: %d vectors patched, semaphore Amarchy.WBReset %s; Ctrl-C reports\n", n, jmp ? "PRESENT" : "ABSENT");
    fflush(stdout);
    for (i = 5; i < n; i++)
        if (skipped[i])
            printf("not patched: LVO -%d\n", 6 * (i + 1));
    fflush(stdout);
    Wait(SIGBREAKF_CTRL_C);
    for (i = 5; i < n; i++)
        if (done[i])
            SetFunction(ib, -6 * (i + 1), old[i]);
    for (i = 5; i < n; i++)
        if (cnt[i])
            printf("LVO -%d called %lu\n", 6 * (i + 1), (unsigned long)cnt[i]);
    printf("lvoprobe: done\n");
    return 0;
}
