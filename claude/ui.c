/* ui -- see ui.h. */
#include <string.h>
#include "ui.h"
#include "tools.h"
#include "util.h"

#define BOLD "\033[1m"
#define DIM  "\033[2m"
#define OFF  "\033[0m"

void ui_init(cl_ui *u, cl_io *io)
{
    memset(u, 0, sizeof(*u));
    u->io = io;
    u->col0 = 1;
}

static void out(cl_ui *u, const char *s, long n)
{
    if (n > 0) {
        u->io->write(u->io->u, s, n);
        u->col0 = s[n - 1] == '\n';
    }
}

void ui_status_clear(cl_ui *u)
{
    if (u->status) {
        u->io->write(u->io->u, "\r\033[K", 4);
        u->status = 0;
        u->col0 = 1;
    }
}

void ui_puts(cl_ui *u, const char *s)
{
    ui_status_clear(u);
    out(u, s, (long)strlen(s));
}

void ui_line(cl_ui *u, const char *s)
{
    ui_status_clear(u);
    if (!u->col0)
        out(u, "\n", 1);
    out(u, s, (long)strlen(s));
    out(u, "\n", 1);
}

void ui_status(cl_ui *u, const char *what)
{
    static const char spin[] = "|/-\\";
    char line[128], sp[3];
    if (!u->status && !u->col0)
        out(u, "\n", 1);
    sp[0] = spin[u->frame++ & 3];
    sp[1] = ' ';
    sp[2] = 0;
    cl_copy(line, "\r" DIM, sizeof(line));
    cl_cat(line, sp, sizeof(line));
    cl_cat(line, what, sizeof(line));
    cl_cat(line, OFF "\033[K", sizeof(line));
    u->io->write(u->io->u, line, (long)strlen(line));
    u->status = 1;
}

static void plain_text(void *p, const char *s, long n)
{
    cl_ui *u = (cl_ui *)p;
    long i = 0, run = 0;
    ui_status_clear(u);
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 && c != '\n' && c != '\t') {
            out(u, s + i - run, run);
            run = 0;
            continue;
        }
        if (c == 0x7f) {
            out(u, s + i - run, run);
            run = 0;
            continue;
        }
        run++;
    }
    out(u, s + n - run, run);
}

static void plain_end(void *p)
{
    cl_ui *u = (cl_ui *)p;
    ui_status_clear(u);
    if (!u->col0)
        out(u, "\n", 1);
}

void ui_plain(cl_ui *u, cl_render *r)
{
    r->u = u;
    r->text = plain_text;
    r->end = plain_end;
}

void ui_tool(cl_ui *u, const char *tool, const char *what)
{
    ui_status_clear(u);
    if (!u->col0)
        out(u, "\n", 1);
    ui_puts(u, BOLD "Tool " OFF);
    ui_puts(u, tool);
    ui_puts(u, "  ");
    ui_puts(u, what);
    ui_puts(u, "\n");
}

int ui_ask(cl_ui *u, const char *tool, const char *what, int outside)
{
    char ans[32];
    (void)what;
    for (;;) {
        long n;
        ui_status_clear(u);
        if (outside)
            ui_puts(u, "This is outside the start directory. ");
        ui_puts(u, BOLD "Allow " OFF);
        ui_puts(u, tool);
        ui_puts(u, outside ? "? Yes once (y), No (n): " : "? Yes once (y), Always this session (a), No (n): ");
        n = u->io->read_line(u->io->u, ans, sizeof(ans));
        u->col0 = 1;
        if (n < 0)
            return ASK_NO;
        if (n == 0)
            continue;
        if (ans[0] == 'y' || ans[0] == 'Y')
            return ASK_ONCE;
        if ((ans[0] == 'a' || ans[0] == 'A') && !outside)
            return ASK_SESSION;
        if (ans[0] == 'n' || ans[0] == 'N')
            return ASK_NO;
    }
}
