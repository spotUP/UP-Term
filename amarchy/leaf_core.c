/* The portable decision of leaf.h: which way a leaf's kind runs. */
#include <stdio.h>
#include <string.h>
#include "leaf.h"

lf_how lf_plan(int kind)
{
    switch (kind) {
    case KA_RUN:
        return LF_WINDOW;
    case KA_DETACH:
        return LF_DETACH;
    case KA_REXX:
        return LF_REXX;
    case KA_OPEN:
        return LF_OPEN;
    default:
        return LF_REQUEST;
    }
}

int lf_screen_spec(char *out, int cap, const char *spec, const char *screen)
{
    static const char opt[] = "/SCREEN ";
    int n = 0, i;
    if (cap < 1)
        return 0;
    out[0] = 0;
    for (i = 0; spec[i]; i++) {
        if (n >= cap - 1)
            return 0;
        out[n++] = spec[i];
    }
    if (screen && screen[0]) {
        for (i = 0; opt[i]; i++) {
            if (n >= cap - 1) { out[0] = 0; return 0; }
            out[n++] = opt[i];
        }
        for (i = 0; screen[i]; i++) {
            if (screen[i] == '/' || n >= cap - 1) { out[0] = 0; return 0; }
            out[n++] = screen[i];
        }
    }
    out[n] = 0;
    return 1;
}

int lf_shell_command(char *out, int cap, const char *vsh, const char *cmd)
{
    int n;
    const char *p;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!cmd || !*cmd)
        return 0;
    if (!vsh || !*vsh) {                /* an older install: the AmigaDOS command path */
        if ((int)strlen(cmd) >= cap)
            return 0;
        strcpy(out, cmd);
        return 1;
    }
    for (p = cmd; *p; p++)              /* a quote would end the argument; no control characters */
        if (*p == '"' || (unsigned char)*p < ' ')
            return 0;
    n = snprintf(out, (size_t)cap, "\"%s\" -c \"%s\"", vsh, cmd);
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

int lf_launch_fill(lf_launch *b, const char *path)
{
    memset(b, 0, sizeof *b);
    if (!path || !*path || strlen(path) >= LF_LAUNCH_PATH)
        return 0;
    strcpy(b->path, path);
    return 1;
}
