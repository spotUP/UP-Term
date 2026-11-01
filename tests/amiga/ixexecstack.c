/* ixexecstack PROGRAM -- vfork + execve of a NATIVE (non-ixemul) program
 * from an ixemul one, as ptyrun starts vsh. Regression test for ixemul's
 * compatible_startup(): the native program must get at least the stack its
 * file's "$STACK: n" cookie asks for, not just the Shell's default stack
 * (tools/rig/stackext_rig.py runs it with stackprobe256k). */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

extern char **environ;

int main(int argc, char **argv)
{
    int st = 0;
    pid_t pid;

    if (argc < 2)
        return 2;
    pid = vfork();
    if (pid == 0) {
        char *av[2];
        av[0] = argv[1];
        av[1] = 0;
        execve(argv[1], av, environ);
        _exit(127);
    }
    if (pid < 0 || waitpid(pid, &st, 0) < 0)
        return 3;
    printf("ixexecstack: child status %d\n", WEXITSTATUS(st));
    return 0;
}
