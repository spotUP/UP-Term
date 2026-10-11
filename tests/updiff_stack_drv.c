/* updiff_stack_drv -- how much stack ud_compare (install/updiff.c) uses, in a
 * build WITHOUT sanitizers (the Makefile builds this at plain -O2). The
 * Installer runs upupdate on a child stack that is probably the 4096-byte DOS
 * default (inferred; the rig showed the old binary crashing at 4096): a stale
 * manifest overran it with 7 KB of tables as locals and the Amiga died with
 * an illegal-instruction alert (80000004). The budget is that 4096 minus a
 * margin for upupdate's own frame, ReadArgs and dos.library calls.
 *
 * The depth is measured by painting the stack below this frame, running the
 * call, and finding the deepest byte it changed. Exit 0 when both cases fit. */
#include <stdio.h>
#include <string.h>
#include "install/updiff.h"

#define PROBE 24576
#define STACK_BUDGET 3072   /* 4096 bytes minus 1024 for upupdate main(), ReadArgs, dos.library */

static const char *const old_lines[] = {
    "tools 300 0000dddd Files/UPTerm\n",
    "copy-coreutils 100 0000aaaa Files/coreutils/bin/ls\n",
    "copy-coreutils 50 0000bbbb Files/coreutils/bin/sort\n",
    "copy-nvim 10 00000001 Files/nvim/share/a.vim\n",
    "system 2000 0000cccc Files/vsh\n",
    "- 7 00001234 README.txt\n",
    "copy-coreutils 0 00000000 copy#Files/coreutils/bin#bin\n",
    "copy-nvim 0 00000000 copy#Files/nvim#nvim\n",
    "all 0 11111111 install.dos#all\n",
    "copy-nvim 0 33333333 install.dos#copy-nvim\n",
    "system 0 22222222 install.dos#system\n",
    "tools 0 44444444 install.dos#tools\n",
    0
};

/* the same kit with every copy part moved to other drawers and files changed */
static const char *const new_lines[] = {
    "tools 300 0000d00d Files/UPTerm\n",
    "copy-coreutils 100 0000a00a Files/coreutils2/bin/ls\n",
    "copy-coreutils 50 0000bbbb Files/coreutils2/bin/sort\n",
    "copy-nvim 10 00000002 Files/nvim2/share/a.vim\n",
    "system 2000 0000c00c Files/vsh\n",
    "- 7 00001234 README.txt\n",
    "copy-coreutils 0 00000000 copy#Files/coreutils2/bin#bin\n",
    "copy-nvim 0 00000000 copy#Files/nvim2#nvim\n",
    "all 0 11111111 install.dos#all\n",
    "copy-nvim 0 33333334 install.dos#copy-nvim\n",
    "system 0 22222223 install.dos#system\n",
    "tools 0 44444445 install.dos#tools\n",
    0
};

static void build(char *dst, const char *const *l)
{
    *dst = 0;
    for (; *l; l++) strcat(dst, *l);
}

static void sink(void *ctx, const char *part, const char *line)
{
    (void)ctx;
    (void)part;
    (void)line;
}

/* mode 0: paint PROBE bytes below the caller; mode 1: how deep the call in between went */
static long stack_probe(int mode) __attribute__((noinline));
static long stack_probe(int mode)
{
    volatile unsigned char buf[PROBE];
    long i;
    if (mode == 0) {
        for (i = 0; i < PROBE; i++) buf[i] = 0xA5;
        return 0;
    }
    for (i = 0; i < PROBE && buf[i] == 0xA5; i++)
        ;
    return PROBE - i;
}

int main(void)
{
    static char a[2048], b[2048];
    static ud_result r;
    long same, changed;
    int rc = 0;
    build(a, old_lines);
    build(b, old_lines);
    stack_probe(0);
    if (ud_compare(a, b, "VTC:distkit", &r, 0, 0) != 0) return 2;
    same = stack_probe(1);
    build(a, old_lines);
    build(b, new_lines);
    stack_probe(0);
    if (ud_compare(a, b, "VTC:distkit", &r, sink, 0) != 0) return 2;
    changed = stack_probe(1);
    printf("updiff stack: unchanged %ld bytes, changed %ld bytes (budget %d)\n", same, changed, STACK_BUDGET);
    if (same <= 0 || changed <= 0) {
        printf("[ERROR] the probe did not see the call\n");
        rc = 1;
    }
    if (same >= STACK_BUDGET || changed >= STACK_BUDGET) {
        printf("[ERROR] ud_compare uses more stack than the Amiga budget\n");
        rc = 1;
    }
    return rc;
}
