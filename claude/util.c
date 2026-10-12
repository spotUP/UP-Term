/* util -- see util.h. */
#include <string.h>
#include "util.h"

int cl_ltoa(long v, char *out)
{
    char t[12];
    int n = 0, i = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (v < 0)
        out[i++] = '-';
    while (n)
        out[i++] = t[--n];
    out[i] = 0;
    return i;
}

char *cl_copy(char *dst, const char *src, long cap)
{
    long n = (long)strlen(src);
    if (cap <= 0)
        return dst;
    if (n > cap - 1)
        n = cap - 1;
    memcpy(dst, src, (size_t)n);
    dst[n] = 0;
    return dst;
}

char *cl_cat(char *dst, const char *src, long cap)
{
    long l = (long)strlen(dst);
    if (l < cap)
        cl_copy(dst + l, src, cap - l);
    return dst;
}

/* c appended at *n of cap bytes (one kept for the terminator): 0, -1 full */
static int put(char *line, long cap, long *n, char c)
{
    if (*n + 1 >= cap)
        return -1;
    line[(*n)++] = c;
    line[*n] = 0;
    return 0;
}

static int put_arg(char *line, long cap, long *n, const char *a, int quote)
{
    if (quote && put(line, cap, n, '"'))
        return -1;
    for (; *a; a++) {
        if (quote && (*a == '"' || *a == '*' || *a == '\n')) {
            if (put(line, cap, n, '*') || put(line, cap, n, *a == '\n' ? 'N' : *a))
                return -1;
        } else if (put(line, cap, n, *a))
            return -1;
    }
    return quote && put(line, cap, n, '"') ? -1 : 0;
}

int cl_shell_line(char *line, long cap, const char *const *argv)
{
    long n = 0;
    int i, quote = !argv[0][0];
    const char *a;
    if (cap < 1)
        return -1;
    line[0] = 0;
    for (a = argv[0]; *a; a++)
        if (strchr(" \t\"*\n;<>|&`${}", *a))
            quote = 1;
    if (put_arg(line, cap, &n, argv[0], quote))
        return -1;
    for (i = 1; argv[i]; i++)
        if (put(line, cap, &n, ' ') || put_arg(line, cap, &n, argv[i], 1))
            return -1;
    return 0;
}

static int low(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int cl_strnieq(const char *a, const char *b, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        if (low((unsigned char)a[i]) != low((unsigned char)b[i]))
            return 0;
        if (!a[i])
            return 1;
    }
    return 1;
}

int cl_strieq(const char *a, const char *b)
{
    while (*a && low((unsigned char)*a) == low((unsigned char)*b)) {
        a++;
        b++;
    }
    return low((unsigned char)*a) == low((unsigned char)*b);
}

long cl_runcommand_args(const char *buf, long pos, long end, const char *args)
{
    long alen = args ? (long)strlen(args) : 0, n = end - pos, i;
    if (alen <= 0 || !buf || pos < 0 || n < alen || memcmp(buf + pos, args, (size_t)alen))
        return 0;
    for (i = pos + alen; i < end && (buf[i] == ' ' || buf[i] == '\t'); i++)
        ;
    if (i < end && buf[i] == '\n')
        i++;
    return i == end ? n : 0;
}

void cl_uuid(unsigned long *seed, char *out)
{
    static const char hex[] = "0123456789abcdef";
    int i, k = 0;
    for (i = 0; i < 32; i++) {
        unsigned d;
        if (!(i & 7))
            *seed = *seed * 1103515245UL + 12345UL;
        d = (unsigned)(*seed >> (4 * (i & 7))) & 15;
        if (i == 12)
            d = 4;              /* version 4 */
        else if (i == 16)
            d = 8 | (d & 3);    /* the variant */
        if (i == 8 || i == 12 || i == 16 || i == 20)
            out[k++] = '-';
        out[k++] = hex[d];
    }
    out[k] = 0;
}
