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
    wbr_gate_build(p->gate, (uint32_t)hook, flat_jmp_target(p->entry));
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

void ipatch_free(ipatch *p, int rc)
{
    if (p->gate && rc != FLAT_STILL_CHAINED)
        FreeMem(p->gate, WBR_GATE_SIZE);
    p->gate = 0;
}
