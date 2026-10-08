/* vsh's heap on the Amiga: malloc, calloc, realloc and free for every process vsh runs on it.
 *
 * vsh is one program image run by several processes at once: a pipeline's builtin stage, a ( ) and
 * a $( ) are subshell processes (shell/vsh.c subshell_proc) that allocate and free on the heap the
 * shell allocates on at the same moment (the parent starts the next stage, os_run). vc.lib's malloc
 * is one pool without a lock: a task switch inside it let two processes edit one puddle list, and
 * the subshell of `echo "$x" | head -1` died in a "Software Failure" while the shell waited for it
 * for ever (rig 2, agent/a051_cmdsub_file, about 1 run in 15; tools/rig/repeat_rig.py).
 *
 * These definitions take the place of vc.lib's (its malloc module is no longer linked, so the
 * library's own callers come here too): one exec pool behind a semaphore, each block led by its
 * size. Built into the Amiga vsh only; the host shell uses the C library's. */
#include <stdlib.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>

#define PUDDLE 16384 /* vc.lib's puddles were 16 KB too (V44 measured one kept after unset) */
#define THRESH 4096  /* larger blocks get memory of their own */

static APTR pool;
static struct SignalSemaphore lock;
static int ready;

/* The pool and its lock, made on the first call: that call is the shell's own, at startup,
 * long before it starts a second process. */
static int mem_init(void)
{
    if (ready)
        return 1;
    Forbid();
    if (!ready) {
        InitSemaphore(&lock);
        pool = CreatePool(MEMF_ANY, PUDDLE, THRESH);
        ready = pool != 0;
    }
    Permit();
    return ready;
}

void *malloc(size_t n)
{
    ULONG *b;
    if (!mem_init())
        return 0;
    ObtainSemaphore(&lock);
    b = (ULONG *)AllocPooled(pool, n + sizeof(ULONG));
    ReleaseSemaphore(&lock);
    if (!b)
        return 0;
    b[0] = n + sizeof(ULONG);
    return b + 1;
}

void free(void *p)
{
    ULONG *b;
    if (!p)
        return;
    b = (ULONG *)p - 1;
    ObtainSemaphore(&lock);
    FreePooled(pool, b, b[0]);
    ReleaseSemaphore(&lock);
}

void *calloc(size_t n, size_t size)
{
    size_t total = n * size;
    void *p;
    if (size && total / size != n)
        return 0;
    p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void *realloc(void *p, size_t n)
{
    void *q;
    size_t old;
    if (!p)
        return malloc(n);
    if (!n) {
        free(p);
        return 0;
    }
    old = ((ULONG *)p)[-1] - sizeof(ULONG);
    if (n <= old && n >= old / 2)
        return p; /* fits, and not much smaller: keep it */
    q = malloc(n);
    if (!q)
        return 0;
    memcpy(q, p, n < old ? n : old);
    free(p);
    return q;
}

/* At exit, as vc.lib's malloc did (a vbcc destructor: __EXIT_<priority>_<name>) */
void _EXIT_1_vsh_mem(void)
{
    if (ready) {
        DeletePool(pool);
        ready = 0;
    }
}
