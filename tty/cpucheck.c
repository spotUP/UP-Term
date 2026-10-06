/* The entry point of the kit's 68020 programs. Built with -cpu=68000 (the
 * Makefile's cpuchk rule) and linked with the program, whose own main is
 * renamed up_main (-Dmain=up_main): a 68000 or 68010 reaches this code
 * before any 68020 instruction of the program runs, prints one line and
 * exits, where it would otherwise stop on an illegal instruction.
 * UP_PROG is the program's name (a bare word, -DUP_PROG=vsh). */
#include <exec/execbase.h>
#include <proto/dos.h>

/* bmsg.c again, for the 68000 and under other names: the program's own copy
 * (68020 code, if it uses bmsg at all) must not run before the check */
#define bmsg_no_stack cpuchk_no_stack
#define bmsg_net_errno cpuchk_net_errno
#define bmsg_amissl_missing cpuchk_amissl_missing
#define bmsg_uptelnet_missing cpuchk_uptelnet_missing
#define bmsg_xcon_missing cpuchk_xcon_missing
#define bmsg_need_68020 cpuchk_need_68020
#define bmsg_cpu_ok cpuchk_cpu_ok
#define bmsg_kit_drawer cpuchk_kit_drawer
#include "bmsg.c"

#ifndef UP_PROG
#define UP_PROG program
#endif
#define STR_(x) #x
#define STR(x) STR_(x)

extern int up_main(int argc, char **argv);

int main(int argc, char **argv)
{
    struct ExecBase *eb = *(struct ExecBase **)4L;
    if (!cpuchk_cpu_ok(eb->AttnFlags)) {
        static const char msg1[] = STR(UP_PROG) ": ";
        const char *msg2 = cpuchk_need_68020();
        BPTR out = Output();
        if (out) {
            const char *p = msg2;
            long n = 0;
            while (p[n])
                n++;
            Write(out, (APTR)msg1, (LONG)sizeof(msg1) - 1);
            Write(out, (APTR)msg2, (LONG)n);
            Write(out, (APTR)"\n", 1);
        }
        return 20; /* RETURN_FAIL */
    }
    return up_main(argc, argv);
}
