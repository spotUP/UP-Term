/* A job whose process was removed from outside (wasabi kill --force is a RemTask) never sends its
 * reply, so the shell waiting for it must notice the process is gone instead (apart from AmigaOS so the
 * host tests it, tests/test_sh_jobgone.c).
 *
 * runner_alive: the job's runner task still exists. child_named: the job has a Shell process of its
 * own (SystemTags, found by NP_ExitData), child_alive: that process exists now, child_seen: it has been seen
 * to exist once (before that it has only not started yet). 1: the job will never reply. */
#ifndef SH_JOBGONE_H
#define SH_JOBGONE_H

static int sh_job_gone(int runner_alive, int child_named, int child_alive, int child_seen)
{
    if (!runner_alive)
        return 1;
    return child_named && child_seen && !child_alive;
}

/* Is the task at the runner's remembered address still the runner? A task that ended and a new one given
 * its address look the same to a search of the task lists. The runner is made with NP_ExitData = its job,
 * so the task there is the runner when it is a Process (ln_Type NT_PROCESS: only then is pr_ExitData
 * there to read) carrying that job. */
static int sh_runner_is(int is_process, unsigned long exit_data, unsigned long job)
{
    return is_process && exit_data == job;
}

/* Is a process the Shell SystemTags made for the job: SystemTags passes NP_ExitData on, so it carries the
 * job, and it is not the runner (the caller leaves the runner out). */
static int sh_child_is(int is_process, unsigned long exit_data, unsigned long job)
{
    return is_process && exit_data == job;
}

#endif
