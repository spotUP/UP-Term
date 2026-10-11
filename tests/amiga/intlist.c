/* intlist: the names of the vertical-blank interrupt servers, one per line (proc_rig.py: no "top sampler" left
 * after top ends, by q, by Ctrl-C or by -n). */
#include <stdio.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <proto/exec.h>

extern struct ExecBase *SysBase;

int main(void)
{
    static char names[32][40];
    struct List *l;
    struct Node *n;
    int k = 0, i;
    Disable();
    l = (struct List *)SysBase->IntVects[INTB_VERTB].iv_Data;
    for (n = l ? l->lh_Head : 0; n && n->ln_Succ && k < 32; n = n->ln_Succ, k++) {
        const char *s = n->ln_Name ? n->ln_Name : "(no name)";
        for (i = 0; i < 39 && s[i]; i++)
            names[k][i] = s[i];
        names[k][i] = 0;
    }
    Enable();
    for (i = 0; i < k; i++)
        printf("%s\n", names[i]);
    return 0;
}
