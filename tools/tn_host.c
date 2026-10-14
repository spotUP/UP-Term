/* tn_host HOST PORT COLS ROWS [TERM] [KEY=file]: uptelnet's protocol (net/tn.c) on
 * a POSIX socket, stdin and stdout for the console. The interop test drives
 * it against tools/uptelnetd.py (tools/test_uptelnetd.py), so the C client
 * and the Mac end are proved to agree before either meets the Amiga.
 * KEY=file is the key login (net/tn.h): the file holds "ID hexkey" and the
 * line goes through the challenge-response; an empty or missing file asks to
 * be paired, and the key the server hands over is written to it. What the
 * login came to goes to stderr: "tn_host: auth <state>" when the line ends.
 * Ends when the server closes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/socket.h>
#include "../net/tn.h"

static int sock = -1;
static const char *keyfile;

/* the server handed a key over (pairing): the key file now holds it */
static void event(void *u, int state, const char *id, const unsigned char *key)
{
    FILE *f;
    char line[TN_ID_MAX + TN_KEY_LEN * 2 + 3];
    (void)u;
    if (state != TN_AUTH_PAIRED)
        return;
    f = keyfile ? fopen(keyfile, "w") : 0;
    if (!f)
        exit(1);
    tn_key_format(id, key, line);
    fputs(line, f);
    fclose(f);
}

static int read_key(const char *path, char *id, unsigned char *key)
{
    char line[200];
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return tn_key_parse(line, id, key);
}

static void report(const tn *t)
{
    static const char *const name[] = { "off", "asked", "sent", "ok", "rejected", "bad-server", "paired" };
    fprintf(stderr, "tn_host: auth %s\n", name[tn_auth(t)]);
}

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
    int in_open = 1, ia = 0;
    const char *term = "xterm-256color";
    if (argc < 5) {
        fprintf(stderr, "usage: tn_host HOST PORT COLS ROWS [TERM] [KEY=file]\n");
        return 2;
    }
    for (ia = 5; ia < argc; ia++) {
        if (!strncmp(argv[ia], "KEY=", 4))
            keyfile = argv[ia] + 4;
        else
            term = argv[ia];
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(argv[1], argv[2], &hints, &ai) || !ai)
        return 1;
    sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock < 0 || connect(sock, ai->ai_addr, ai->ai_addrlen) < 0)
        return 1;
    freeaddrinfo(ai);
    tn_init(&t, term, atoi(argv[3]), atoi(argv[4]), out, show, 0);
    if (keyfile) {
        char id[TN_ID_MAX + 1];
        unsigned char key[TN_KEY_LEN], cn[16];
        FILE *r = fopen("/dev/urandom", "r");
        int have = read_key(keyfile, id, key);
        if (!r || fread(cn, 1, sizeof(cn), r) != sizeof(cn))
            return 1;
        fclose(r);
        tn_set_key(&t, have ? id : 0, key, cn, event);
    }
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
            if (k <= 0) {
                if (keyfile)
                    report(&t);
                return 0;
            }
            tn_recv(&t, buf, (int)k);
            if (tn_auth(&t) == TN_AUTH_BAD_SERVER) {
                report(&t);
                return 3;
            }
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
