/* A job whose process was removed from outside (wasabi kill --force is a RemTask) never sends its
 * reply, so the shell waiting for it must notice the process is gone instead (apart from AmigaOS so the
 * host tests it, tests/test_sh_jobgone.c).
 *
 * runner_alive: the job's runner task still exists. child_named: the job has a Shell process of its
 * own (SystemTags, found by name), child_alive: that process exists now, child_seen: it has been seen
 * to exist once (before that it has only not started yet). 1: the job will never reply. */
#ifndef SH_JOBGONE_H
#define SH_JOBGONE_H

static int sh_job_gone(int runner_alive, int child_named, int child_alive, int child_seen)
{
    if (!runner_alive)
        return 1;
    return child_named && child_seen && !child_alive;
}

#endif
