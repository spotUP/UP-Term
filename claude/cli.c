/* cli -- see cli.h. */
#include <stdlib.h>
#include <string.h>
#include "cli.h"
#include "path.h"
#include "util.h"

/* ---- the words ---- */

int cli_split(const char *line, cl_args *a)
{
    long n = (long)strlen(line), k = 0;
    const char *p = line;
    int cap = 8, cur = 0;
    memset(a, 0, sizeof(*a));
    a->buf = (char *)malloc((size_t)n + 1);
    a->v = (char **)malloc((size_t)cap * sizeof(char *));
    a->quoted = (char *)malloc((size_t)cap);
    if (!a->buf || !a->v || !a->quoted)
        return -1;
    for (;;) {
        int q = 0, was = 0;
        long start;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (!*p)
            break;
        start = k;
        while (*p && (q || (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'))) {
            if (*p == '"') {
                q = !q;
                was = 1;
                p++;
                continue;
            }
            if (q && *p == '*' && p[1] && strchr("NnEe\"*", p[1])) {
                /* *N *E *" ** inside quotes; any other star is itself ("Bash(make *)") */
                char c = p[1];
                p += 2;
                a->buf[k++] = c == 'N' || c == 'n' ? '\n' : c == 'E' || c == 'e' ? 27 : c;
                continue;
            }
            a->buf[k++] = *p++;
        }
        a->buf[k++] = 0;
        if (cur == cap) {
            char **v;
            char *qq;
            cap *= 2;
            v = (char **)realloc(a->v, (size_t)cap * sizeof(char *));
            if (!v)
                return -1;
            a->v = v;
            qq = (char *)realloc(a->quoted, (size_t)cap);
            if (!qq)
                return -1;
            a->quoted = qq;
        }
        a->v[cur] = a->buf + start;
        a->quoted[cur] = (char)was;
        cur++;
    }
    a->n = cur;
    return 0;
}

void cli_args_free(cl_args *a)
{
    free(a->buf);
    free(a->v);
    free(a->quoted);
    memset(a, 0, sizeof(*a));
}

/* ---- the options ---- */

enum {
    O_PRINT, O_MODEL, O_EFFORT, O_URL, O_ROOT, O_PING, O_DEBUG, O_PLAIN, O_CONTINUE, O_RESUME, O_NAME, O_FORK,
    O_NOPERSIST, O_FALLBACK, O_OUTFMT, O_INFMT, O_PARTIAL, O_PERM, O_SKIP, O_ALLOW, O_DENY, O_TOOLS, O_ADDDIR,
    O_SYSP, O_SYSPF, O_APPEND, O_APPENDF, O_SETTINGS, O_MAXTURNS, O_BUDGET, O_VERBOSE, O_AGENT, O_VERSION,
    O_HELP
};

/* takes: 0 a switch, 1 a value, 2 an optional value, 3 values (Claude Code's variadic flags) */
typedef struct opt {
    int id;
    const char *flag;           /* --flag ("" none) */
    char short_;                /* -x (0 none) */
    const char *alt;            /* another --flag spelling, 0 none */
    const char *kw;             /* the AmigaDOS keyword, '-' ignored when matching */
    const char *kw2;            /* its alias, 0 none */
    int takes;
} opt;

static const opt opts[] = {
    { O_PRINT, "print", 'p', 0, "PRINT", "P", 0 },
    { O_MODEL, "model", 0, 0, "MODEL", 0, 1 },
    { O_EFFORT, "effort", 0, 0, "EFFORT", 0, 1 },
    { O_URL, "url", 0, 0, "URL", 0, 1 },
    { O_ROOT, "root", 0, 0, "ROOT", 0, 1 },
    { O_PING, "ping", 0, 0, "PING", 0, 0 },
    { O_DEBUG, "debug", 0, 0, "DEBUG", 0, 0 },
    { O_PLAIN, "plain", 0, 0, "PLAIN", 0, 0 },
    { O_CONTINUE, "continue", 'c', 0, "CONTINUE", "C", 0 },
    { O_RESUME, "resume", 'r', 0, "RESUME", "R", 2 },
    { O_NAME, "name", 'n', 0, "NAME", "N", 1 },
    { O_FORK, "fork-session", 0, 0, "FORK-SESSION", 0, 0 },
    { O_NOPERSIST, "no-session-persistence", 0, 0, "NO-SESSION-PERSISTENCE", 0, 0 },
    { O_FALLBACK, "fallback-model", 0, 0, "FALLBACK", "FALLBACK-MODEL", 1 },
    { O_OUTFMT, "output-format", 0, 0, "OUTPUT-FORMAT", 0, 1 },
    { O_INFMT, "input-format", 0, 0, "INPUT-FORMAT", 0, 1 },
    { O_PARTIAL, "include-partial-messages", 0, 0, "INCLUDE-PARTIAL-MESSAGES", 0, 0 },
    { O_PERM, "permission-mode", 0, 0, "PERMISSION-MODE", 0, 1 },
    { O_SKIP, "dangerously-skip-permissions", 0, 0, "DANGEROUSLY-SKIP-PERMISSIONS", 0, 0 },
    { O_ALLOW, "allowedTools", 0, "allowed-tools", "ALLOWED-TOOLS", 0, 3 },
    { O_DENY, "disallowedTools", 0, "disallowed-tools", "DISALLOWED-TOOLS", 0, 3 },
    { O_TOOLS, "tools", 0, 0, "TOOLS", 0, 1 },
    { O_ADDDIR, "add-dir", 0, 0, "ADD-DIR", 0, 3 },
    { O_SYSP, "system-prompt", 0, 0, "SYSTEM-PROMPT", 0, 1 },
    { O_SYSPF, "system-prompt-file", 0, 0, "SYSTEM-PROMPT-FILE", 0, 1 },
    { O_APPEND, "append-system-prompt", 0, 0, "APPEND-SYSTEM-PROMPT", 0, 1 },
    { O_APPENDF, "append-system-prompt-file", 0, 0, "APPEND-SYSTEM-PROMPT-FILE", 0, 1 },
    { O_SETTINGS, "settings", 0, 0, "SETTINGS", 0, 1 },
    { O_MAXTURNS, "max-turns", 0, 0, "MAX-TURNS", 0, 1 },
    { O_BUDGET, "max-budget-usd", 0, 0, "MAX-BUDGET-USD", 0, 1 },
    { O_VERBOSE, "verbose", 0, 0, "VERBOSE", 0, 0 },
    { O_AGENT, "agent", 0, 0, "AGENT", 0, 1 },
    { O_VERSION, "version", 'v', 0, "VERSION", 0, 0 },
    { O_HELP, "help", 'h', 0, "HELP", 0, 0 }
};
#define NOPTS ((int)(sizeof(opts) / sizeof(opts[0])))

void cli_init(cl_cli *c)
{
    memset(c, 0, sizeof(*c));
}

static void strs_free(cl_strs *s)
{
    int i;
    for (i = 0; i < s->n; i++)
        free(s->v[i]);
    free(s->v);
    memset(s, 0, sizeof(*s));
}

void cli_free(cl_cli *c)
{
    free(c->prompt);
    free(c->tools);
    free(c->sys_prompt);
    free(c->sys_file);
    free(c->app_prompt);
    free(c->app_file);
    free(c->settings);
    strs_free(&c->allow);
    strs_free(&c->deny);
    strs_free(&c->dirs);
    cli_init(c);
}

static char *dupn(const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (d) {
        memcpy(d, s, (size_t)n);
        d[n] = 0;
    }
    return d;
}

static int strs_add(cl_strs *s, const char *v, long n)
{
    char *d;
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 8;
        char **q = (char **)realloc(s->v, (size_t)nc * sizeof(char *));
        if (!q)
            return -1;
        s->v = q;
        s->cap = nc;
    }
    d = dupn(v, n);
    if (!d)
        return -1;
    s->v[s->n++] = d;
    return 0;
}

/* "Read,Edit" or "Bash(git log *) Read": items split on , and blanks outside parentheses */
static int strs_items(cl_strs *s, const char *v)
{
    while (*v) {
        const char *b;
        int depth = 0;
        while (*v == ' ' || *v == ',' || *v == '\t')
            v++;
        b = v;
        while (*v && (depth || (*v != ',' && *v != ' ' && *v != '\t'))) {
            if (*v == '(')
                depth++;
            else if (*v == ')' && depth)
                depth--;
            v++;
        }
        if (v > b && strs_add(s, b, (long)(v - b)))
            return -1;
    }
    return 0;
}

static int set_str(char **dst, const char *v)
{
    free(*dst);
    *dst = dupn(v, (long)strlen(v));
    return *dst ? 0 : -1;
}

static int fail(cl_cli *c, const char *a, const char *b, const char *d)
{
    cl_copy(c->err, a, sizeof(c->err));
    if (b)
        cl_cat(c->err, b, sizeof(c->err));
    if (d)
        cl_cat(c->err, d, sizeof(c->err));
    return -1;
}

static int one_of(const char *v, const char *const *set, int icase)
{
    int i;
    for (i = 0; set[i]; i++)
        if (icase ? cl_strieq(v, set[i]) : !strcmp(v, set[i]))
            return i;
    return -1;
}

static const char *const fmts_out[] = { "text", "json", "stream-json", 0 };
static const char *const fmts_in[] = { "text", "stream-json", 0 };
static const char *const efforts[] = { "low", "medium", "high", "xhigh", "max", 0 };
static const char *const modes[] = { "default", "acceptEdits", "plan", "dontAsk", "bypassPermissions", "manual",
                                     "auto", 0 };

/* "5", "0.25", "1.5" US dollars -> micro-dollars; -1 not a positive amount */
static long dollars(const char *v, unsigned long *out)
{
    unsigned long whole = 0, frac = 0;
    int digits = 0, fd = 0;
    while (*v >= '0' && *v <= '9') {
        if (whole > 100000UL)
            return -1;
        whole = whole * 10 + (unsigned long)(*v++ - '0');
        digits++;
    }
    if (*v == '.') {
        v++;
        while (*v >= '0' && *v <= '9') {
            if (fd < 6) {
                frac = frac * 10 + (unsigned long)(*v - '0');
                fd++;
            }
            v++;
            digits++;
        }
    }
    if (*v || !digits)
        return -1;
    while (fd++ < 6)
        frac *= 10;
    *out = whole * 1000000UL + frac;
    return *out ? 0 : -1;
}

/* the option's display name for a message: --flag or KEYWORD */
static void opt_name(const opt *o, int amiga, char *out, long cap)
{
    if (amiga)
        cl_copy(out, o->kw, cap);
    else {
        cl_copy(out, "--", cap);
        cl_cat(out, o->flag, cap);
    }
}

static int bad_choice(cl_cli *c, const opt *o, int amiga, const char *v, const char *choices)
{
    char nm[64];
    opt_name(o, amiga, nm, sizeof(nm));
    cl_copy(c->err, "error: option '", sizeof(c->err));
    cl_cat(c->err, nm, sizeof(c->err));
    cl_cat(c->err, "' argument '", sizeof(c->err));
    cl_cat(c->err, v, sizeof(c->err));
    cl_cat(c->err, "' is invalid. Allowed choices are ", sizeof(c->err));
    cl_cat(c->err, choices, sizeof(c->err));
    cl_cat(c->err, ".", sizeof(c->err));
    return -1;
}

/* one option with its value (v 0 for a switch or an absent optional value) */
static int set(cl_cli *c, const opt *o, const char *v, int amiga)
{
    int i;
    switch (o->id) {
    case O_PRINT:
        c->print = 1;
        break;
    case O_MODEL:
        cl_copy(c->model, v, sizeof(c->model));
        break;
    case O_EFFORT:
        if ((i = one_of(v, efforts, amiga)) < 0)
            return bad_choice(c, o, amiga, v, "low, medium, high, xhigh, max");
        cl_copy(c->effort, efforts[i], sizeof(c->effort));
        break;
    case O_URL:
        cl_copy(c->url, v, sizeof(c->url));
        break;
    case O_ROOT:
        cl_copy(c->root, v, sizeof(c->root));
        break;
    case O_PING:
        c->ping = 1;
        break;
    case O_DEBUG:
        c->debug = 1;
        break;
    case O_PLAIN:
        c->plain = 1;
        break;
    case O_CONTINUE:
        c->cont = 1;
        break;
    case O_RESUME:
        c->resume = 1;
        cl_copy(c->resume_name, v ? v : "", sizeof(c->resume_name));
        break;
    case O_NAME:
        cl_copy(c->name, v, sizeof(c->name));
        break;
    case O_FORK:
        c->fork = 1;
        break;
    case O_NOPERSIST:
        c->no_persist = 1;
        break;
    case O_FALLBACK: {
        /* Claude Code takes a chain "sonnet,haiku"; one fallback here: the first */
        long k = 0;
        while (v[k] && v[k] != ',' && v[k] != ' ')
            k++;
        cl_copy(c->fallback, v, k + 1 < (long)sizeof(c->fallback) ? k + 1 : (long)sizeof(c->fallback));
        break;
    }
    case O_OUTFMT:
        if ((i = one_of(v, fmts_out, amiga)) < 0)
            return bad_choice(c, o, amiga, v, "text, json, stream-json");
        c->out = i;
        break;
    case O_INFMT:
        if ((i = one_of(v, fmts_in, amiga)) < 0)
            return bad_choice(c, o, amiga, v, "text, stream-json");
        c->in = i ? CLI_STREAM : CLI_TEXT;
        break;
    case O_PARTIAL:
        c->partial = 1;
        break;
    case O_PERM:
        if ((i = one_of(v, modes, amiga)) < 0)
            return bad_choice(c, o, amiga, v, "default, acceptEdits, plan, dontAsk, bypassPermissions, manual");
        if (!strcmp(modes[i], "auto"))
            return fail(c, "Permission mode auto needs Claude Code's action classifier, which C:Claude does not "
                           "have. Use default, acceptEdits, plan, dontAsk or bypassPermissions.",
                        0, 0);
        cl_copy(c->perm, modes[i], sizeof(c->perm));
        break;
    case O_SKIP:
        c->skip_perms = 1;
        break;
    case O_ALLOW:
        return strs_items(&c->allow, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_DENY:
        return strs_items(&c->deny, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_ADDDIR:
        return strs_items(&c->dirs, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_TOOLS:
        c->has_tools = 1;
        return set_str(&c->tools, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_SYSP:
        return set_str(&c->sys_prompt, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_SYSPF:
        return set_str(&c->sys_file, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_APPEND:
        return set_str(&c->app_prompt, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_APPENDF:
        return set_str(&c->app_file, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_SETTINGS:
        return set_str(&c->settings, v) ? fail(c, "Out of memory.", 0, 0) : 0;
    case O_MAXTURNS: {
        long t = 0;
        const char *p = v;
        while (*p >= '0' && *p <= '9' && t < 100000)
            t = t * 10 + (*p++ - '0');
        if (*p || p == v || t <= 0)
            return fail(c, "error: --max-turns must be a positive number, not '", v, "'.");
        c->max_turns = (int)t;
        break;
    }
    case O_BUDGET:
        if (dollars(v, &c->budget_micro))
            return fail(c, "error: --max-budget-usd must be a positive amount in US dollars, not '", v, "'.");
        c->has_budget = 1;
        break;
    case O_VERBOSE:
        c->verbose = 1;
        break;
    case O_AGENT:
        cl_copy(c->agent, v, sizeof(c->agent));
        break;
    case O_VERSION:
        c->version = 1;
        break;
    case O_HELP:
        c->help = 1;
        break;
    }
    return 0;
}

/* a keyword's name equal to w (up to n characters), case-insensitive, '-' ignored */
static int kw_eq(const char *kw, const char *w, long n)
{
    long i = 0;
    if (!kw)
        return 0;
    for (;;) {
        while (*kw == '-')
            kw++;
        while (i < n && w[i] == '-')
            i++;
        if (!*kw || i >= n)
            return !*kw && i >= n;
        if (cl_strnieq(kw, w + i, 1) == 0)
            return 0;
        kw++;
        i++;
    }
}

/* the option a keyword word names (name: up to '=' or the end) */
static const opt *keyword(const char *w, const char **val)
{
    long n = 0;
    int i;
    while (w[n] && w[n] != '=')
        n++;
    *val = w[n] == '=' ? w + n + 1 : 0;
    if (!n)
        return 0;
    for (i = 0; i < NOPTS; i++)
        if (kw_eq(opts[i].kw, w, n) || kw_eq(opts[i].kw2, w, n))
            return &opts[i];
    return 0;
}

static int keyword_prompt(const char *w, const char **val)
{
    long n = 0;
    while (w[n] && w[n] != '=')
        n++;
    *val = w[n] == '=' ? w + n + 1 : 0;
    return kw_eq("PROMPT", w, n);
}

static const opt *by_flag(const char *name, long n)
{
    int i;
    for (i = 0; i < NOPTS; i++)
        if (((long)strlen(opts[i].flag) == n && !strncmp(opts[i].flag, name, (size_t)n)) ||
            (opts[i].alt && (long)strlen(opts[i].alt) == n && !strncmp(opts[i].alt, name, (size_t)n)))
            return &opts[i];
    return 0;
}

static const opt *by_short(char ch)
{
    int i;
    for (i = 0; i < NOPTS; i++)
        if (opts[i].short_ == ch)
            return &opts[i];
    return 0;
}

/* may word i be an option's value (not a flag, not a keyword)? */
static int is_value(int i, int argc, char **argv, const char *quoted, int flags_on)
{
    const char *v;
    if (i >= argc)
        return 0;
    if (flags_on && argv[i][0] == '-' && argv[i][1])
        return 0;
    return (quoted && quoted[i]) || !keyword(argv[i], &v);
}

static int add_prompt(cl_cli *c, const char *w)
{
    long a = c->prompt ? (long)strlen(c->prompt) : 0, b = (long)strlen(w);
    char *p = (char *)realloc(c->prompt, (size_t)(a + b + 2));
    if (!p)
        return fail(c, "Out of memory.", 0, 0);
    if (a)
        p[a++] = ' ';
    memcpy(p + a, w, (size_t)b + 1);
    c->prompt = p;
    return 0;
}

int cli_parse(cl_cli *c, int argc, char **argv, const char *quoted)
{
    int i, flags_on = 1, in_prompt = 0;
    for (i = 0; i < argc; i++) {
        const char *w = argv[i], *v;
        const opt *o;
        int q = quoted && quoted[i];
        if (flags_on && !strcmp(w, "--")) {
            flags_on = 0;
            continue;
        }
        if (argc == 1 && !q && !strcmp(w, "?")) {
            c->ask_template = 1;
            return 0;
        }
        if (flags_on && w[0] == '-' && w[1] == '-' && w[2]) {
            /* --flag, --flag=value, --flag value */
            long n = 2;
            while (w[n] && w[n] != '=')
                n++;
            o = by_flag(w + 2, n - 2);
            if (!o)
                return fail(c, "error: unknown option '", w, "'");
            v = w[n] == '=' ? w + n + 1 : 0;
            if (o->takes == 0 && v)
                return fail(c, "error: option '--", o->flag, "' takes no value");
            if (o->takes && !v && i + 1 < argc && (o->takes == 1 || is_value(i + 1, argc, argv, quoted, flags_on)))
                v = argv[++i];
            if ((o->takes == 1 || o->takes == 3) && !v)
                return fail(c, "error: option '--", o->flag, "' argument missing");
            if (set(c, o, v, 0))
                return -1;
            while (o->takes == 3 && is_value(i + 1, argc, argv, quoted, flags_on))
                if (set(c, o, argv[++i], 0))
                    return -1;
            continue;
        }
        if (flags_on && w[0] == '-' && w[1] && w[1] != '-') {
            /* -p, -pc, -r NAME, -n NAME */
            long k;
            for (k = 1; w[k]; k++) {
                o = by_short(w[k]);
                if (!o) {
                    char s[3];
                    s[0] = '-';
                    s[1] = w[k];
                    s[2] = 0;
                    return fail(c, "error: unknown option '", s, "'");
                }
                v = 0;
                if (o->takes) {
                    if (w[k + 1])
                        return fail(c, "error: option '--", o->flag, "' takes a value: it must come last in a group");
                    if (i + 1 < argc && (o->takes == 1 || is_value(i + 1, argc, argv, quoted, flags_on)))
                        v = argv[++i];
                    if (o->takes == 1 && !v)
                        return fail(c, "error: option '--", o->flag, "' argument missing");
                }
                if (set(c, o, v, 0))
                    return -1;
            }
            continue;
        }
        if (!in_prompt && !q && keyword_prompt(w, &v)) {
            /* PROMPT=text or PROMPT text...: the rest of the line (/F) */
            in_prompt = 1;
            if (v && *v && add_prompt(c, v))
                return -1;
            continue;
        }
        if (!in_prompt && !q && (o = keyword(w, &v)) != 0) {
            /* KEYWORD, KEYWORD=value, KEYWORD value */
            if (o->takes == 0) {
                if (v)
                    return fail(c, "Claude: ", o->kw, " takes no value (it is a switch)");
            } else if (!v) {
                if (i + 1 < argc && (o->takes != 2 || is_value(i + 1, argc, argv, quoted, flags_on)))
                    v = argv[++i];
                else if (o->takes != 2)
                    return fail(c, "Claude: ", o->kw, " needs a value");
            }
            if (set(c, o, v, 1))
                return -1;
            continue;
        }
        in_prompt = 1;
        if (add_prompt(c, w))
            return -1;
    }
    /* the combinations Claude Code refuses */
    if (c->out == CLI_STREAM && c->print && !c->verbose)
        return fail(c, "Error: When using --print, --output-format=stream-json requires --verbose", 0, 0);
    if (c->partial && (!c->print || c->out != CLI_STREAM))
        return fail(c, "Error: --include-partial-messages requires --print and --output-format=stream-json", 0, 0);
    if (c->in == CLI_STREAM && !c->print)
        return fail(c, "Error: --input-format=stream-json requires --print", 0, 0);
    if (c->sys_prompt && c->sys_file)
        return fail(c, "Error: Cannot use both --system-prompt and --system-prompt-file", 0, 0);
    if (c->fork && !c->cont && !c->resume)
        return fail(c, "Error: --fork-session requires --continue or --resume", 0, 0);
    if (c->print && c->resume && !c->resume_name[0])
        return fail(c, "Error: --resume needs a session ID or name in print mode", 0, 0);
    return 0;
}

int cli_parse_line(cl_cli *c, const char *line)
{
    cl_args a;
    int rc;
    if (cli_split(line, &a)) {
        cli_args_free(&a);
        return fail(c, "Out of memory.", 0, 0);
    }
    rc = cli_parse(c, a.n, a.v, a.quoted);
    cli_args_free(&a);
    return rc;
}

const char *cli_version(void)
{
    return "1.0 (C:Claude, Claude Code for AmigaOS)";
}

static const char *const usage[] = {
    "Usage: Claude [options] [prompt]\n",
    "       Claude PROMPT/F,PRINT=P/S,MODEL/K,... (Claude ? shows the whole template)\n",
    "\n",
    "Claude Code for the Amiga. Starts a conversation; with a prompt, starts with it.\n",
    "-p (PRINT) answers once and ends: for scripts, with text piped in (Type file | Claude -p \"explain\").\n",
    "\n",
    "  -p, --print                      answer once, print, end\n",
    "  --output-format text|json|stream-json   the print mode's output\n",
    "  --input-format text|stream-json  the print mode's input (stream-json: a message per line)\n",
    "  --include-partial-messages       stream-json: the stream's events as they come\n",
    "  -c, --continue                   go on with this directory's last conversation\n",
    "  -r, --resume [id or name]        go on with that one (none: choose from a list)\n",
    "  -n, --name NAME                  name the conversation\n",
    "  --fork-session                   with -c or -r: as a new conversation\n",
    "  --no-session-persistence         nothing saved (print mode)\n",
    "  --model NAME                     opus, sonnet, haiku, fable or a model id\n",
    "  --fallback-model NAME            when the model is overloaded\n",
    "  --effort low|medium|high|xhigh|max\n",
    "  --permission-mode default|acceptEdits|plan|dontAsk|bypassPermissions\n",
    "  --dangerously-skip-permissions   the same as --permission-mode bypassPermissions\n",
    "  --allowedTools RULES...          allowed without asking: Read \"Bash(make *)\"\n",
    "  --disallowedTools RULES...       denied; a bare tool name removes the tool\n",
    "  --tools LIST                     the tools Claude has: \"\" none, \"default\" all, \"Read,Grep\"\n",
    "  --add-dir DIRS...                more directories Claude may use\n",
    "  --system-prompt TEXT, --system-prompt-file FILE                replace the system prompt\n",
    "  --append-system-prompt TEXT, --append-system-prompt-file FILE  add to it\n",
    "  --settings FILE or JSON          settings for this session\n",
    "  --agent NAME                     the conversation as that agent (prompt, tools, model)\n",
    "  --max-turns N, --max-budget-usd AMOUNT   print mode limits\n",
    "  --verbose                        print mode: the transcript to the error stream\n",
    "  -v, --version    -h, --help\n",
    "\n",
    "Every flag is also a keyword: MODEL=haiku, OUTPUT-FORMAT=json, PRINT, CONTINUE ...\n",
    "C:Claude's own: URL=url (another endpoint), ROOT=dir (the start directory), PING (the\n",
    0
};

const char *cli_usage(int i)
{
    return i >= 0 && i < (int)(sizeof(usage) / sizeof(usage[0])) ? usage[i] : 0;
}

/* ---- applied to the REPL ---- */

static void put_list(jw *w, const char *key, const cl_strs *s, int bare_too, int *first)
{
    int i, any = 0;
    for (i = 0; i < s->n; i++) {
        if (!bare_too && !strchr(s->v[i], '('))
            continue;
        if (!any) {
            if (!*first)
                jw_raw(w, ",", 1);
            *first = 0;
            jw_strz(w, key);
            jw_rawz(w, ":[");
        } else
            jw_raw(w, ",", 1);
        any = 1;
        jw_strz(w, s->v[i]);
    }
    if (any)
        jw_raw(w, "]", 1);
}

/* the flags as a settings object (the command line's layer) */
static char *flags_json(cl_cli *c)
{
    jw w;
    int first = 1;
    const char *mode = c->skip_perms ? "bypassPermissions" : c->perm;
    jw_init(&w);
    jw_raw(&w, "{", 1);
    if (c->model[0]) {
        jw_rawz(&w, "\"model\":");
        jw_strz(&w, c->model);
        first = 0;
    }
    if (c->effort[0]) {
        jw_rawz(&w, first ? "\"effortLevel\":" : ",\"effortLevel\":");
        jw_strz(&w, c->effort);
        first = 0;
    }
    if (c->fallback[0]) {
        jw_rawz(&w, first ? "\"fallbackModel\":" : ",\"fallbackModel\":");
        jw_strz(&w, c->fallback);
        first = 0;
    }
    if (c->allow.n || c->deny.n || c->dirs.n || mode[0]) {
        int pf = 1;
        jw_rawz(&w, first ? "\"permissions\":{" : ",\"permissions\":{");
        first = 0;
        put_list(&w, "allow", &c->allow, 1, &pf);
        put_list(&w, "deny", &c->deny, 0, &pf);    /* a bare name removes the tool instead */
        put_list(&w, "additionalDirectories", &c->dirs, 1, &pf);
        if (mode[0]) {
            jw_rawz(&w, pf ? "\"defaultMode\":" : ",\"defaultMode\":");
            jw_strz(&w, mode);
        }
        jw_raw(&w, "}", 1);
    }
    jw_raw(&w, "}", 1);
    if (w.oom) {
        jw_free(&w);
        return 0;
    }
    return w.p;
}

/* a file named on the command line, relative to the start directory */
static char *read_arg_file(cl_cli *c, cl_repl *r, const char *name, const char *flag)
{
    char full[512], *b = 0;
    long n = 0;
    if (path_join(r->tools.root, name, full, sizeof(full)) || r->sys->read(r->sys->u, full, 1024L * 1024, &b, &n)) {
        cl_copy(c->err, "Error: ", sizeof(c->err));
        cl_cat(c->err, flag, sizeof(c->err));
        cl_cat(c->err, " file not found or not readable: ", sizeof(c->err));
        cl_cat(c->err, name, sizeof(c->err));
        return 0;
    }
    return b;
}

/* the tools Claude is given: --tools, then --disallowedTools' bare names, then the agent's list */
static unsigned long tool_set(const cl_cli *c, const cl_agent *a)
{
    unsigned long m = (1ul << (T_COUNT + 1)) - 1;
    int i;
    if (c->has_tools) {
        if (!c->tools[0])
            m = 0;
        else if (strcmp(c->tools, "default"))
            m = tools_mask(c->tools);
    }
    for (i = 0; i < c->deny.n; i++)
        if (!strchr(c->deny.v[i], '('))
            m &= !strcmp(c->deny.v[i], "*") ? 0 : ~tools_mask(c->deny.v[i]);
    if (a && a->tools && a->tools[0])
        m &= tools_mask(a->tools);
    return m;
}

int cli_apply(cl_cli *c, cl_repl *r)
{
    const cl_agent *a = 0;
    unsigned long m;
    int i;
    for (i = 0; i < c->dirs.n; i++) {
        char full[512];
        if (path_join(r->tools.root, c->dirs.v[i], full, sizeof(full)) || r->sys->kind(r->sys->u, full) != 2)
            return fail(c, "Error: --add-dir: not a directory: ", c->dirs.v[i], 0);
    }
    if (c->agent[0]) {
        a = tools_agent(&r->tools, c->agent);
        if (!a)
            return fail(c, "Error: no agent named ", c->agent, " (/agents lists them).");
    }
    /* the settings layer: --settings, then the flags (the flags win) */
    if (c->settings) {
        char *s;
        jv o;
        if (c->settings[0] == '{')
            s = dupn(c->settings, (long)strlen(c->settings));
        else if ((s = read_arg_file(c, r, c->settings, "--settings")) == 0)
            return -1;
        if (!s || json_parse(s, (long)strlen(s), &o) || json_type(o) != J_OBJ) {
            free(s);
            return fail(c, "Error: --settings is not a JSON object: ", c->settings, 0);
        }
        free(r->layer[0]);
        r->layer[0] = s;
    }
    free(r->layer[1]);
    r->layer[1] = flags_json(c);
    /* the system prompt: --system-prompt(-file), else the agent's; and what is appended */
    free(r->sys_replace);
    r->sys_replace = 0;
    if (c->sys_prompt)
        r->sys_replace = dupn(c->sys_prompt, (long)strlen(c->sys_prompt));
    else if (c->sys_file) {
        if ((r->sys_replace = read_arg_file(c, r, c->sys_file, "--system-prompt-file")) == 0)
            return -1;
    } else if (a && a->prompt && a->prompt[0])
        r->sys_replace = dupn(a->prompt, (long)strlen(a->prompt));
    free(r->sys_append);
    r->sys_append = 0;
    if (c->app_file || c->app_prompt) {
        /* Claude Code: the file's text first, a blank line, then the text */
        jw w;
        jw_init(&w);
        if (c->app_file) {
            char *f = read_arg_file(c, r, c->app_file, "--append-system-prompt-file");
            if (!f) {
                jw_free(&w);
                return -1;
            }
            jw_rawz(&w, f);
            free(f);
        }
        if (c->app_prompt) {
            if (w.n)
                jw_rawz(&w, "\n\n");
            jw_rawz(&w, c->app_prompt);
        }
        r->sys_append = w.p;
    }
    if (repl_load(r))
        return fail(c, "Out of memory.", 0, 0);
    /* the agent's model, unless --model says otherwise */
    if (a && !c->model[0] && a->model && a->model[0] && strcmp(a->model, "inherit"))
        cl_copy(r->model, cfg_model(a->model), sizeof(r->model));
    /* the tools */
    m = tool_set(c, a);
    r->tools.allowed = m & ((1ul << T_COUNT) - 1);
    r->tools.web_search = r->tools.web_search && (m & (1ul << T_COUNT)) != 0;
    free(r->tools.json);
    r->tools.json = 0;
    /* print mode */
    if (c->print) {
        r->no_person = 1;
        r->max_turns = c->max_turns;
        r->budget_micro = c->has_budget ? c->budget_micro : 0;
        r->budget_base = r->conv.cost_micro;
        if (c->no_persist)
            r->sess.off = 1;
    }
    return 0;
}

int cli_session(cl_cli *c, cl_repl *r)
{
    if (c->cont && repl_continue(r))
        return fail(c, "No conversation found to continue", 0, 0);
    if (c->resume) {
        if (!c->resume_name[0])
            repl_line(r, "/resume");        /* the picker */
        else if (repl_resume_session(r, c->resume_name))
            return fail(c, "No conversation found with session ID: ", c->resume_name, 0);
    }
    if (c->fork && (c->cont || c->resume) && r->conv.n)
        sess_branch(&r->sess, &r->conv, r->io->ms ? r->io->ms(r->io->u) : 0);
    if (c->name[0])
        sess_rename(&r->sess, c->name);
    return 0;
}
