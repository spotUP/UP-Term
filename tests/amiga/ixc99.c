/* ixc99 -- the C99/POSIX pieces UP-Term added to ixemul 48.2 for libevent
 * and tmux: snprintf/vsnprintf with size 0 (how asprintf measures; 4.4BSD
 * returned EOF and tmux died with no message), and libixcompat's strtoll,
 * strtoull, fmod, round, basename, dirname, nl_langinfo, gmtime_r,
 * ctime_r, strsignal, fseeko/ftello. ok/FAIL per line. */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <stddef.h>
#include <libgen.h>
#include <langinfo.h>

static int passed, total;

static void check(int ok, const char *what)
{
    total++;
    passed += ok != 0;
    printf("%s %d %s\n", ok ? "ok" : "FAIL", total, what);
}

static int vs(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

/* "ixc99 tty", in a console window: stdin is a terminal, open for reading
 * and writing as on Unix (tmux draws on a dup of it; select() left it out
 * of every write set) */
static int tty_mode(void)
{
    fd_set w;
    struct timeval tv = { 2, 0 };
    int n;
    FD_ZERO(&w);
    FD_SET(0, &w);
    n = select(1, 0, &w, 0, &tv);
    check(n == 1 && FD_ISSET(0, &w), "select for writing on stdin (a console)");
    check(write(0, "[written to fd 0]\n", 18) == 18, "write to stdin (a console)");
    printf("ixc99 tty: passed %d of %d\n", passed, total);
    return passed == total ? 0 : 10;
}

int main(int argc, char **argv)
{
    char b[64];
    struct tm tm;
    time_t t = 86400 + 3600;
    FILE *f;

    if (argc > 1 && !strcmp(argv[1], "tty"))
        return tty_mode();

    check(snprintf(NULL, 0, "%d-%s", 12345, "ab") == 8, "snprintf(NULL, 0) returns the length");
    {
        int r = vs(NULL, 0, "%s", "hello");
        sprintf(b, "vsnprintf(NULL, 0) returns the length (%d)", r);
        check(r == 5, b);
        {
            char out[64], what[100];
            memset(out, '#', sizeof out);
            out[20] = 0;
            r = vs(out, sizeof out, "%s-%d", "x", 42);
            sprintf(what, "vsnprintf into a buffer (%d [%.20s])", r, out);
            check(r == 4 && !strcmp(out, "x-42"), what);
        }
    }
    check(snprintf(b, 4, "%s", "abcdef") == 6 && !strcmp(b, "abc"), "snprintf truncates, returns the full length");
    {
        /* C99 length modifiers: each consumes its argument (an unknown one
         * left it, and every later argument shifted) */
        size_t z = 123;
        long long ll = -5000000000LL;
        sprintf(b, "%zu|%hhu|%jd|%td|%lld|%s", z, (unsigned char)300, (long long)-7, (ptrdiff_t)-9, ll, "end");
        check(!strcmp(b, "123|44|-7|-9|-5000000000|end"), b);
    }
    check(strtoll("-9000000000", NULL, 10) == -9000000000LL, "strtoll past 32 bits");
    check(strtoull("18446744073709551615", NULL, 10) == 18446744073709551615ULL, "strtoull to the top");
    check(fmod(7.5, 2.0) == 1.5 && fmod(-7.5, 2.0) == -1.5, "fmod keeps the sign of x");
    check(round(2.5) == 3.0 && round(-2.5) == -3.0 && round(2.4) == 2.0, "round: half away from zero");
    check(!strcmp(basename("/a/b/c.txt"), "c.txt") && !strcmp(basename("VTC:x"), "x"), "basename");
    check(!strcmp(dirname("/a/b/c.txt"), "/a/b") && !strcmp(dirname("VTC:x"), "VTC:") &&
          !strcmp(dirname("c.txt"), "."), "dirname");
    check(!strcmp(nl_langinfo(CODESET), "ISO8859-1"), "nl_langinfo(CODESET)");
    check(gmtime_r(&t, &tm) == &tm && tm.tm_mday == 2 && tm.tm_hour == 1, "gmtime_r");
    check(ctime_r(&t, b) == b && strlen(b) == 25, "ctime_r");
    check(strsignal(SIGINT) != NULL && strlen(strsignal(SIGINT)) > 0, "strsignal");
    if ((f = fopen("/T/ixc99.tmp", "w+")) != NULL) {
        fputs("0123456789", f);
        check(fseeko(f, 4, SEEK_SET) == 0 && ftello(f) == 4 && fgetc(f) == '4', "fseeko/ftello");
        fclose(f);
        remove("/T/ixc99.tmp");
    } else
        check(0, "fseeko/ftello (no temporary file)");
    printf("ixc99: passed %d of %d\n", passed, total);
    return passed == total ? 0 : 10;
}
