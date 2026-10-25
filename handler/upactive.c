#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <string.h>
#include "upactive.h"
#include "keymap_act.h"

typedef struct upact {
    struct SignalSemaphore ss;   /* ss_Link.ln_Name points at name */
    LONG count;
    char name[16];
    unsigned long mru[UPMRU_N];        /* request port ids, most recently focused first (upmru_*);
                                  * 48 bytes, the size the two names before it took */
} upact;

static upact *find(void)
{
    struct SignalSemaphore *s;
    Forbid();
    s = FindSemaphore((STRPTR)UPACTIVE_NAME);
    Permit();
    return (upact *)s;
}

int upactive_publish(void)
{
    upact *u;
    int i;
    Forbid();
    u = (upact *)FindSemaphore((STRPTR)UPACTIVE_NAME);
    if (!u) {
        u = (upact *)AllocVec(sizeof(upact), MEMF_PUBLIC | MEMF_CLEAR);
        if (u) {
            for (i = 0; UPACTIVE_NAME[i]; i++)
                u->name[i] = UPACTIVE_NAME[i];
            u->ss.ss_Link.ln_Name = u->name;
            u->ss.ss_Link.ln_Pri = 0;
            AddSemaphore(&u->ss);   /* never freed: other tasks may hold it */
        }
    }
    Permit();
    return u != 0;
}

void upactive_note(int on)
{
    upact *u = find();
    if (!u && upactive_publish())
        u = find();
    if (!u)
        return;
    ObtainSemaphore(&u->ss);
    if (on)
        u->count++;
    else if (u->count > 0)
        u->count--;
    ReleaseSemaphore(&u->ss);
}

long upactive_count(void)
{
    upact *u = find();
    long n;
    if (!u)
        return 0;
    ObtainSemaphoreShared(&u->ss);
    n = u->count;
    ReleaseSemaphore(&u->ss);
    return n;
}

void upactive_focus(unsigned long self)
{
    upact *u = find();
    if (!u && upactive_publish())
        u = find();
    if (!u)
        return;
    ObtainSemaphore(&u->ss);
    upmru_focus(u->mru, self);
    ReleaseSemaphore(&u->ss);
}

void upactive_forget(unsigned long self)
{
    upact *u = find();
    if (!u)
        return;
    ObtainSemaphore(&u->ss);
    upmru_forget(u->mru, self);
    ReleaseSemaphore(&u->ss);
}

int upactive_target(int self_in_mru, char out[24])
{
    upact *u = find();
    unsigned long id;
    if (!u)
        return 0;
    ObtainSemaphoreShared(&u->ss);
    id = upmru_target(u->mru, self_in_mru);
    ReleaseSemaphore(&u->ss);
    if (id)
        upreq_portname(id, out);
    return id != 0;
}

int upactive_terminal_present(int self_in_mru)
{
    char name[24];
    struct MsgPort *tp;
    if (!upactive_target(self_in_mru, name))
        return 0;
    Forbid();
    tp = FindPort((STRPTR)name);
    Permit();
    return tp != 0;
}

/* Directory Opus runs when its first ARexx port exists (the Amarchy DOpus group needs it). */
int upactive_dopus_present(void)
{
    struct MsgPort *p;
    Forbid();
    p = FindPort((STRPTR)"DOPUS.1");
    Permit();
    return p != 0;
}
