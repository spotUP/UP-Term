/* ixsock -- do ixemul's AF_UNIX sockets work, the way GNU screen uses
 * them (P7: its backend listens on a named socket, attachers connect)?
 * A listener, a vfork'ed client that connects and writes, select() on
 * the listening socket and on the connection. */
#include <stdio.h>
#include <stdlib.h>
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
    if (argc > 2 && !strcmp(argv[1], "pingpong")) {
        /* both processes select for reading and writing on their end and
         * send 20000 bytes each way in 100-byte pieces, non-blocking, as
         * tmux's client and server do. Prints what each side got. */
        int fd = atoi(argv[2]), sent = 0, got = 0, spins = 0;
        char out[100];
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        memset(out, argc > 3 ? 'c' : 'p', sizeof out);
        while ((sent < 20000 || got < 20000) && spins < 2000) {
            fd_set r, w;
            struct timeval tv = { 5, 0 };
            FD_ZERO(&r);
            FD_ZERO(&w);
            FD_SET(fd, &r);
            if (sent < 20000)
                FD_SET(fd, &w);
            spins++;
            if (select(fd + 1, &r, &w, 0, &tv) <= 0)
                break;
            if (FD_ISSET(fd, &r)) {
                int n = read(fd, buf, sizeof buf);
                if (n > 0)
                    got += n;
            }
            if (FD_ISSET(fd, &w)) {
                int want = 20000 - sent < (int)sizeof out ? 20000 - sent : (int)sizeof out;
                int n = write(fd, out, want);
                if (n > 0)
                    sent += n;
            }
        }
        printf("%s %s: sent %d, got %d, %d selects\n", sent == 20000 && got == 20000 ? "ok" : "FAIL",
               argc > 3 ? "child" : "parent", sent, got, spins);
        return !(sent == 20000 && got == 20000);
    }
    if (argc > 1 && !strcmp(argv[1], "pairpingpong")) {
        int sv[2], pid;
        char num[16], *av[5];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
            return 1;
        sprintf(num, "%d", sv[1]);
        if ((pid = vfork()) == 0) {
            close(sv[0]);
            execl(argv[0], argv[0], "pingpong", num, "child", (char *)0);
            _exit(1);
        }
        close(sv[1]);
        sprintf(num, "%d", sv[0]);
        av[0] = argv[0]; av[1] = "pingpong"; av[2] = num; av[3] = 0;
        n = main(3, av);
        waitpid(pid, &st, 0);
        return n;
    }
    if (argc > 2 && !strcmp(argv[1], "pairwriter")) {
        /* the other process: select for writing, then write, 5 times */
        int fd = atoi(argv[2]), k;
        for (k = 0; k < 5; k++) {
            fd_set w;
            struct timeval tv = { 1, 0 };
            FD_ZERO(&w);
            FD_SET(fd, &w);
            select(fd + 1, 0, &w, 0, &tv);
            usleep(200000);
            write(fd, "x", 1);
        }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "pairwake")) {
        /* a reader and a writer in two processes, both in select on one
         * stream (tmux's client and server): each write must wake the
         * reader. One waiting slot per stream let the writer's select take
         * the reader's wake-up, and the reader slept for ever. */
        int sv[2], k, got = 0, pid;
        char num[16];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
            return 1;
        sprintf(num, "%d", sv[1]);
        if ((pid = vfork()) == 0) {
            close(sv[0]);
            execl(argv[0], argv[0], "pairwriter", num, (char *)0);
            _exit(1);
        }
        close(sv[1]);
        for (k = 0; k < 5; k++)
            if (readable(sv[0], 3000000) && read(sv[0], buf, sizeof buf) > 0)
                got++;
        waitpid(pid, &st, 0);
        printf("%s the reader woke for %d of 5 writes from another process\n", got == 5 ? "ok" : "FAIL", got);
        return got == 5 ? 0 : 1;
    }
    if (argc > 1 && !strcmp(argv[1], "pair")) {
        /* socketpair: libevent's signal pipe and tmux's client/server pair
         * (48.2 answered EPFNOSUPPORT for every domain) */
        int sv[2], bad = 0, k;
        n = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
        printf("%s socketpair %d errno %d\n", n == 0 ? "ok" : "FAIL", n, n < 0 ? errno : 0);
        if (n != 0)
            return 1;
        bad |= readable(sv[1], 0);
        write(sv[0], "ping", 4);
        bad |= !readable(sv[1], 200000);
        n = read(sv[1], buf, sizeof(buf));
        bad |= !(n == 4 && !memcmp(buf, "ping", 4));
        printf("%s 0 -> 1: select, then %d bytes\n", bad ? "FAIL" : "ok", n);
        write(sv[1], "pong!", 5);
        n = read(sv[0], buf, sizeof(buf));
        printf("%s 1 -> 0: %d bytes\n", n == 5 && !memcmp(buf, "pong!", 5) ? "ok" : "FAIL", n);
        bad |= !(n == 5 && !memcmp(buf, "pong!", 5));
        {
            /* two messages with a descriptor each, sent before the other
             * end reads (tmux's client sends its stdin and stdout so), read
             * with room for one descriptor (tmux's imsg): each read gets its
             * own message and its own descriptor. Before: neither arrived. */
            int a = open("/T/ixsock.a", O_RDWR | O_CREAT | O_TRUNC, 0666);
            int b = open("/T/ixsock.b", O_RDWR | O_CREAT | O_TRUNC, 0666);
            int k, got[2] = { -1, -1 }, len[2] = { 0, 0 };
            write(a, "AAA", 3);
            write(b, "BBBB", 4);
            for (k = 0; k < 2; k++) {
                struct msghdr m;
                struct iovec iov;
                char cbuf[CMSG_SPACE(sizeof(int))];
                struct cmsghdr *cm;
                int fd = k ? b : a;
                memset(&m, 0, sizeof(m));
                iov.iov_base = k ? "second" : "first";
                iov.iov_len = k ? 6 : 5;
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
                sendmsg(sv[0], &m, 0);
            }
            for (k = 0; k < 2; k++) {
                struct msghdr m;
                struct iovec iov;
                union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int))]; } cb;
                struct cmsghdr *cm;
                memset(&m, 0, sizeof(m));
                iov.iov_base = buf;
                iov.iov_len = sizeof(buf);
                m.msg_iov = &iov;
                m.msg_iovlen = 1;
                m.msg_control = cb.b;
                m.msg_controllen = sizeof(cb.b);
                len[k] = recvmsg(sv[1], &m, 0);
                cm = m.msg_controllen ? CMSG_FIRSTHDR(&m) : NULL;
                if (cm && cm->cmsg_type == SCM_RIGHTS)
                    memcpy(&got[k], CMSG_DATA(cm), sizeof(int));
            }
            n = 0;
            if (got[0] >= 0 && got[1] >= 0) {
                char x[8];
                lseek(got[0], 0, SEEK_SET);
                lseek(got[1], 0, SEEK_SET);
                n = read(got[0], x, sizeof x) == 3 && read(got[1], x, sizeof x) == 4;
            }
            printf("%s two passed descriptors, one per read: lengths %d %d, fds %d %d\n",
                   len[0] == 5 && len[1] == 6 && n ? "ok" : "FAIL", len[0], len[1], got[0], got[1]);
            bad |= !(len[0] == 5 && len[1] == 6 && n);
            close(a);
            close(b);
            if (got[0] >= 0) close(got[0]);
            if (got[1] >= 0) close(got[1]);
        }
        {
            /* non-blocking (tmux and libevent set it): a read with nothing
             * there is EAGAIN, and writing into a full buffer stops short */
            int fl = fcntl(sv[1], F_GETFL, 0), w = 0, r;
            char big[1024];
            fcntl(sv[1], F_SETFL, fl | O_NONBLOCK);
            fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL, 0) | O_NONBLOCK);
            r = read(sv[1], buf, sizeof buf);
            k = errno;
            printf("%s non-blocking read of nothing: %d errno %d\n",
                   r == -1 && k == EAGAIN ? "ok" : "FAIL", r, k);
            bad |= !(r == -1 && k == EAGAIN);
            memset(big, 'x', sizeof big);
            for (k = 0; k < 200; k++) {
                r = write(sv[0], big, sizeof big);
                if (r <= 0)
                    break;
                w += r;
            }
            k = errno;
            printf("%s non-blocking write fills the buffer and stops: %d bytes, then %d errno %d\n",
                   r == -1 && k == EAGAIN && w > 0 ? "ok" : "FAIL", w, r, k);
            bad |= !(r == -1 && k == EAGAIN && w > 0);
            while (read(sv[1], big, sizeof big) > 0)
                ;
            fcntl(sv[1], F_SETFL, fl);
        }
        close(sv[0]);
        n = readable(sv[1], 200000) ? (int)read(sv[1], buf, sizeof(buf)) : -2;
        printf("%s after closing 0, 1 reads end of file (%d)\n", n == 0 ? "ok" : "FAIL", n);
        bad |= n != 0;
        close(sv[1]);
        printf("ixsock pair: %s\n", bad ? "FAIL" : "all ok");
        return bad;
    }
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
