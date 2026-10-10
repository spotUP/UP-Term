/* Where a command vsh starts gets its own handle on the shell's stream
 * (shell/sh_own.h): at the handler of that stream, not at the window the
 * shell was started from. */
#include "harness.h"
#include "../shell/sh_own.h"

static int window_tag, pty_tag, pipe_tag;

/* `ptyrun /bin/sh -c "sed p"`: vsh's input is a pty slave, its console the
 * window ptyrun was started in. sed must read the pty: its own handle is
 * opened at the slave's port, never at the window's */
static void program_under_vsh_on_a_pty_reads_the_pty_not_the_window(void)
{
    CHECK(sh_own_port(&pty_tag, 1, 1, &window_tag) == &pty_tag);
}

/* vsh at a prompt in a console window: the stream is the window itself */
static void program_under_vsh_in_a_window_reads_the_window(void)
{
    CHECK(sh_own_port(&window_tag, 1, 1, &window_tag) == &window_tag);
}

/* a file or a pipe is handed over as the shell's handle itself */
static void file_or_pipe_is_shared_not_reopened(void)
{
    CHECK(sh_own_port(&pipe_tag, 1, 0, &window_tag) == 0);
}

/* no stream at all (a background command's output with none): the console */
static void no_stream_falls_back_to_the_console(void)
{
    CHECK(sh_own_port(0, 0, 0, &window_tag) == &window_tag);
}

void suite_sh_own(void)
{
    program_under_vsh_on_a_pty_reads_the_pty_not_the_window();
    program_under_vsh_in_a_window_reads_the_window();
    file_or_pipe_is_shared_not_reopened();
    no_stream_falls_back_to_the_console();
}
