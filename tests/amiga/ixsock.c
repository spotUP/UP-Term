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
#include <sys/uio.h>
#include <fcntl.h>

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
    if ((f = fopen("/VTC/ixsock.log", "a"))) {
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
    int s, c, n, st, rights = argc > 1 && !strcmp(argv[1], "rights");
    char buf[64];
    const char *path = "/T/ixsock.test";
    if (argc > 1 && !strcmp(argv[1], "client")) {
        printf("c1 client started\n");
        c = socket(AF_UNIX, SOCK_STREAM, 0);
        printf("c2 client socket %d\n", c);
        memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        strcpy(a.sun_path, path);
        if (connect(c, (struct sockaddr *)&a, sizeof(a)) < 0) {
            printf("client: connect errno %d\n", errno);
            return 1;
        }
        printf("c3 client connected\n");
        if (argc > 2 && !strcmp(argv[2], "rights")) {
            /* a file's descriptor to the server (SCM_RIGHTS), as screen's
             * attacher hands its terminal to the backend */
            struct msghdr m;
            struct iovec iov;
            char cbuf[CMSG_SPACE(sizeof(int))];
            struct cmsghdr *cm;
            int fd = open("/T/ixsock.rights", O_RDWR | O_CREAT | O_TRUNC, 0666);
            char back[64];
            write(fd, "first line\n", 11);
            lseek(fd, 0, SEEK_SET);
            memset(&m, 0, sizeof(m));
            iov.iov_base = "fd";
            iov.iov_len = 2;
            m.msg_iov = &iov;
            m.msg_iovlen = 1;
            m.msg_control = cbuf;
            m.msg_controllen = sizeof(cbuf);
            cm = CMSG_FIRSTHDR(&m);
            cm->cmsg_level = SOL_SOCKET;
            cm->cmsg_type = SCM_RIGHTS;
            cm->cmsg_len = CMSG_LEN(sizeof(int));
            memcpy(CMSG_DATA(cm), &fd, sizeof(int));
            m.msg_controllen = cm->cmsg_len;
            printf("c5 sendmsg %d errno %d\n", (int)sendmsg(c, &m, 0), errno);
            read(c, back, 1); /* the server says when it has written */
            lseek(fd, 0, SEEK_SET);
            n = read(fd, back, sizeof(back) - 1);
            back[n > 0 ? n : 0] = 0;
            printf("c6 the file now: [%s]\n", back);
            close(fd);
            close(c);
            return strstr(back, "written by the server") ? 0 : 1;
        }
        write(c, "hello over AF_UNIX", 18);
        printf("c4 client wrote\n");
        close(c);
        return 0;
    }
    unlink(path);
    unlink("/VTC/ixsock.log");
    printf("0 start\n");
    if (argc > 1 && !strcmp(argv[1], "nop"))
        return 0; /* does a program that links the socket calls even start? */
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
    if (argc > 1 && !strcmp(argv[1], "listen")) {
        close(s);
        unlink(path);
        printf("3b closed\n");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "server")) {
        printf("3c waiting for a client started apart\n");
    } else {
    printf("3c vfork\n");
    if (vfork() == 0) {
        execl(argv[0], argv[0], "client", rights ? "rights" : (char *)0, (char *)0);
        _exit(127);
    }
    }
    if (argc > 1 && (!strcmp(argv[1], "noselect") || !strcmp(argv[1], "server")))
        printf("4 (no select)\n");
    else
        printf("4 select on listener: %d\n", readable(s, 3000000));
    printf("4b accept...\n");
    c = accept(s, 0, 0);
    printf("5 accept %d errno %d\n", c, c < 0 ? errno : 0);
    printf("6 select on connection: %d\n", c >= 0 && readable(c, 3000000));
    if (rights) {
        struct msghdr m;
        struct iovec iov;
        char cbuf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr *cm;
        int fd = -1;
        memset(&m, 0, sizeof(m));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf) - 1;
        m.msg_iov = &iov;
        m.msg_iovlen = 1;
        m.msg_control = cbuf;
        m.msg_controllen = sizeof(cbuf);
        n = recvmsg(c, &m, 0);
        cm = CMSG_FIRSTHDR(&m);
        if (m.msg_controllen >= CMSG_LEN(sizeof(int)) && cm->cmsg_type == SCM_RIGHTS)
            memcpy(&fd, CMSG_DATA(cm), sizeof(int));
        printf("7 recvmsg %d bytes, fd %d\n", n, fd);
        if (fd >= 0) {
            n = read(fd, buf, sizeof(buf) - 1);
            buf[n > 0 ? n : 0] = 0;
            printf("7b read through the passed fd: [%s]\n", buf);
            write(fd, "written by the server\n", 22);
            close(fd);
        }
        write(c, "k", 1);
    } else {
        n = c >= 0 ? read(c, buf, sizeof(buf) - 1) : -1;
        buf[n > 0 ? n : 0] = 0;
        printf("7 read %d [%s]\n", n, buf);
    }
    if (!(argc > 1 && !strcmp(argv[1], "server"))) {
        wait(&st);
        printf("8 client exit %d\n", WEXITSTATUS(st));
    }
    close(c);
    close(s);
    unlink(path);
    return 0;
}
