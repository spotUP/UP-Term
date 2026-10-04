/* mdv -- Markdown rendered in the window, the way glow shows it: headings,
 * emphasis, lists, tables fitted to the width, code in colour, links as
 * OSC 8 hyperlinks with the URL shown. Not a console: the same layout
 * without colours (80 columns unless -w says).
 *
 *   mdv [-w COLS] [-L] [-U] [colour options] [FILE...]
 *
 * Portable: vw_plat_amiga.c or vw_plat_posix.c under it. */
#include <stdlib.h>
#include <string.h>
#include "md.h"
#include "hl_lex.h"
#include "vw_cli.h"
#include "vw_plat.h"

static const char vers[] = "$VER: mdv 1.0 (4.10.2026) UP-Term";

static void usage(void)
{
    vw_say("mdv -- show Markdown formatted\n"
           "usage: mdv [options] [FILE...]   (no FILE, or -: standard input)\n"
           "  -w, --width N     wrap at N columns (default: the window's width)\n"
           "  -L, --no-osc8     no OSC 8 hyperlinks\n"
           "  -U, --no-urls     do not show a link's URL after its text\n");
    vw_say(vw_cli_help);
}

/* the whole stream in memory: 0 when it could not be read */
static char *slurp(vw_file *f, long *len)
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

int main(int argc, char **argv)
{
    vw_cli cli;
    vw_out out;
    md_opts mo;
    int i = 1, nfiles = 0, tty, rc = 0, r, osc8 = 1;
    long width = 0;
    char **files, v[16];
    (void)vers;
    mo.urls = 1;
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
        if (!strcmp(a, "-w") || !strcmp(a, "--width")) {
            width = vw_number(i + 1 < argc ? argv[++i] : "");
            if (width < 10 || width > 1000) {
                vw_say("mdv: --width takes 10 to 1000\n");
                return vw_fail_code;
            }
        } else if (!strcmp(a, "-L") || !strcmp(a, "--no-osc8")) {
            osc8 = 0;
        } else if (!strcmp(a, "-U") || !strcmp(a, "--no-urls")) {
            mo.urls = 0;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help") || !strcmp(a, "?")) {
            usage();
            return 0;
        } else if (a[0] == '-' && a[1]) {
            vw_say("mdv: unknown option ");
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
    if (!width && tty)
        width = vw_columns();
    if (!width && vw_env("COLUMNS", v, sizeof(v)))
        width = vw_number(v);
    if (width < 10)
        width = 80;
    /* the last column stays free: a full line would wrap twice on some consoles */
    mo.width = (int)(tty ? width - 1 : width);
    if (vw_cli_start(&cli, tty, &out) < 0)
        return vw_fail_code;
    out.osc8 = osc8 && out.depth > 0 && out.cs == VW_UTF8;
    if (nfiles == 0)
        files[nfiles++] = (char *)"-";
    for (i = 0; i < nfiles; i++) {
        vw_file *f = vw_open(files[i]);
        char *doc;
        long len = 0;
        if (!f) {
            vw_say("mdv: cannot open ");
            vw_say(files[i]);
            vw_say("\n");
            rc = vw_fail_code;
            continue;
        }
        doc = slurp(f, &len);
        vw_close(f);
        if (!doc) {
            vw_say("mdv: cannot read ");
            vw_say(files[i]);
            vw_say("\n");
            rc = vw_fail_code;
            continue;
        }
        if (i > 0)
            vo_raw(&out, "\n", 1);
        if (md_render(doc, len, &mo, &out) < 0) {
            vw_say("mdv: out of memory\n");
            rc = vw_fail_code;
        }
        vo_flush(&out);
        free(doc);
    }
    hl_cleanup();
    free(files);
    if (vw_write_failed())
        rc = vw_fail_code;
    return rc;
}
