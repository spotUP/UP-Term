/* cmdpath: the command path of a CLI (cli_CommandDir), one place for the node layout and the copy, free,
 * add and remove loops (RULES.md single source of truth).
 *
 * cli_CommandDir is a BPTR chain of AllocVec'd nodes {next, lock}: a Shell started with NP_Path gets a copy
 * of it and frees it when it ends. Three users:
 *   shell/vsh.c wb_path()           the Shell vsh starts from Workbench gets a copy of the Workbench path
 *   amarchy/proclaunch.c            every Amarchy launch gets a copy of the Workbench path (+ UP-Term:bin)
 *   install/wbpath.c (WBPath)       edits the Workbench process's own path live (UHC:ExtBin on install)
 * Needs CMDPATH_DOS defined before the include, after proto/exec.h, proto/dos.h and dos/dosextens.h. */
#ifndef CMDPATH_H
#define CMDPATH_H

#if defined(__GNUC__) || defined(__clang__)
#define CMDPATH_FN static __attribute__((unused))
#else
#define CMDPATH_FN static
#endif

#ifdef CMDPATH_DOS

struct cmdpath_node {
    BPTR next, lock;
};

/* Where process P keeps the head of its command path (0: P is no CLI process). Read and edit it under
 * Forbid(): the Shell owning the chain may change it. */
CMDPATH_FN BPTR *cmdpath_field(struct Process *p)
{
    struct CommandLineInterface *cli =
        p && p->pr_Task.tc_Node.ln_Type == NT_PROCESS ? (struct CommandLineInterface *)BADDR(p->pr_CLI) : 0;
    return cli ? &cli->cli_CommandDir : 0;
}

/* The Workbench process (0: none). */
CMDPATH_FN struct Process *cmdpath_workbench(void)
{
    struct Process *wbp = (struct Process *)FindTask((STRPTR)"Workbench");
    return wbp && wbp->pr_Task.tc_Node.ln_Type == NT_PROCESS ? wbp : 0;
}

/* A copy (every lock DupLock'ed) of the chain from SRC. *TAILP is left pointing at the last node's next
 * field, where a caller appends. Stops at the first node it cannot allocate: a short path, never a broken
 * chain. */
CMDPATH_FN BPTR cmdpath_copy(BPTR src, BPTR **tailp)
{
    BPTR head = 0, *tail = &head;
    for (; src; src = ((struct cmdpath_node *)BADDR(src))->next) {
        struct cmdpath_node *n = (struct cmdpath_node *)AllocVec(sizeof(*n), MEMF_PUBLIC | MEMF_CLEAR);
        if (!n)
            break;
        n->lock = DupLock(((struct cmdpath_node *)BADDR(src))->lock);
        *tail = MKBADDR(n);
        tail = &n->next;
    }
    if (tailp)
        *tailp = tail;
    return head;
}

/* Append LOCK (the chain owns it from now on) at *TAILP; 0 when out of memory (LOCK stays the caller's). */
CMDPATH_FN int cmdpath_append(BPTR **tailp, BPTR lock)
{
    struct cmdpath_node *n = (struct cmdpath_node *)AllocVec(sizeof(*n), MEMF_PUBLIC | MEMF_CLEAR);
    if (!n)
        return 0;
    n->lock = lock;
    **tailp = MKBADDR(n);
    *tailp = &n->next;
    return 1;
}

/* Free a chain made by cmdpath_copy (and the locks in it). */
CMDPATH_FN void cmdpath_free(BPTR p)
{
    while (p) {
        struct cmdpath_node *n = (struct cmdpath_node *)BADDR(p);
        BPTR next = n->next;
        if (n->lock)
            UnLock(n->lock);
        FreeVec(n);
        p = next;
    }
}

/* Is a node of the chain from HEAD the same object as LOCK? */
CMDPATH_FN int cmdpath_has(BPTR head, BPTR lock)
{
    for (; head; head = ((struct cmdpath_node *)BADDR(head))->next)
        if (SameLock(((struct cmdpath_node *)BADDR(head))->lock, lock) == LOCK_SAME)
            return 1;
    return 0;
}

/* Unlink and free every node that is the same object as LOCK; the number removed. */
CMDPATH_FN int cmdpath_remove(BPTR *headp, BPTR lock)
{
    int n = 0;
    BPTR *link = headp;
    while (*link) {
        struct cmdpath_node *node = (struct cmdpath_node *)BADDR(*link);
        if (SameLock(node->lock, lock) == LOCK_SAME) {
            *link = node->next;
            if (node->lock)
                UnLock(node->lock);
            FreeVec(node);
            n++;
        } else
            link = &node->next;
    }
    return n;
}

/* The last next field of the chain at *HEADP (where an append goes). */
CMDPATH_FN BPTR *cmdpath_tail(BPTR *headp)
{
    BPTR *link = headp;
    while (*link)
        link = &((struct cmdpath_node *)BADDR(*link))->next;
    return link;
}

#endif /* CMDPATH_DOS */
#endif /* CMDPATH_H */
