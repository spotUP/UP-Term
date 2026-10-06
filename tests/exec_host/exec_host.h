/* The part of AmigaOS exec that handler/brk.c uses, for the host tests
 * (tests/test_brk.c). Lists keep exec's layout: a walk ends at the node
 * whose ln_Succ is 0, the list's own lh_Tail. Forbid, Permit, Disable,
 * Enable and Signal are the test's: Disable shuts out interrupts (a POSIX
 * signal there), Forbid only other tasks. */
#ifndef EXEC_HOST_H
#define EXEC_HOST_H

typedef unsigned long ULONG;
typedef unsigned char UBYTE;
typedef signed char BYTE;
typedef char *STRPTR;
typedef void *APTR;

struct Node {
    struct Node *ln_Succ;
    struct Node *ln_Pred;
    UBYTE ln_Type;
    BYTE ln_Pri;
    char *ln_Name;
};

struct List {
    struct Node *lh_Head;
    struct Node *lh_Tail;
    struct Node *lh_TailPred;
    UBYTE lh_Type;
    UBYTE l_pad;
};

struct Task {
    struct Node tc_Node;
};

struct MsgPort {
    struct Node mp_Node;
    UBYTE mp_Flags;
    UBYTE mp_SigBit;
    void *mp_SigTask;
    struct List mp_MsgList;
};

struct ExecBase {
    struct Task *ThisTask;
    struct List TaskReady;
    struct List TaskWait;
    struct Node pad; /* a tail node read through &TaskWait.lh_Tail stays inside */
};

extern struct ExecBase *SysBase;

void Forbid(void);
void Permit(void);
void Disable(void);
void Enable(void);
ULONG Signal(struct Task *t, ULONG sigs);

#endif
