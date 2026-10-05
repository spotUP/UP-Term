/* glob -- see glob.h. A small backtracking matcher: the paths are short
 * and '**' is tried at directory boundaries only. */
#include <string.h>
#include "glob.h"
#include "util.h"

static int lower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (c >= 0xc0 && c <= 0xde && c != 0xd7)
        return c + 32;
    return c;
}

/* the end of a {..} group starting at p ('{'): the '}' or 0 */
static const char *brace_end(const char *p)
{
    int d = 0;
    for (; *p; p++) {
        if (*p == '{')
            d++;
        else if (*p == '}' && !--d)
            return p;
    }
    return 0;
}

/* [..] at p against c: 1 / 0, *after the pattern past it; -1 not a class */
static int class_match(const char *p, int c, const char **after)
{
    int neg = 0, in = 0, first = 1;
    p++;
    if (*p == '!' || *p == '^') {
        neg = 1;
        p++;
    }
    for (; *p && (*p != ']' || first); first = 0) {
        int lo = lower((unsigned char)*p), hi;
        p++;
        hi = lo;
        if (*p == '-' && p[1] && p[1] != ']') {
            hi = lower((unsigned char)p[1]);
            p += 2;
        }
        if (c >= lo && c <= hi)
            in = 1;
    }
    if (*p != ']')
        return -1;
    *after = p + 1;
    return neg ? !in : in;
}

static int match(const char *p, const char *s, int depth);

/* {a,b}rest: each alternative followed by rest */
static int alternatives(const char *p, const char *s, int depth)
{
    const char *end = brace_end(p), *a = p + 1;
    char buf[512];
    if (!end)
        return *s == '{' && match(p + 1, s + 1, depth + 1);
    while (a < end) {
        const char *b = a;
        int d = 0;
        long al, rl = (long)strlen(end + 1);
        while (b < end && (d || *b != ',')) {
            if (*b == '{')
                d++;
            else if (*b == '}')
                d--;
            b++;
        }
        al = (long)(b - a);
        if (al + rl < (long)sizeof(buf)) {
            memcpy(buf, a, (size_t)al);
            memcpy(buf + al, end + 1, (size_t)rl + 1);
            if (match(buf, s, depth + 1))
                return 1;
        }
        a = b + 1;
    }
    return 0;
}

static int match(const char *p, const char *s, int depth)
{
    if (depth > 64)
        return 0;
    for (;;) {
        if (!*p)
            return !*s;
        if (p[0] == '*' && p[1] == '*' && (p[2] == '/' || !p[2])) {
            /* any number of whole directories (none included) */
            const char *rest = p[2] ? p + 3 : p + 2;
            if (!*rest)
                return 1;
            if (match(rest, s, depth + 1))
                return 1;
            for (; *s; s++)
                if (*s == '/' && match(rest, s + 1, depth + 1))
                    return 1;
            return 0;
        }
        switch (*p) {
        case '*':
            while (*p == '*')
                p++;
            for (;; s++) {
                if (match(p, s, depth + 1))
                    return 1;
                if (!*s || *s == '/')
                    return 0;
            }
        case '?':
            if (!*s || *s == '/')
                return 0;
            p++;
            s++;
            break;
        case '[': {
            const char *after;
            int r;
            if (!*s || *s == '/')
                return 0;
            r = class_match(p, lower((unsigned char)*s), &after);
            if (r < 0) {
                if (*s != '[')
                    return 0;
                p++;
                s++;
                break;
            }
            if (!r)
                return 0;
            p = after;
            s++;
            break;
        }
        case '{':
            return alternatives(p, s, depth);
        case '\\':
            if (p[1])
                p++;
            /* fall through */
        default:
            if (lower((unsigned char)*p) != lower((unsigned char)*s))
                return 0;
            p++;
            s++;
            break;
        }
    }
}

int glob_match(const char *pat, const char *path)
{
    return match(pat, path, 0);
}

int glob_wild(const char *p)
{
    for (; *p; p++)
        if (*p == '*' || *p == '?' || *p == '[' || *p == '{' || *p == '#')
            return 1;
    return 0;
}

int glob_depth(const char *p)
{
    int d = 0;
    if (strstr(p, "**"))
        return -1;
    for (; *p; p++)
        d += *p == '/';
    return d;
}

int glob_from_amiga(const char *in, char *out, long cap)
{
    long o = 0;
    int paren = 0;
    for (; *in; in++) {
        const char *put = 0;
        char one[2];
        if (*in == '#') {
            if (in[1] != '?') {
                cl_copy(out, "only #? of the AmigaDOS repetitions is understood (use * instead)", cap);
                return -1;
            }
            put = "*";
            in++;
        } else if (*in == '~') {
            cl_copy(out, "the AmigaDOS ~ (not) is not understood in a Glob pattern", cap);
            return -1;
        } else if (*in == '(') {
            paren++;
            put = "{";
        } else if (*in == ')' && paren) {
            paren--;
            put = "}";
        } else if (*in == '|' && paren) {
            put = ",";
        } else if (*in == '%') {
            put = "";
        } else if (*in == '\'' && in[1]) {
            /* AmigaDOS's escape */
            in++;
            one[0] = '\\';
            one[1] = 0;
            if (o + 1 >= cap)
                return -1;
            out[o++] = '\\';
            one[0] = *in;
            one[1] = 0;
            put = one;
        } else {
            one[0] = *in;
            one[1] = 0;
            put = one;
        }
        if (o + (long)strlen(put) >= cap) {
            cl_copy(out, "the pattern is too long", cap);
            return -1;
        }
        strcpy(out + o, put);
        o += (long)strlen(put);
    }
    out[o] = 0;
    return 0;
}
