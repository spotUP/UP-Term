/* amarchy-flat: the resident flat-UI program (plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0, OD-F8).
 *
 *   amarchy-flat            start: make the public state block "Amarchy.Flat", print its STATUS, wait for Ctrl-C
 *   amarchy-flat STATUS     print the STATUS line of the running program and return (flat=on|off|absent|held)
 *
 * C0 is the skeleton: the state block and the semaphore only. No class hook is installed yet (FLAT-C1), so nothing
 * is ever chained and Ctrl-C always unloads. The block layout and the STATUS text are flat_core.c (host-tested).
 * Return code: 0 done, 5 another copy is running, 10 no memory. */
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../tty/qmark.h"
#include "flat_core.h"
#include "taskalive.h"

static const char ver[] = "$VER: amarchy-flat 0.1 (30.10.2026) (UP-Term)";

/* RULES.md rule 6: the stack the program asks for, and the cookie the Shell and Workbench read. */
long __stack = 16384;
const char amarchy_flat_stack_cookie[] = "$STACK: 16384";

/* The public record. ss is first so AddSemaphore's node is the allocation's start (as ixshared, ixwatch.h). */
typedef struct flatshared {
    struct SignalSemaphore ss;
    char name[16];
    struct Task *task;          /* the resident program; only compared, never dereferenced alone */
    char tname[32];
    flat_state st;
} flatshared;

static flatshared *find_record(void)
{
    flatshared *r;
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    Permit();
    return r;
}

/* Print the STATUS line of the record in memory (a copy taken under Forbid; the layout is checked before use). */
static void print_status(void)
{
    flat_state copy;
    char line[96];
    flatshared *r;
    int found = 0;
    memset(&copy, 0, sizeof copy);
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r) {
        found = 1;
        copy = r->st;
    }
    Permit();
    if (flat_status_line(&copy, sizeof copy, found, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
}

static int is_status(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return (a[0] == 'S' || a[0] == 's') && (a[1] == 'T' || a[1] == 't') && (a[2] == 'A' || a[2] == 'a') &&
           (a[3] == 'T' || a[3] == 't') && (a[4] == 'U' || a[4] == 'u') && (a[5] == 'S' || a[5] == 's') &&
           (a[6] == 0 || a[6] == ' ' || a[6] == '\t' || a[6] == '\n');
}

static int is_help(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return qmark_line(a) || (a[0] == '-' && ((a[1] == 'h' && (a[2] == 0 || a[2] == ' ' || a[2] == '\n')) || (a[1] == '-' && a[2] == 'h')));
}

/* Take the record out of the list and free it; 0 when someone holds it (never pull it from under a user). */
static int drop_record(flatshared *r)
{
    int ok = 0;
    Forbid();
    if (AttemptSemaphore(&r->ss)) {
        RemSemaphore(&r->ss);
        ReleaseSemaphore(&r->ss);
        ok = 1;
    }
    Permit();
    if (ok)
        FreeVec(r);
    return ok;
}

int main(void)
{
    const char *args = (const char *)GetArgStr();
    flatshared *r;
    (void)ver;
    if (is_help(args)) {
        PutStr((STRPTR)"amarchy-flat [STATUS]: the flat-UI resident program; prints flat=on|off|absent|held. Ctrl-C unloads it\n");
        return 0;
    }
    if (is_status(args)) {
        print_status();
        return 0;
    }
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r && task_alive(r->task, r->tname)) {
        Permit();
        PutStr((STRPTR)"amarchy-flat: already running\n");
        print_status();
        return 5;
    }
    if (r) {                                            /* a dead owner left its record: take it over, never leak it */
        r->task = 0;
        Permit();
        if (!drop_record(r)) {
            PutStr((STRPTR)"amarchy-flat: a stale record is held\n");
            return 5;
        }
        Forbid();
    }
    r = (flatshared *)AllocVec(sizeof(flatshared), MEMF_PUBLIC | MEMF_CLEAR);
    if (r) {
        struct Task *me = FindTask(0);
        int i;
        for (i = 0; FLAT_SEM_NAME[i] && i < (int)sizeof r->name - 1; i++)
            r->name[i] = FLAT_SEM_NAME[i];
        r->ss.ss_Link.ln_Name = r->name;
        flat_init(&r->st);
        r->task = me;
        if (me->tc_Node.ln_Name) {
            for (i = 0; me->tc_Node.ln_Name[i] && i < (int)sizeof r->tname - 1; i++)
                r->tname[i] = me->tc_Node.ln_Name[i];
        }
        AddSemaphore(&r->ss);
    }
    Permit();
    if (!r) {
        PutStr((STRPTR)"amarchy-flat: out of memory\n");
        return 10;
    }
    print_status();
    for (;;) {
        Wait(SIGBREAKF_CTRL_C);
        if (!flat_can_exit(&r->st)) {
            PutStr((STRPTR)"amarchy-flat: a hook is still chained on top of ours, not unloading\n");
            continue;
        }
        if (drop_record(r))
            return 0;
        PutStr((STRPTR)"amarchy-flat: the state block is in use, not unloading\n");
    }
}
