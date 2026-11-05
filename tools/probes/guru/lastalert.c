/* lastalert: prints the Guru that Exec keeps over a warm reset, then clears it so the next boot does not report it again.
 * Usage: lastalert        print "<code hi> <code lo> task <addr>" or nothing (exit 0), then clear the record
 *        lastalert -d     dump low memory ($0..$20, $100..$120) and ExecBase LastAlert[] (measurement, no clear)
 *        lastalert -a     call Alert(0x80000008) (the rig's forced dead-end Guru; the machine resets)
 * Not a user command. The record is read where Kickstart 3.2 keeps it (tools/probes/guru, thoughts research s58). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/alerts.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

long __stack = 16384;
const char lastalert_stack_cookie[] = "$STACK: 16384";

/* -c: measurement only. Installs a ColdCapture (runs at the next reset with the OLD ExecBase in A6, return address
 * in A5) that copies the old LastAlert[] into a block, prints the block address; -r <hex> reads it back after boot. */
static int install_capture(struct ExecBase *eb)
{
    static const UWORD tmpl[] = {
        0x207C, 0, 0,                       /* movea.l #blk,a0 */
        0x20EE, 0, 0x20EE, 0, 0x20EE, 0, 0x20EE, 0,   /* move.l off+n*4(a6),(a0)+  x4 */
        0x20FC, 0xA1E4, 0x7ED0,             /* move.l #$A1E47ED0,(a0)+ */
        0x4ED5                              /* jmp (a5) */
    };
    UWORD *code = AllocMem(64, MEMF_PUBLIC | MEMF_CLEAR);
    ULONG *blk = AllocMem(32, MEMF_PUBLIC | MEMF_CLEAR);
    UWORD off = (UWORD)((char *)&eb->LastAlert[0] - (char *)eb), *w, sum = 0;
    int i;
    memcpy(code, tmpl, sizeof tmpl);
    code[1] = (UWORD)((ULONG)blk >> 16); code[2] = (UWORD)(ULONG)blk;
    for (i = 0; i < 4; i++) code[4 + i * 2] = (UWORD)(off + i * 4);
    CacheClearU();
    Forbid();
    eb->ColdCapture = (APTR)code;
    for (w = (UWORD *)&eb->SoftVer; w < (UWORD *)&eb->ChkSum; w++) sum += *w;
    eb->ChkSum = (UWORD)~sum;
    Permit();
    printf("capture code %08lX block %08lX off %04X\n", (unsigned long)code, (unsigned long)blk, off);
    return 0;
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = SysBase;
    if (argc > 1 && !strcmp(argv[1], "-c"))
        return install_capture(eb);
    if (argc > 2 && !strcmp(argv[1], "-r")) {
        volatile ULONG *p = (volatile ULONG *)strtoul(argv[2], 0, 16);
        int i;
        for (i = 0; i < 5; i++)
            printf("blk[%d]: %08lX\n", i, (unsigned long)p[i]);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "-a")) {
        Alert(0x80000008UL);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "-d")) {
        volatile ULONG *p = (volatile ULONG *)0;
        int i;
        Disable();
        for (i = 0; i < 8; i++)
            printf("$%03X: %08lX\n", i * 4, (unsigned long)p[i]);
        for (i = 0; i < 8; i++)
            printf("$%03X: %08lX\n", 0x100 + i * 4, (unsigned long)p[0x40 + i]);
        Enable();
        for (i = 0; i < 4; i++)
            printf("LastAlert[%d]: %08lX\n", i, (unsigned long)eb->LastAlert[i]);
        printf("ThisTask %08lX\n", (unsigned long)eb->ThisTask);
        return 0;
    }
    printf("TBD\n");
    return 0;
}
