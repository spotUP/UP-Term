/* The shell notices a job removed from outside (shell/sh_jobgone.h): vsh waited for ever for the
 * reply of a job `wasabi kill --force` had removed (VSH-KILL-FORCE-HANG). */
#include "harness.h"
#include "../shell/sh_jobgone.h"

static void a_removed_runner_is_gone(void)
{
    CHECK(sh_job_gone(0, 0, 0, 0) == 1);
    CHECK(sh_job_gone(0, 1, 1, 1) == 1);
}

static void a_removed_shell_process_of_a_resident_command_or_script_is_gone(void)
{
    CHECK(sh_job_gone(1, 1, 0, 1) == 1);
}

static void a_running_job_is_not_gone(void)
{
    CHECK(sh_job_gone(1, 0, 0, 0) == 0);
    CHECK(sh_job_gone(1, 1, 1, 1) == 0);
}

static void a_shell_process_not_started_yet_is_not_gone(void)
{
    CHECK(sh_job_gone(1, 1, 0, 0) == 0);
}

void suite_sh_jobgone(void)
{
    a_removed_runner_is_gone();
    a_removed_shell_process_of_a_resident_command_or_script_is_gone();
    a_running_job_is_not_gone();
    a_shell_process_not_started_yet_is_not_gone();
}
