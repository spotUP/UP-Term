#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <intuition/intuitionbase.h>
#include <string.h>
#include "ipatch.h"
#include "wbreset_core.h"

extern struct IntuitionBase *IntuitionBase;

int ipatch_alloc(ipatch *p, long lvo)
{
    memset(p, 0, sizeof *p);
    if (!IntuitionBase)
        return -1;
    p->entry = (unsigned char *)IntuitionBase + lvo;
    if (!flat_jmp_target(p->entry))
        return -2;
    p->gate = (unsigned char *)AllocMem(WBR_GATE_SIZE, MEMF_PUBLIC);
    if (!p->gate)
        return -4;
    flat_swap_init(&p->sw);
    return 0;
}

int ipatch_arm(ipatch *p, void *hook)
{
    int rc;
    wbr_gate_build(p->gate, (uint32_t)hook, flat_jmp_target(p->entry), (uint32_t)p->gate);
    CacheClearU();
    p->slot = (uintptr_t *)(p->entry + 2);
    rc = flat_swap_install(&p->sw, p->slot, (uintptr_t)p->gate);
    return rc;
}

void ipatch_passthru(ipatch *p)
{
    if (!p->gate)
        return;
    memset(p->gate + WBR_GATE_HOOK, 0, 4);
    CacheClearU();
}

int ipatch_unarm(ipatch *p)
{
    if (!p->sw.installed)
        return FLAT_NOT_INSTALLED;
    return flat_swap_remove(&p->sw, p->slot, 0);
}

uint32_t ipatch_saved(const ipatch *p)
{
    const unsigned char *g = p->gate;
    return ((uint32_t)g[WBR_GATE_SAVED] << 24) | ((uint32_t)g[WBR_GATE_SAVED + 1] << 16) |
           ((uint32_t)g[WBR_GATE_SAVED + 2] << 8) | g[WBR_GATE_SAVED + 3];
}

long ipatch_busy(const ipatch *p)
{
    const unsigned char *g;
    if (!p->gate)
        return 0;
    g = p->gate + WBR_GATE_BUSY;
    return (long)(((uint32_t)g[0] << 24) | ((uint32_t)g[1] << 16) | ((uint32_t)g[2] << 8) | g[3]);
}

void ipatch_leave(ipatch *p)
{
    Forbid();
    --*(volatile long *)(p->gate + WBR_GATE_BUSY);
    Permit();
}

/* The gate is never freed: a task can sit between the library's JMP and the gate's first instruction for any time,
 * and no counter sees it. A gate with hook word 0 is safe forever (it counts, uncounts, jumps to the original). */
void ipatch_forget(ipatch *p)
{
    p->gate = 0;
    p->entry = 0;
    p->slot = 0;
}
