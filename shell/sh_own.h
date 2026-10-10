/* Where a command vsh starts gets a handle of its own on one of the shell's
 * streams, apart from AmigaOS so the host tests it (tests/test_sh_own.c).
 *
 * vsh gives a command its own handle on an interactive stream, not the
 * shell's: RunCommand puts the argument line in the input handle's buffer
 * and takes it back when the command returns, and a background command's
 * output must outlive nothing of the shell's. The handle is opened as "*"
 * sent to a port. That port is the handler serving the shell's stream: the
 * console window, or the slave of a pseudo-terminal (ptyrun, screen, a
 * Unix program's /bin/sh -c), which answers "*" with another handle on
 * itself (handler/pty_handler.c). pr_ConsoleTask is the window the shell
 * was started from, which is not its stream on a pseudo-terminal: `ptyrun
 * /bin/sh -c "sed p"` read the window, or nothing, instead of the pty.
 * The console is the answer only when the shell has no stream there. */
#ifndef SH_OWN_H
#define SH_OWN_H

/* The port to open the command's own handle at, or 0: hand it the shell's
 * handle itself (a file or a pipe: one offset, as Unix shares the
 * descriptor). stream_port: the handler of the shell's stream (fh_Type),
 * have_stream: the shell has one, interactive: IsInteractive of it,
 * console_port: pr_ConsoleTask. */
static void *sh_own_port(void *stream_port, int have_stream, int interactive, void *console_port)
{
    if (!have_stream)
        return console_port;
    return interactive ? stream_port : 0;
}

#endif
