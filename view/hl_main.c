/* hl -- files in colour, the way bat and highlight show them: the
 * language from the name, the #! line or -l, line numbers on a console.
 * Not a console (a pipe, a file): the bytes as they are, as cat, unless
 * --color=always or -n says otherwise.
 *
 *   hl [-n|-N] [-p] [-l LANG] [-T N] [--list] [colour options] [FILE...]
 *
 * Portable: vw_plat_amiga.c or vw_plat_posix.c under it. */
#include <stdlib.h>
#include <string.h>
#include "hl_lex.h"
#include "hl_view.h"
#include "md.h"
#include "vw_cli.h"
#include "vw_plat.h"

static const char vers[] = "$VER: hl 1.0 (4.10.2026) UP-Term";

static void usage(void)
{
    vw_say("hl -- show files with syntax colours\n"
           "usage: hl [options] [FILE...]   (no FILE, or -: standard input)\n"
           "  -n, --number      line numbers (default on a console)\n"
           "  -N, --no-number   no line numbers\n"
           "  -p, --plain       colours only: no numbers, no file headers; a .md or .markdown\n"
           "                    file on a console is shown formatted, as mdv does\n");
    vw_say("  -l, --lang LANG   the language (else from the name or the #! line)\n"
           "  -T, --tabs N      tab width when numbering (default 8, 0 keeps tabs)\n"
           "  --list            the languages and their names\n");
    vw_say(vw_cli_help);
}

static void list_langs(void)
{
    int i;
    for (i = 0; i < hl_nlangs; i++) {
        vw_say(hl_langs[i].name);
        vw_say("\t");
        vw_say(hl_langs[i].names);
        if (hl_langs[i].exts[0]) {
            vw_say("\t.");
            vw_say(hl_langs[i].exts);
        }
        vw_say("\n");
    }
}

typedef struct opts {
    int numbers;            /* -1 auto */
    int plain;
    int tabs;
    const hl_lang *lang;
} opts;

/* hl -p is cat with colours: a Markdown file is shown formatted, as mdv
 * shows it, on a console only (a pipe or a file gets the bytes) */
static int is_markdown(const char *name)
{
    size_t n = strlen(name), e;
    static const char *const ext[2] = { ".md", ".markdown" };
    int i;
    for (i = 0; i < 2; i++) {
        e = strlen(ext[i]);
        if (n > e) {
            size_t k;
            for (k = 0; k < e; k++) {
                char ch = name[n - e + k];
                if (ch >= 'A' && ch <= 'Z')
                    ch = (char)(ch + 32);
                if (ch != ext[i][k])
                    break;
            }
            if (k == e)
                return 1;
        }
    }
    return 0;
}

/* the document read whole and drawn by md_render: 0, or -1 (read or memory) */
static int show_markdown(vw_file *f, vw_out *out)
{
    long n = 0;
    char *doc = vw_slurp(f, &n);
    md_opts mo;
    int r;
    if (!doc)
        return -1;
    mo.width = vw_wrap_width(0);
    mo.urls = 1;
    out->osc8 = vw_osc8_ok(out);
    r = md_render(doc, n, &mo, out);
    free(doc);
    return r;
}

/* the whole stream as it is: hl as cat */
static int copy(vw_file *f)
{
    char buf[4096];
    long k;
    while ((k = vw_read(f, buf, sizeof(buf))) > 0) {
        vw_write(0, buf, k);
        if (vw_break())
            return -2;
    }
    return k < 0 ? -1 : 0;
}

static void header(vw_out *o, const char *name)
{
    vo_class(o, HL_META);
    vo_rawz(o, o->cs == VW_UTF8 ? "\342\224\200\342\224\200 " : "-- ");
    vo_textz(o, name);
    vo_reset(o);
    vo_raw(o, "\n", 1);
}

typedef struct lbuf {
    char *p;
    long n, cap;
} lbuf;

static int lb_add(lbuf *b, const char *s, long n)
{
    if (b->n + n + 1 > b->cap) {
        long nc = (b->n + n + 1) * 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np)
            return -1;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    return 0;
}

/* one stream through the lexer: 0, -1 a read error, -2 ^C */
static int show(vw_file *f, const char *name, const opts *op, int numbers, vw_out *o)
{
    hl_view v;
    static char buf[8192];
    lbuf big;               /* a line that does not fit in buf, or spans reads */
    long have = 0, k;
    int started = 0, rc = 0;
    big.p = 0;
    big.n = big.cap = 0;
    for (;;) {
        long start = 0;
        k = vw_read(f, buf + have, (long)sizeof(buf) - have);
        if (k < 0)
            rc = -1;
        if (k <= 0)
            break;
        have += k;
        /* whole lines are lexed where they lie in buf */
        for (;;) {
            char *nl = (char *)memchr(buf + start, '\n', have - start);
            const char *ln = buf + start;
            long len;
            if (!nl)
                break;
            len = (long)(nl - ln);
            if (big.n) {
                if (lb_add(&big, ln, len) < 0) {
                    rc = -1;
                    goto done;
                }
                ln = big.p;
                len = big.n;
            }
            if (!started) {
                hl_view_begin(&v, o, op->lang ? op->lang : hl_detect(name, ln, len), numbers, numbers ? op->tabs : 0);
                started = 1;
            }
            hl_view_line(&v, ln, len, 1);
            big.n = 0;
            start = (long)(nl + 1 - buf);
            if (vw_break()) {
                rc = -2;
                goto done;
            }
        }
        /* an unfinished line: to the front, or to big when buf is full of it */
        if (start < have) {
            if (start > 0) {
                memmove(buf, buf + start, have - start);
                have -= start;
            } else if (have == (long)sizeof(buf)) {
                if (lb_add(&big, buf, have) < 0) {
                    rc = -1;
                    goto done;
                }
                have = 0;
            }
        } else {
            have = 0;
        }
    }
    if (have > 0 || big.n > 0) {
        const char *ln = buf;
        long len = have;
        if (big.n) {
            if (lb_add(&big, buf, have) < 0) {
                rc = -1;
                goto done;
            }
            ln = big.p;
            len = big.n;
        }
        if (!started)
            hl_view_begin(&v, o, op->lang ? op->lang : hl_detect(name, ln, len), numbers, numbers ? op->tabs : 0);
        hl_view_line(&v, ln, len, 0);
    }
done:
    vo_reset(o);
    vo_flush(o);
    free(big.p);
    return rc;
}

int main(int argc, char **argv)
{
    opts op;
    vw_cli cli;
    vw_out out;
    int i = 1, nfiles = 0, tty, numbers, rc = 0, r;
    char **files;
    (void)vers;
    op.numbers = -1;
    op.plain = 0;
    op.tabs = 8;
    op.lang = 0;
    vw_cli_init(&cli);
    files = (char **)malloc(sizeof(char *) * (argc + 1));
    if (!files)
        return vw_fail_code;
    while (i < argc) {
        const char *a = argv[i];
        r = vw_cli_option(&cli, argc, argv, &i);
        if (r < 0)
            return vw_fail_code;
        if (r > 0)
            continue;
        if (!strcmp(a, "-n") || !strcmp(a, "--number")) {
            op.numbers = 1;
        } else if (!strcmp(a, "-N") || !strcmp(a, "--no-number")) {
            op.numbers = 0;
        } else if (!strcmp(a, "-p") || !strcmp(a, "--plain")) {
            op.plain = 1;
        } else if (!strcmp(a, "-l") || !strcmp(a, "--lang") || !strncmp(a, "--lang=", 7)) {
            const char *v = a[1] == 'l' ? (i + 1 < argc ? argv[++i] : "") : a[6] == '=' ? a + 7
                            : (i + 1 < argc ? argv[++i] : "");
            op.lang = hl_find(v, (long)strlen(v));
            if (!op.lang && strcmp(v, "plain") && strcmp(v, "text")) {
                vw_say("hl: no language ");
                vw_say(v);
                vw_say(" (hl --list names them)\n");
                return vw_fail_code;
            }
        } else if (!strcmp(a, "-T") || !strcmp(a, "--tabs")) {
            long t = vw_number(i + 1 < argc ? argv[++i] : "");
            if (t < 0 || t > 32) {
                vw_say("hl: --tabs takes 0 to 32\n");
                return vw_fail_code;
            }
            op.tabs = (int)t;
        } else if (!strcmp(a, "--list")) {
            list_langs();
            return 0;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help") || !strcmp(a, "?")) {
            usage();
            return 0;
        } else if (a[0] == '-' && a[1]) {
            vw_say("hl: unknown option ");
            vw_say(a);
            vw_say("\n");
            usage();
            return vw_fail_code;
        } else {
            files[nfiles++] = argv[i];
        }
        i++;
    }
    tty = vw_out_is_tty();
    numbers = op.plain ? 0 : op.numbers >= 0 ? op.numbers : tty;
    if (vw_cli_start(&cli, tty, &out) < 0)
        return vw_fail_code;
    if (nfiles == 0)
        files[nfiles++] = (char *)"-";
    for (i = 0; i < nfiles; i++) {
        vw_file *f = vw_open(files[i]);
        if (!f) {
            vw_say("hl: cannot open ");
            vw_say(files[i]);
            vw_say("\n");
            rc = vw_fail_code;
            continue;
        }
        if (op.plain && out.depth && is_markdown(files[i])) {
            r = show_markdown(f, &out);
        } else if (!out.depth && !numbers) {
            r = copy(f);
        } else {
            if (nfiles > 1 && numbers)
                header(&out, files[i]);
            r = show(f, strcmp(files[i], "-") ? files[i] : 0, &op, numbers, &out);
        }
        vw_close(f);
        if (r == -2) {
            vw_say("***Break\n");
            rc = vw_fail_code;
            break;
        }
        if (r < 0) {
            vw_say("hl: error reading ");
            vw_say(files[i]);
            vw_say("\n");
            rc = vw_fail_code;
        }
    }
    vo_flush(&out);
    hl_cleanup();
    free(files);
    if (vw_write_failed())
        rc = vw_fail_code;
    return rc;
}
