/* ps and top's portable part (proc/proc_core.h): the VBlank samples counted per task, idle and gone tasks, the
 * ring's wrap, per cents and the lines both programs print. */
#include <string.h>
#include "harness.h"
#include "../proc/proc_core.h"

static pc_ring ring;

static void put(unsigned long v, int times)
{
    while (times-- > 0) {
        ring.slot[ring.count & (PC_RING - 1)] = v;
        ring.count++;
    }
}

static void task(pc_task *t, unsigned long id, const char *name, const char *cmd, int cli, int pri, int state)
{
    memset(t, 0, sizeof(*t));
    t->id = id;
    strcpy(t->name, name);
    strcpy(t->cmd, cmd);
    t->cli = cli;
    t->pri = pri;
    t->state = state;
    t->process = cli > 0;
}

/* a busy loop takes 95 of 100 samples: top shows 95.0; idle samples count to idle, not to the last task */
static void busy_task_gets_its_share_and_idle_is_not_charged(void)
{
    pc_task t[2];
    long idle = 0, gone = 0, n;
    char line[128];
    memset(&ring, 0, sizeof(ring));
    task(&t[0], 0x1000, "Shell Process", "busyloop", 3, 0, PC_RUN);
    task(&t[1], 0x2000, "input.device", "", 0, 20, PC_WAIT);
    put(0x1000, 95);
    put(0x2000, 2);
    put(0x2000 | PC_IDLE, 2); /* exec idled in input.device's Wait */
    put(0x3000, 1);           /* a task that has ended since */
    n = pc_account(t, 2, &ring, 0, &idle, &gone);
    CHECK_INT(n, 100);
    CHECK_INT(t[0].samples, 95);
    CHECK_INT(t[1].samples, 2);
    CHECK_INT(idle, 2);
    CHECK_INT(gone, 1);
    pc_top_row(line, sizeof(line), &t[0], n, 0);
    CHECK_STR(line, "  95.0    3    0  running  busyloop");
    pc_top_summary(line, sizeof(line), 2, n - idle, n, 1258291UL, 125829120UL, 0);
    CHECK_STR(line, "2 tasks, CPU busy 98.0 %, idle 2.0 %, 100 samples; free chip 1.2 MB, fast 120.0 MB");
}

/* an idle system: all samples idle, busy 0.0 */
static void idle_system_is_near_zero(void)
{
    pc_task t[1];
    long idle = 0, gone = 0, n;
    char line[128];
    memset(&ring, 0, sizeof(ring));
    task(&t[0], 0x1000, "ramlib", "", 0, 0, PC_WAIT);
    put(0x1000 | PC_IDLE, 100);
    n = pc_account(t, 1, &ring, 0, &idle, &gone);
    CHECK_INT(idle, 100);
    pc_top_summary(line, sizeof(line), 1, n - idle, n, 0, 0, 0);
    CHECK(strstr(line, "busy 0.0 %, idle 100.0 %") != 0);
}

/* more samples than the ring holds: only the newest PC_RING are counted, from where the last interval ended */
static void ring_keeps_the_newest_samples(void)
{
    pc_task t[1];
    long idle = 0, gone = 0, n;
    unsigned long from;
    memset(&ring, 0, sizeof(ring));
    task(&t[0], 0x1000, "a", "", 0, 0, PC_RUN);
    put(0x1000 | PC_IDLE, 10);
    from = ring.count;
    put(0x1000 | PC_IDLE, PC_RING / 2);
    put(0x1000, PC_RING);
    n = pc_account(t, 1, &ring, from, &idle, &gone);
    CHECK_INT(n, PC_RING);
    CHECK_INT(t[0].samples, PC_RING);
    CHECK_INT(idle, 0);
}

static void per_cents_round_and_print_in_five_columns(void)
{
    char s[8];
    CHECK_INT(pc_tenths(1, 3), 333);
    CHECK_INT(pc_tenths(2, 3), 667);
    CHECK_INT(pc_tenths(5, 0), 0);
    pc_pct(s, 1000);
    CHECK_STR(s, "100.0");
    pc_pct(s, 5);
    CHECK_STR(s, "  0.5");
}

/* ps: CLI processes by number with their command, then the tasks by name; a line cut to the width */
static void ps_lists_cli_processes_first_with_their_command(void)
{
    pc_task t[3];
    char line[128];
    task(&t[0], 0x10, "input.device", "", 0, 20, PC_WAIT);
    task(&t[1], 0x20, "Shell Process", "VTC:vsh", 2, 0, PC_WAIT);
    task(&t[2], 0x30, "Background CLI", "BOOTX:amiagent", 1, 0, PC_READY);
    pc_sort_ps(t, 3);
    CHECK_STR(pc_shown(&t[0]), "BOOTX:amiagent");
    CHECK_STR(pc_shown(&t[1]), "VTC:vsh");
    CHECK_STR(pc_shown(&t[2]), "input.device");
    pc_ps_header(line, sizeof(line), 0);
    CHECK_STR(line, " CLI  PRI  STATE    TYPE     NAME");
    pc_ps_row(line, sizeof(line), &t[2], 0);
    CHECK_STR(line, "   -   20  waiting  task     input.device");
    pc_ps_row(line, sizeof(line), &t[0], 20);
    CHECK_STR(line, "   1    0  ready    ");
}

/* top: the most samples first */
static void top_sorts_by_cpu_then_name(void)
{
    pc_task t[3];
    task(&t[0], 1, "b", "", 0, 0, PC_WAIT);
    task(&t[1], 2, "a", "", 0, 0, PC_WAIT);
    task(&t[2], 3, "c", "", 0, 0, PC_RUN);
    t[2].samples = 9;
    pc_sort_cpu(t, 3);
    CHECK_STR(t[0].name, "c");
    CHECK_STR(t[1].name, "a");
    CHECK_STR(t[2].name, "b");
}

void suite_proc(void)
{
    busy_task_gets_its_share_and_idle_is_not_charged();
    idle_system_is_near_zero();
    ring_keeps_the_newest_samples();
    per_cents_round_and_print_in_five_columns();
    ps_lists_cli_processes_first_with_their_command();
    top_sorts_by_cpu_then_name();
}
