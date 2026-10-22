#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <string.h>
#include "upactive.h"

typedef struct upact {
    struct SignalSemaphore ss;   /* ss_Link.ln_Name points at name */
    LONG count;
    char name[16];
    char cur[24], prev[24];      /* request port names (upreq.h) */
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

void upactive_focus(const char *self)
{
    upact *u = find();
    if (!u && upactive_publish())
        u = find();
    if (!u)
        return;
    ObtainSemaphore(&u->ss);
    if (strcmp(u->cur, self)) {
        strcpy(u->prev, u->cur);
        strncpy(u->cur, self, sizeof u->cur - 1);
    }
    ReleaseSemaphore(&u->ss);
}

void upactive_forget(const char *self)
{
    upact *u = find();
    if (!u)
        return;
    ObtainSemaphore(&u->ss);
    if (!strcmp(u->cur, self))
        u->cur[0] = 0;
    if (!strcmp(u->prev, self))
        u->prev[0] = 0;
    ReleaseSemaphore(&u->ss);
}

int upactive_prev(char *out, int n)
{
    upact *u = find();
    int ok;
    if (!u)
        return 0;
    ObtainSemaphoreShared(&u->ss);
    ok = u->prev[0] != 0;
    if (ok) {
        strncpy(out, u->prev, (size_t)n - 1);
        out[n - 1] = 0;
    }
    ReleaseSemaphore(&u->ss);
    return ok;
}
