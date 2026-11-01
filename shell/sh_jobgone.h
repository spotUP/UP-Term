/* A job whose process was removed from outside (wasabi kill --force is a RemTask) never sends its
 * reply, so the shell waiting for it must notice the process is gone instead (apart from AmigaOS so the
 * host tests it, tests/test_sh_jobgone.c).
 *
 * runner_alive: the job's runner task still exists. child_named: the job has a Shell process of its
 * own (SystemTags, found by its input handle), child_alive: that process exists now, child_seen: it has been seen
 * to exist once (before that it has only not started yet). 1: the job will never reply. */
#ifndef SH_JOBGONE_H
#define SH_JOBGONE_H

#include <string.h>

static int sh_job_gone(int runner_alive, int child_named, int child_alive, int child_seen)
{
    if (!runner_alive)
        return 1;
    return child_named && child_seen && !child_alive;
}

/* Should a break job_signal kept (pend) go out now: some is pending and the SystemTags child exists
 * (child_found). The job's runner got the break at once; it is blocked in SystemTags, so only the child
 * can act on it (`script & kill %1` signals before SystemTags has made the child). */
static int sh_flush_pending(unsigned long pend, int child_found)
{
    return pend != 0 && child_found;
}

/* The wait of a kill that needs a helper process (ixkill): the verdict of one look. The shell must not
 * become uninterruptible or wait without limit on a helper that is itself starved or stuck (kill -s TERM
 * of a priority-1 busy loop kept the sender over 90 s, deaf to Ctrl-C: s33 VSH-KILL-TERM-NATIVE-HANG).
 * The helper's reply wins over a break and a break over the limit. */
#define SH_KW_WAIT 0    /* keep waiting */
#define SH_KW_DONE 1    /* the helper replied: its return code says whether the signal went out */
#define SH_KW_INTR 2    /* Ctrl-C to the sender: stop waiting, the signal counts as not sent */
#define SH_KW_TIMEOUT 3 /* the limit is over: not sent */
#define SH_KILL_LIMIT_MS 5000L

static int sh_killwait(int replied, int ctrl_c, long elapsed_ms, long limit_ms)
{
    if (replied)
        return SH_KW_DONE;
    if (ctrl_c)
        return SH_KW_INTR;
    return elapsed_ms >= limit_ms ? SH_KW_TIMEOUT : SH_KW_WAIT;
}

/* A process vsh makes must leave pr_ExitData at 0: AmiTCP netlib programs (ixnet's init_inet_daemon
 * among them) read it as the DaemonMessage of an inetd start, fail to obtain the socket and exit(161)
 * (VSH-EXT-RC161). So a job is told apart by what programs only display or what vsh owns: the runner by
 * its per-job unique task name, the SystemTags child by its input handle. */

/* "<prefix> <n>" into buf (32 bytes): the task name of job number n (no sprintf in the vsh build). */
static void sh_job_tname(char *buf, const char *prefix, unsigned long n)
{
    char d[12];
    int k = 0;
    size_t l = strlen(prefix);
    memcpy(buf, prefix, l);
    buf[l++] = ' ';
    do {
        d[k++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (k)
        buf[l++] = d[--k];
    buf[l] = 0;
}

/* Is the task at the runner's remembered address still the runner? A task that ended and a new one given
 * its address look the same to a search of the task lists. The runner's name ("vsh job <n>", <n> never
 * repeating in the shell's lifetime) is unique to the job, so the task there is the runner when it is a
 * Process (ln_Type NT_PROCESS) with that name. */
static int sh_runner_is(int is_process, const char *name, const char *tname)
{
    return is_process && name != 0 && tname != 0 && strcmp(name, tname) == 0;
}

/* Is a process the Shell SystemTags made for the job: a CLI process whose standard input is the job's
 * input handle, that is not the job's runner (is_runner), not the shell's own process (is_self: a
 * non-interactive foreground job shares the shell's input) and not any other vsh runner or subshell
 * (their names start with "vsh ": they inherited the same handle). */
static int sh_child_is(int is_process, int has_cli, unsigned long cli_in, unsigned long job_in,
                       int is_runner, int is_self, const char *name)
{
    return is_process && has_cli && job_in != 0 && cli_in == job_in && !is_runner && !is_self &&
           !(name != 0 && strncmp(name, "vsh ", 4) == 0);
}

#endif
