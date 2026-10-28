/* crashme [odd|illegal|ram|cold|coldrom|alert|dump|peek <hexaddr> <hexlen>] -- die the way the Installer died (INSTALLER-END-CRASH, s18), on purpose.
 * The reset loop that follows the Installer's guru on rig 9 (CPU reset PC=f80dc8 every ~1500 log lines,
 * surviving warm resets) has never been separated from the Installer itself. This program reproduces the
 * three recorded crash shapes without the Installer, so the loop can be bisected to "any guru on this rig"
 * or "the Installer's RAM state":
 *   odd      jump to an odd address in RAM: Exception 3 (address error), as in hunt 1 (1050301a)
 *   illegal  execute 0x0008 / 0x1050 words in RAM: Illegal instruction, as in s15/s17 (100DA418, 104E8438)
 *   ram      jump into a zeroed RAM block: whatever the CPU makes of it
 * Prints a mark before dying so the serial/console shows how far it got. */
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

long __stack = 65536;
const char *crashme_cookie = "$STACK:65536";

typedef void (*fn)(void);

static ULONG hexval(const char *s)
{
    ULONG v = 0;
    for (; *s; s++) {
        char c = *s | 0x20;
        v = (v << 4) | (ULONG)(c >= 'a' ? c - 'a' + 10 : c - '0');
    }
    return v;
}

int main(int argc, char **argv)
{
    const char *how = argc > 1 ? argv[1] : "odd";
    UWORD *code = (UWORD *)AllocMem(64, MEMF_PUBLIC | MEMF_CLEAR);
    if (!code) {
        Printf("crashme: no memory\n");
        return 20;
    }
    Printf("crashme: %s, code block at %08lx\n", (LONG)how, (ULONG)code);
    Flush(Output());
    if (strcmp(how, "peek") == 0 && argc > 3) {
        /* no crash: crashme peek <hexaddr> <hexlen>: longwords of any memory, same format as dump */
        UBYTE *p = (UBYTE *)hexval(argv[2]);
        LONG n = (LONG)hexval(argv[3]), i;
        for (i = 0; i < n; i += 16)
            Printf("%08lx: %08lx %08lx %08lx %08lx\n", (ULONG)(p + i), *(ULONG *)(p + i), *(ULONG *)(p + i + 4),
                   *(ULONG *)(p + i + 8), *(ULONG *)(p + i + 12));
        FreeMem(code, 64);
        return 0;
    } else if (strcmp(how, "dump") == 0) {
        /* no crash: hex of the code hung on LVO -726 (the 060 library's ColdReboot) and of ROM ColdReboot,
         * for m68k-amigaos-objdump -D -b binary -m m68k on the host */
        UBYTE *p = (UBYTE *)*(ULONG *)((UBYTE *)SysBase - 726 + 2);
        LONG i;
        Printf("crashme: LVO -726 -> %08lx, AttnFlags %04lx\n", (ULONG)p, (ULONG)SysBase->AttnFlags);
        for (i = 0; i < 512; i += 16)
            Printf("%08lx: %08lx %08lx %08lx %08lx\n", (ULONG)(p + i), *(ULONG *)(p + i), *(ULONG *)(p + i + 4),
                   *(ULONG *)(p + i + 8), *(ULONG *)(p + i + 12));
        p = (UBYTE *)0x00F80D9EUL;
        for (i = 0; i < 64; i += 16)
            Printf("%08lx: %08lx %08lx %08lx %08lx\n", (ULONG)(p + i), *(ULONG *)(p + i), *(ULONG *)(p + i + 4),
                   *(ULONG *)(p + i + 8), *(ULONG *)(p + i + 12));
        FreeMem(code, 64);
        return 0;
    } else if (strcmp(how, "cold") == 0) {
        /* the normal path: whatever SetPatch / 68060.library hung on LVO -726 (ami.py reboot takes this) */
        Printf("crashme: ColdReboot LVO -726 -> %08lx\n", *(ULONG *)((UBYTE *)SysBase - 726 + 2));
        Flush(Output());
        ColdReboot();
    } else if (strcmp(how, "coldrom") == 0) {
        /* the guru path: exec's dead-end alert calls the ROM routine, not the patched LVO */
        Printf("crashme: LVO -726 -> %08lx, jumping to ROM ColdReboot 00f80d9e\n",
               *(ULONG *)((UBYTE *)SysBase - 726 + 2));
        Flush(Output());
        ((fn)0x00F80D9EUL)();
    } else if (strcmp(how, "alert") == 0) {
        Alert(0x80000003);  /* dead-end alert: the requester, then exec's own reboot */
    } else if (strcmp(how, "illegal") == 0) {
        code[0] = 0x1050;   /* the opcode seen at 104E8438 */
        code[1] = 0x0008;   /* the opcode seen at 100DA418 */
        ((fn)code)();
    } else if (strcmp(how, "ram") == 0) {
        ((fn)code)();       /* zeros: ORI.B #0,D0 ... runs off the block */
    } else {
        ((fn)((UBYTE *)code + 1))();   /* odd address: Exception 3 */
    }
    Printf("crashme: still alive (%s did not trap)\n", (LONG)how);
    FreeMem(code, 64);
    return 10;
}
