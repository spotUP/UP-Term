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

static void a_kept_break_goes_out_once_the_child_exists(void)
{
    CHECK(sh_flush_pending(0x1000, 0) == 0); /* the child is not made yet: keep it */
    CHECK(sh_flush_pending(0x1000, 1) == 1);
    CHECK(sh_flush_pending(0, 1) == 0); /* nothing kept */
}

static void a_new_task_at_a_dead_runners_address_is_not_the_runner(void)
{
    CHECK(sh_runner_is(1, "vsh job 7", "vsh job 7") == 1);
    CHECK(sh_runner_is(1, "vsh job 8", "vsh job 7") == 0); /* another process, reusing the address */
    CHECK(sh_runner_is(1, "Background CLI", "vsh job 7") == 0);
    CHECK(sh_runner_is(1, 0, "vsh job 7") == 0);
    CHECK(sh_runner_is(0, "vsh job 7", "vsh job 7") == 0); /* a task that is no process */
}

static void the_shell_process_systemtags_made_carries_the_job_input_and_nothing_else_is_it(void)
{
    CHECK(sh_child_is(1, 1, 0x1000, 0x1000, 0, 0, "Background CLI") == 1);
    CHECK(sh_child_is(1, 1, 0x2000, 0x1000, 0, 0, "Background CLI") == 0); /* another job's process */
    CHECK(sh_child_is(1, 0, 0x1000, 0x1000, 0, 0, "x") == 0);              /* no CLI */
    CHECK(sh_child_is(0, 1, 0x1000, 0x1000, 0, 0, "x") == 0);              /* no process */
    CHECK(sh_child_is(1, 1, 0, 0, 0, 0, "x") == 0);                        /* no input at all */
}

static void a_shell_sharing_the_input_is_not_the_child(void)
{
    CHECK(sh_child_is(1, 1, 0x1000, 0x1000, 0, 1, "Shell") == 0); /* the shell's own process */
    CHECK(sh_child_is(1, 1, 0x1000, 0x1000, 1, 0, "vsh job 3") == 0); /* the runner */
}

static void a_vsh_named_runner_sharing_the_input_is_not_the_child(void)
{
    CHECK(sh_child_is(1, 1, 0x1000, 0x1000, 0, 0, "vsh job 9") == 0);
    CHECK(sh_child_is(1, 1, 0x1000, 0x1000, 0, 0, "vsh subshell 2") == 0);
    CHECK(sh_child_is(1, 1, 0x1000, 0x1000, 0, 0, "vshell") == 1); /* only the "vsh " prefix */
}

static void a_job_name_is_the_prefix_and_its_number(void)
{
    char b[32];
    sh_job_tname(b, "vsh job", 1);
    CHECK(strcmp(b, "vsh job 1") == 0);
    sh_job_tname(b, "vsh subshell", 4294967295UL);
    CHECK(strcmp(b, "vsh subshell 4294967295") == 0);
    sh_job_tname(b, "vsh job", 0);
    CHECK(strcmp(b, "vsh job 0") == 0);
}

void suite_sh_jobgone(void)
{
    a_job_name_is_the_prefix_and_its_number();
    the_shell_process_systemtags_made_carries_the_job_input_and_nothing_else_is_it();
    a_shell_sharing_the_input_is_not_the_child();
    a_vsh_named_runner_sharing_the_input_is_not_the_child();
    a_new_task_at_a_dead_runners_address_is_not_the_runner();
    a_removed_runner_is_gone();
    a_removed_shell_process_of_a_resident_command_or_script_is_gone();
    a_running_job_is_not_gone();
    a_shell_process_not_started_yet_is_not_gone();
    a_kept_break_goes_out_once_the_child_exists();
}
