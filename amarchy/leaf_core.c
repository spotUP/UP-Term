/* The portable decision of leaf.h: which way a leaf's kind runs. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "leaf.h"

lf_how lf_plan(int kind)
{
    switch (kind) {
    case KA_RUN:
        return LF_WINDOW;
    case KA_DETACH:
        return LF_DETACH;
    case KA_SCAN:
        return LF_SCAN;
    case KA_REXX:
        return LF_REXX;
    case KA_OPEN:
        return LF_OPEN;
    case KA_VIEWER:
        return LF_VIEWER;
    case KA_WIN:
        return LF_WIN;
    default:
        return LF_REQUEST;
    }
}

const char *lf_rexx_notice(const char *script, long rc)
{
    const char *base = script ? script : "";
    const char *p;
    for (p = base; *p; p++)
        if (*p == '/' || *p == ':')
            base = p + 1;
    if (rc == 5 && strncmp(base, "dopus-", 6) == 0)
        return "Directory Opus is not running";
    return 0;
}

int lf_rexx_notice_line(char *out, int cap, const char *script, const char *spec)
{
    const char *note = lf_rexx_notice(script, 5);
    const char *p;
    int n;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!note || !spec || !*spec)
        return 0;
    for (p = script; *p; p++)
        if (*p == '\'' || *p == '"' || (unsigned char)*p < ' ')
            return 0;
    for (p = spec; *p; p++)
        if (*p == '\'' || *p == '"' || (unsigned char)*p < ' ')
            return 0;
    n = snprintf(out, (size_t)cap,
                 "RX \"r = '%s'(); if r = 5 then do; if open(w, '%s', 'W') then do; "
                 "call writech(w, '%s'); address command 'Wait 3 SECS'; call close(w); end; end\"",
                 script, spec, note);
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
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

/* Directory Opus 5 has no per-lister raise (DOpusM2_ARexx.guide): `dopus front` moves the whole Opus screen forward,
 * and `lister set <h> visible off` then `visible on` re-opens that one lister on top of the others. */
int lf_lister_front_command(char *out, int cap, int step, unsigned long handle)
{
    int n;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (step == 0)
        n = snprintf(out, (size_t)cap, "dopus front");
    else if (step == 1 || step == 2)
        n = snprintf(out, (size_t)cap, "lister set %lu visible %s", handle, step == 1 ? "off" : "on");
    else
        return 0;
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

int lf_viewer_command(char *out, int cap, const char *dir)
{
    int n;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!dir || !*dir || strchr(dir, '"'))
        return 0;
    n = snprintf(out, (size_t)cap, "\"%s%samarchy-gui\" viewer", dir,
                 dir[strlen(dir) - 1] == ':' || dir[strlen(dir) - 1] == '/' ? "" : "/");
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
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

/* the n characters at s end in suffix, any case (vbcc has no strncasecmp) */
static int ends_ci(const char *s, int n, const char *suffix)
{
    int k = (int)strlen(suffix), i;
    if (n <= k)
        return 0;
    for (i = 0; i < k; i++)
        if (tolower((unsigned char)s[n - k + i]) != suffix[i])
            return 0;
    return 1;
}

/* the first word of arg names a script file rather than starting ARexx program text */
static int rexx_is_script(const char *arg)
{
    int n = 0;
    while (arg[n] && arg[n] != ' ' && arg[n] != '\t') {
        if (arg[n] == ':' || arg[n] == '/')
            return 1;
        n++;
    }
    return ends_ci(arg, n, ".rexx") || ends_ci(arg, n, ".rx");
}

int lf_rexx_command(char *out, int cap, const char *arg)
{
    int n = 0, script;
    const char *p;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!arg || !*arg)
        return 0;
    for (p = arg; *p; p++)
        if ((unsigned char)*p < ' ' && *p != '\t')
            return 0;
    script = rexx_is_script(arg);
    n = snprintf(out, (size_t)cap, script ? "RX %s" : "RX \"%s\"", arg);
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    if (!script)                        /* a '"' inside the string would end it early */
        for (n = 4; out[n + 1]; n++)
            if (out[n] == '"')
                out[n] = '\'';
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

int lf_clip_entry(const char *path)
{
    int n = 0;
    if (strncmp(path, "clip:", 5))
        return -2;
    path += 5;
    if (!*path || !strcmp(path, "none"))
        return -1;
    for (; *path; path++) {
        if (*path < '0' || *path > '9' || n > 1000)
            return -1;
        n = n * 10 + (*path - '0');
    }
    return n;
}
