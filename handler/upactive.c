#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include "upactive.h"

typedef struct upact {
    struct SignalSemaphore ss;   /* ss_Link.ln_Name points at name */
    LONG count;
    char name[16];
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
