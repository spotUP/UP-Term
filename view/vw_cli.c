/* vw_cli -- the shared options (vw_cli.h). */
#include <stdlib.h>
#include <string.h>
#include "vw_cli.h"
#include "vw_plat.h"

const char vw_cli_help[] =
    "  --color=auto|always|never  colours (auto: on a console)\n"
    "  --theme NAME|FILE          ansi (default), mono, rich, or a theme file\n"
    "                             (also the variable HL_THEME)\n"
    "  --colors 16|256|24         colour depth (default: from TERM/COLORTERM)\n"
    "  --utf8, --latin1           the terminal's character set (default: from\n"
    "                             TERM and LANG)\n";

void vw_cli_init(vw_cli *c)
{
    c->color = -1;
    c->depth = 0;
    c->cs = -1;
    c->theme = 0;
}

long vw_number(const char *s)
{
    long v = 0;
    if (!s || !*s)
        return -1;
    while (*s) {
        if (*s < '0' || *s > '9' || v > 100000L)
            return -1;
        v = v * 10 + (*s++ - '0');
    }
    return v;
}

/* --name=value or --name value: the value, 0 when absent */
static const char *value(const char *arg, const char *name, int argc, char **argv, int *i, int *took)
{
    size_t k = strlen(name);
    *took = 0;
    if (strncmp(arg, name, k))
        return 0;
    if (arg[k] == '=') {
        *took = 1;
        return arg + k + 1;
    }
    if (arg[k] == 0) {
        *took = 1;
        if (*i + 1 < argc)
            return argv[++*i];
        return "";
    }
    return 0;
}

static int bad(const char *what, const char *arg)
{
    vw_say(what);
    vw_say(arg);
    vw_say("\n");
    return -1;
}

int vw_cli_option(vw_cli *c, int argc, char **argv, int *i)
{
    const char *a = argv[*i], *v;
    int took;
    if ((v = value(a, "--color", argc, argv, i, &took)) != 0 ||
        (v = value(a, "--colour", argc, argv, i, &took)) != 0) {
        if (!strcmp(v, "auto"))
            c->color = -1;
        else if (!strcmp(v, "always") || !strcmp(v, "on"))
            c->color = 1;
        else if (!strcmp(v, "never") || !strcmp(v, "off"))
            c->color = 0;
        else
            return bad("--color takes auto, always or never, not ", v);
        ++*i;
        return 1;
    }
    if ((v = value(a, "--theme", argc, argv, i, &took)) != 0) {
        if (!*v)
            return bad("--theme needs a name or a file", "");
        c->theme = v;
        ++*i;
        return 1;
    }
    if ((v = value(a, "--colors", argc, argv, i, &took)) != 0) {
        long d = vw_number(v);
        if (!strcmp(v, "truecolor") || !strcmp(v, "24bit"))
            d = 24;
        if (d != 16 && d != 256 && d != 24 && d != 8)
            return bad("--colors takes 16, 256 or 24, not ", v);
        c->depth = d == 8 ? 16 : (int)d;
        ++*i;
        return 1;
    }
    if (!strcmp(a, "--utf8") || !strcmp(a, "--utf-8")) {
        c->cs = VW_UTF8;
        ++*i;
        return 1;
    }
    if (!strcmp(a, "--latin1") || !strcmp(a, "--ascii")) {
        c->cs = VW_LATIN1;
        ++*i;
        return 1;
    }
    (void)took;
    return 0;
}

static int contains(const char *s, const char *w)
{
    return strstr(s, w) != 0;
}

int vw_term_utf8(void)
{
    char v[80];
    if ((vw_env("LC_ALL", v, sizeof(v)) && *v) || (vw_env("LC_CTYPE", v, sizeof(v)) && *v) ||
        (vw_env("LANG", v, sizeof(v)) && *v)) {
        if (contains(v, "UTF-8") || contains(v, "utf8") || contains(v, "UTF8") || contains(v, "utf-8"))
            return 1;
        if (strcmp(v, "C") && strcmp(v, "POSIX"))
            return 0; /* a locale with another character set */
    }
    if (vw_env("TERM", v, sizeof(v)))
        return contains(v, "vtcon") || !strncmp(v, "xterm", 5) || !strncmp(v, "screen", 6) ||
               !strncmp(v, "tmux", 4) || contains(v, "256color") || contains(v, "kitty") ||
               contains(v, "alacritty") || contains(v, "wezterm");
    return 0;
}

static int load_theme(vw_cli *c, const char *name)
{
    vw_file *f;
    char *buf = 0, err[96];
    long n = 0, cap = 0, k;
    if (hl_theme_builtin(&c->th, name))
        return 0;
    f = vw_open(name);
    if (!f) {
        vw_say("no theme ");
        vw_say(name);
        vw_say(" (built in: ");
        vw_say(hl_theme_names());
        vw_say(")\n");
        return -1;
    }
    for (;;) {
        if (n + 1024 > cap) {
            char *nb = (char *)realloc(buf, cap + 4096);
            if (!nb)
                break;
            buf = nb;
            cap += 4096;
        }
        k = vw_read(f, buf + n, 1024);
        if (k <= 0)
            break;
        n += k;
    }
    vw_close(f);
    hl_theme_builtin(&c->th, "ansi");
    if (hl_theme_parse(&c->th, buf ? buf : "", n, err, sizeof(err)) < 0) {
        vw_say(name);
        vw_say(": ");
        vw_say(err);
        vw_say("\n");
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

int vw_cli_start(vw_cli *c, int tty, vw_out *o)
{
    char term[80], ct[40], th[256];
    int color = c->color < 0 ? tty : c->color;
    int depth = c->depth;
    const char *theme = c->theme;
    if (!theme && vw_env("HL_THEME", th, sizeof(th)) && *th)
        theme = th;
    if (load_theme(c, theme ? theme : "ansi") < 0)
        return -1;
    if (!depth) {
        if (!vw_env("TERM", term, sizeof(term)))
            term[0] = 0;
        if (!vw_env("COLORTERM", ct, sizeof(ct)))
            ct[0] = 0;
        depth = hl_depth_from_env(term, ct);
    }
    vo_init(o, vw_write, 0, c->cs >= 0 ? c->cs : vw_term_utf8() ? VW_UTF8 : VW_LATIN1,
            color ? depth : 0, &c->th);
    return 0;
}

int vw_wrap_width(long width)
{
    char v[16];
    if (width >= 10)
        return (int)width;
    width = vw_columns();
    if (width >= 10)
        return (int)width - 1;
    width = vw_env("COLUMNS", v, sizeof(v)) ? vw_number(v) : 0;
    return width >= 10 ? (int)width : 80;
}

int vw_osc8_ok(const vw_out *o)
{
    return o->depth > 0 && o->cs == VW_UTF8;
}

/* the whole stream in memory: 0 when it could not be read */
char *vw_slurp(vw_file *f, long *len)
{
    char *buf = 0;
    long n = 0, cap = 0, k;
    for (;;) {
        if (n + 4096 > cap) {
            char *nb = (char *)realloc(buf, cap + 16384);
            if (!nb) {
                free(buf);
                return 0;
            }
            buf = nb;
            cap += 16384;
        }
        k = vw_read(f, buf + n, 4096);
        if (k < 0) {
            free(buf);
            return 0;
        }
        if (k == 0)
            break;
        n += k;
        if (vw_break()) {
            free(buf);
            return 0;
        }
    }
    *len = n;
    return buf;
}
