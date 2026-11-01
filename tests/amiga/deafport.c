/* deafport -- a public ARexx port that holds its first message for LATE seconds before replying (a peer that is busy or
 * stalled), then answers every later one at once. Used by tests/amiga/rexxbound.c (REXX-DEAD-AFTER-THEME). Runs until
 * Ctrl-C. Usage: deafport PORT LATE */
#include <dos/dos.h>
#include <rexx/storage.h>
#include <proto/exec.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
    struct MsgPort *p;
    long late = 12, i;
    int first = 1;
    if (argc < 2)
        return 10;
    if (argc > 2)
        for (i = 0; argv[2][i] >= '0' && argv[2][i] <= '9'; i++)
            late = i ? late * 10 + (argv[2][i] - '0') : argv[2][i] - '0';
    p = CreateMsgPort();
    if (!p)
        return 20;
    p->mp_Node.ln_Name = argv[1];
    p->mp_Node.ln_Pri = 0;
    AddPort(p);
    for (;;) {
        struct RexxMsg *m;
        ULONG sigs = Wait((1UL << p->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        while ((m = (struct RexxMsg *)GetMsg(p)) != 0) {
            if (first)
                Delay(late * 50);
            first = 0;
            m->rm_Result1 = 0;
            m->rm_Result2 = 0;
            ReplyMsg((struct Message *)m);
        }
    }
    RemPort(p);
    DeleteMsgPort(p);
    return 0;
}
