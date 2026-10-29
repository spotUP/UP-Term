/* idleprobe: P2 R2.2. A process at priority -128 counts loops. Reports: counts per second idle (3 samples),
 * counts while the main task is busy, counts at a 50 percent duty cycle, and the time of a fixed busy
 * benchmark without and with the idle task (3 runs each). Usage: idleprobe [bench_iterations]. Not a user command. */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>

long __stack = 16384;
const char idleprobe_stack_cookie[] = "$STACK: 16384";

static volatile unsigned long g_count;
static volatile int g_quit, g_alive;

static void idle_entry(void)
{
    g_alive = 1;
    while (!g_quit)
        g_count++;
    g_alive = 0;
}

static unsigned long ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (unsigned long)d.ds_Minute * 3000UL + (unsigned long)d.ds_Tick;
}

static void idle_start(void)
{
    g_quit = 0;
    CreateNewProcTags(NP_Entry, (ULONG)idle_entry, NP_Priority, -128, NP_StackSize, 4096, NP_Name, (ULONG)"idleprobe-idle", TAG_DONE);
    while (!g_alive)
        Delay(1);
}

static void idle_stop(void)
{
    g_quit = 1;
    while (g_alive)
        Delay(1);
}

static unsigned long bench(unsigned long n)
{
    volatile unsigned long acc = 1;
    unsigned long i, t0 = ticks();
    for (i = 0; i < n; i++)
        acc = acc * 33UL + i;
    return ticks() - t0;
}

static void busy_ticks(unsigned long tk)
{
    volatile unsigned long acc = 1;
    unsigned long t0 = ticks(), i;
    while (ticks() - t0 < tk)
        for (i = 0; i < 1000; i++)
            acc = acc * 33UL + i;
}

int main(int argc, char **argv)
{
    unsigned long n = argc > 1 ? strtoul(argv[1], 0, 10) : 10000000UL;
    unsigned long c0, c1, tb[3], ti[3], sb = 0, si = 0;
    int k;
    idle_start();
    Delay(25);
    for (k = 0; k < 3; k++) {
        c0 = g_count;
        Delay(50);
        c1 = g_count;
        printf("idle 1 s sample %d: %lu counts\n", k, c1 - c0);
    }
    c0 = g_count;
    busy_ticks(50);
    c1 = g_count;
    printf("main busy 1 s: %lu counts\n", c1 - c0);
    c0 = g_count;
    for (k = 0; k < 10; k++) {
        busy_ticks(5);
        Delay(5);
    }
    c1 = g_count;
    printf("50%% duty over 2 s (100 ms busy, 100 ms Delay x10): %lu counts\n", c1 - c0);
    idle_stop();
    for (k = 0; k < 3; k++) {
        tb[k] = bench(n);
        sb += tb[k];
    }
    idle_start();
    for (k = 0; k < 3; k++) {
        ti[k] = bench(n);
        si += ti[k];
    }
    idle_stop();
    printf("bench n=%lu without idle: %lu %lu %lu ticks (sum %lu)\n", n, tb[0], tb[1], tb[2], sb);
    printf("bench n=%lu with idle:    %lu %lu %lu ticks (sum %lu)\n", n, ti[0], ti[1], ti[2], si);
    if (sb)
        printf("overhead: %ld per mille\n", ((long)si - (long)sb) * 1000L / (long)sb);
    return 0;
}
