/* handler/brk.c on the host, against a model of exec's task lists
 * (tests/exec_host): an interrupt is a POSIX timer signal, Disable() blocks
 * it, Forbid() does not -- exec's own rule. */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include "harness.h"
#include "../handler/brk.h"
#include <exec/execbase.h>

static struct ExecBase eb;
struct ExecBase *SysBase = &eb;

static sigset_t irq_set;
static int disabled;

void Forbid(void) {}
void Permit(void) {}
void Disable(void)
{
    if (disabled++ == 0)
        sigprocmask(SIG_BLOCK, &irq_set, 0);
}
void Enable(void)
{
    if (--disabled == 0)
        sigprocmask(SIG_UNBLOCK, &irq_set, 0);
}
ULONG Signal(struct Task *t, ULONG sigs)
{
    (void)t;
    return sigs;
}

static void new_list(struct List *l)
{
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = 0;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
}

static void add_tail(struct List *l, struct Node *n)
{
    struct Node *tail = (struct Node *)&l->lh_Tail;
    n->ln_Succ = tail;
    n->ln_Pred = l->lh_TailPred;
    l->lh_TailPred->ln_Succ = n;
    l->lh_TailPred = n;
}

static void rem(struct Node *n)
{
    n->ln_Pred->ln_Succ = n->ln_Succ;
    n->ln_Succ->ln_Pred = n->ln_Pred;
}

#define WAITERS 1000
static struct Task me, owner, waiters[WAITERS];
static volatile sig_atomic_t owner_waits, wakes, wakes_in_check, in_check;

/* The interrupt: a device answers, Signal() moves the waiting owner to
 * TaskReady (exec enqueues it behind its equals: the tail). */
static void irq(int sig)
{
    (void)sig;
    if (owner_waits) {
        rem(&owner.tc_Node);
        add_tail(&SysBase->TaskReady, &owner.tc_Node);
        owner_waits = 0;
        wakes++;
        if (in_check)
            wakes_in_check++;
    }
}

/* The rig's intermittent C:Claude failure: Return made a new line in the
 * input box and a typed '/' was lost, because the console asked
 * task_alive whether its termios owner (C:Claude, waiting for input or the
 * network) lived while an interrupt's Signal() moved it from TaskWait to
 * TaskReady. Walked under Forbid() only, the owner was in neither list when
 * each was read, so the console took it for dead and left raw mode. The
 * owner is alive at every check. */
static void console_keeps_raw_mode_when_an_interrupt_wakes_the_owner(void)
{
    struct sigaction sa, old_sa;
    struct itimerval tv, old_tv;
    long checks = 0, dead = 0;
    time_t until;
    int i;

    new_list(&eb.TaskReady);
    new_list(&eb.TaskWait);
    eb.ThisTask = &me;
    for (i = 0; i < WAITERS; i++)
        add_tail(&eb.TaskWait, &waiters[i].tc_Node);
    add_tail(&eb.TaskWait, &owner.tc_Node);
    owner_waits = 1;
    wakes = wakes_in_check = in_check = 0;

    sigemptyset(&irq_set);
    sigaddset(&irq_set, SIGALRM);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = irq;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, &old_sa);
    tv.it_interval.tv_sec = tv.it_value.tv_sec = 0;
    tv.it_interval.tv_usec = tv.it_value.tv_usec = 50;
    setitimer(ITIMER_REAL, &tv, &old_tv);

    until = time(0) + 3;
    while (wakes_in_check < 200 && time(0) < until) {
        int alive;
        /* the owner runs, then waits again: a task switch, interrupts off */
        Disable();
        if (!owner_waits) {
            rem(&owner.tc_Node);
            add_tail(&eb.TaskWait, &owner.tc_Node);
            owner_waits = 1;
        }
        Enable();
        /* the console's check (handler/vtcon_handler.c tty_active) */
        Forbid();
        in_check = 1;
        alive = task_alive(&owner);
        in_check = 0;
        Permit();
        checks++;
        if (!alive)
            dead++;
    }

    memset(&tv, 0, sizeof(tv));
    setitimer(ITIMER_REAL, &tv, 0);
    sigaction(SIGALRM, &old_sa, 0);
    setitimer(ITIMER_REAL, &old_tv, 0);

    /* the race was run: interrupts arrived while the console checked */
    CHECK(wakes_in_check >= 200);
    CHECK_INT(dead, 0);
    if (dead)
        printf("  owner taken for dead %ld times in %ld checks (%ld wakes)\n",
               dead, checks, (long)wakes);
}

/* A task in neither list (it ended) is not alive; ThisTask is. */
static void ended_task_is_not_alive(void)
{
    struct Task gone;
    new_list(&eb.TaskReady);
    new_list(&eb.TaskWait);
    eb.ThisTask = &me;
    add_tail(&eb.TaskWait, &waiters[0].tc_Node);
    CHECK(!task_alive(&gone));
    CHECK(!task_alive(0));
    CHECK(task_alive(&me));
    CHECK(task_alive(&waiters[0]));
}

void suite_brk(void)
{
    console_keeps_raw_mode_when_an_interrupt_wakes_the_owner();
    ended_task_is_not_alive();
}
