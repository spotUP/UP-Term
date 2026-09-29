/* ixsock -- do ixemul's AF_UNIX sockets work, the way GNU screen uses
 * them (P7: its backend listens on a named socket, attachers connect)?
 * A listener, a vfork'ed client that connects and writes, select() on
 * the listening socket and on the connection. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/wait.h>

/* every line also to RAM:ixsock.log, closed at once: a hang shows how far it got */
#define printf say
static void say(const char *fmt, ...)
{
    char line[200];
    FILE *f;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    fputs(line, stdout);
    fflush(stdout);
    if ((f = fopen("/RAM/ixsock.log", "a"))) {
        fputs(line, f);
        fclose(f);
    }
}

static int readable(int fd, int usec)
{
    fd_set r;
    struct timeval tv;
    FD_ZERO(&r);
    FD_SET(fd, &r);
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    return select(fd + 1, &r, 0, 0, &tv) > 0;
}

int main(int argc, char **argv)
{
    struct sockaddr_un a;
    int s, c, n, st;
    char buf[64];
    const char *path = "/T/ixsock.test";
    if (argc > 1 && !strcmp(argv[1], "client")) {
        c = socket(AF_UNIX, SOCK_STREAM, 0);
        memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        strcpy(a.sun_path, path);
        if (connect(c, (struct sockaddr *)&a, sizeof(a)) < 0) {
            printf("client: connect errno %d\n", errno);
            return 1;
        }
        write(c, "hello over AF_UNIX", 18);
        close(c);
        return 0;
    }
    unlink(path);
    unlink("/RAM/ixsock.log");
    printf("0 start\n");
    s = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("1 socket %d errno %d\n", s, s < 0 ? errno : 0);
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, path);
    n = bind(s, (struct sockaddr *)&a, sizeof(a));
    printf("2 bind %d errno %d\n", n, n < 0 ? errno : 0);
    n = listen(s, 5);
    printf("3 listen %d errno %d\n", n, n < 0 ? errno : 0);
    fflush(stdout);
    if (vfork() == 0) {
        execl(argv[0], argv[0], "client", (char *)0);
        _exit(127);
    }
    printf("4 select on listener: %d\n", readable(s, 3000000));
    c = accept(s, 0, 0);
    printf("5 accept %d errno %d\n", c, c < 0 ? errno : 0);
    printf("6 select on connection: %d\n", c >= 0 && readable(c, 3000000));
    n = c >= 0 ? read(c, buf, sizeof(buf) - 1) : -1;
    buf[n > 0 ? n : 0] = 0;
    printf("7 read %d [%s]\n", n, buf);
    wait(&st);
    printf("8 client exit %d\n", WEXITSTATUS(st));
    close(c);
    close(s);
    unlink(path);
    return 0;
}
