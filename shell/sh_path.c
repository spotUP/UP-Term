/* sh_path: a Unix-style name as AmigaDOS writes it (see sh_path.h). */
#include <string.h>
#include "sh_path.h"

int sh_vol_slashes(const char *in)
{
    const char *c = in;
    long n = 0;
    while (*c && *c != ':' && *c != '/')
        c++;
    if (*c != ':' || c == in)
        return 0;
    while (c[1 + n] == '/')
        n++;
    return (int)n;
}

void sh_amiga_name(const char *in, char *out, int max)
{
    int n = 0, slashes;
    /* the Unix devices a script names: /dev/null is NIL:, /dev/tty the console (". /dev/null",
     * test -c /dev/null) */
    if (!strcmp(in, "/dev/null") || !strcmp(in, "/dev/tty")) {
        strncpy(out, in[5] == 'n' ? "NIL:" : "*", (size_t)max - 1);
        out[max - 1] = 0;
        return;
    }
    for (;;) {
        if (!strncmp(in, "./", 2))
            in += 2;
        else if (!strncmp(in, "../", 3) || !strcmp(in, "..")) {
            if (n < max - 1)
                out[n++] = '/';
            in += in[2] ? 3 : 2;
        } else if (!strcmp(in, "."))
            in++;
        else
            break;
    }
    out[n] = 0;
    if ((slashes = sh_vol_slashes(in)) != 0) {
        /* "Vol:/x" is Vol:x: HOME is a volume root and POSIX code writes $HOME/x */
        size_t vl = (size_t)(strchr(in, ':') - in) + 1;
        strncat(out, in, vl < (size_t)(max - n - 1) ? vl : (size_t)(max - n - 1));
        in += vl + slashes;
    }
    strncat(out, in, max - strlen(out) - 1);
}
