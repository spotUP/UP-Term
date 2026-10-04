/* tn_host HOST PORT COLS ROWS [TERM]: uptelnet's protocol (net/tn.c) on a
 * POSIX socket, stdin and stdout for the console. The interop test drives
 * it against tools/uptelnetd.py (tools/test_uptelnetd.py), so the C client
 * and the Mac end are proved to agree before either meets the Amiga.
 * Ends when the server closes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include "../net/tn.h"

static int sock = -1;

static void out(void *u, const unsigned char *b, int n)
{
    (void)u;
    while (n > 0) {
        ssize_t k = send(sock, b, (size_t)n, 0);
        if (k <= 0)
            exit(1);
        b += k;
        n -= (int)k;
    }
}

static void show(void *u, const unsigned char *b, int n)
{
    (void)u;
    fwrite(b, 1, (size_t)n, stdout);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    struct addrinfo hints, *ai;
    struct pollfd p[2];
    unsigned char buf[4096];
    tn t;
    int in_open = 1;
    if (argc < 5) {
        fprintf(stderr, "usage: tn_host HOST PORT COLS ROWS [TERM]\n");
        return 2;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(argv[1], argv[2], &hints, &ai) || !ai)
        return 1;
    sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock < 0 || connect(sock, ai->ai_addr, ai->ai_addrlen) < 0)
        return 1;
    freeaddrinfo(ai);
    tn_init(&t, argc > 5 ? argv[5] : "xterm-256color", atoi(argv[3]), atoi(argv[4]), out, show, 0);
    tn_start(&t);
    for (;;) {
        int n = 0;
        p[n].fd = sock;
        p[n++].events = POLLIN;
        if (in_open) {
            p[n].fd = 0;
            p[n++].events = POLLIN;
        }
        if (poll(p, (nfds_t)n, -1) < 0)
            return 1;
        if (p[0].revents) {
            ssize_t k = recv(sock, buf, sizeof(buf), 0);
            if (k <= 0)
                return 0;
            tn_recv(&t, buf, (int)k);
        }
        if (in_open && p[1].revents) {
            ssize_t k = read(0, buf, sizeof(buf));
            if (k <= 0) {
                in_open = 0;
                continue;
            }
            /* a line from the test is what a key press sends: CR, no LF */
            {
                ssize_t i;
                for (i = 0; i < k; i++)
                    if (buf[i] == '\n')
                        buf[i] = '\r';
            }
            tn_send(&t, buf, (int)k);
        }
    }
}
