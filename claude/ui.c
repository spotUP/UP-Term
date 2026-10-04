/* ui -- see ui.h. With a screen attached (u->tui, ledger A3) each call
 * goes to it (tui.c, show.c); without one, the A2 line mode below. */
#include <string.h>
#include "ui.h"
#include "tools.h"
#include "tui.h"
#include "show.h"
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
    if (u->tui)
        return;
    if (u->status) {
        u->io->write(u->io->u, "\r\033[K", 4);
        u->status = 0;
        u->col0 = 1;
    }
}

void ui_puts(cl_ui *u, const char *s)
{
    if (u->tui) {
        show_note(u->show, s);
        return;
    }
    ui_status_clear(u);
    out(u, s, (long)strlen(s));
}

void ui_line(cl_ui *u, const char *s)
{
    if (u->tui) {
        show_note(u->show, s);
        return;
    }
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
    if (u->tui) {
        tui_tick(u->tui);       /* the spinner turns; keys wait for ui_poll */
        return;
    }
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

void ui_tool(cl_ui *u, int tool, const char *name, const char *what, const char *in, long inn)
{
    if (u->tui) {
        show_tool(u->show, tool, in, inn, what);
        return;
    }
    ui_status_clear(u);
    if (!u->col0)
        out(u, "\n", 1);
    ui_puts(u, BOLD "Tool " OFF);
    ui_puts(u, name);
    ui_puts(u, "  ");
    ui_puts(u, what);
    ui_puts(u, "\n");
}

void ui_preview(cl_ui *u, int tool, const char *path, const char *before, long bn, const char *after, long an)
{
    if (u->tui)
        show_preview(u->show, tool, path, before, bn, after, an);
}

void ui_result(cl_ui *u, int tool, const char *in, long inn, int is_error, const char *text, long n)
{
    if (u->tui)
        show_result(u->show, tool, in, inn, is_error, text, n);
}

/* the file name at the end of a path */
static const char *base(const char *p)
{
    const char *b = p, *s;
    for (s = p; *s; s++)
        if (*s == '/' || *s == ':')
            b = s + 1;
    return *b ? b : p;
}

static int tui_ask(cl_ui *u, int tool, const char *what, int outside)
{
    static const char *const titles[T_COUNT] = { "Read file", "List directory", "Search files", "Write file",
                                                 "Edit file", "Run command", "Todos" };
    const char *opt[3];
    char q[400], yes2[80];
    int n, c;
    show_head(u->show);
    q[0] = 0;
    if (outside)
        cl_copy(q, "Outside the start directory. ", sizeof(q));
    if (tool == T_EDIT_FILE) {
        cl_cat(q, "Do you want to make this edit to ", sizeof(q));
        cl_cat(q, base(what), sizeof(q));
        cl_cat(q, "?", sizeof(q));
    } else if (tool == T_WRITE_FILE) {
        cl_cat(q, "Do you want to write ", sizeof(q));
        cl_cat(q, base(what), sizeof(q));
        cl_cat(q, "?", sizeof(q));
    } else if (tool == T_RUN_COMMAND) {
        cl_cat(q, "Do you want to run it?", sizeof(q));
    } else {
        cl_cat(q, "Do you want to allow this?", sizeof(q));
    }
    cl_copy(yes2, "Yes, and don't ask again this session", sizeof(yes2));
    if (perm_read_only(tool))
        cl_cat(yes2, " (all reads)", sizeof(yes2));
    opt[0] = "Yes";
    if (outside) {
        opt[1] = "No, and tell Claude what to do differently (esc)";
        n = 2;
    } else {
        opt[1] = yes2;
        opt[2] = "No, and tell Claude what to do differently (esc)";
        n = 3;
    }
    c = tui_menu(u->tui, tool >= 0 && tool < T_COUNT ? titles[tool] : "Tool", q, opt, n, 0, n - 1);
    if (c < 0)
        return ASK_NO;
    if (c == 0)
        return ASK_ONCE;
    if (c == n - 1)
        return ASK_STOP;
    return ASK_SESSION;
}

int ui_ask(cl_ui *u, int tool, const char *name, const char *what, int outside)
{
    char ans[32];
    if (u->tui)
        return tui_ask(u, tool, what, outside);
    for (;;) {
        long n;
        ui_status_clear(u);
        if (outside)
            ui_puts(u, "This is outside the start directory. ");
        ui_puts(u, BOLD "Allow " OFF);
        ui_puts(u, name);
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

void ui_busy(cl_ui *u, int on)
{
    if (u->tui)
        tui_busy(u->tui, on);
}

void ui_tokens(cl_ui *u, long n)
{
    if (u->tui)
        u->tui->tokens = n;
}

int ui_poll(cl_ui *u)
{
    int stop = u->io->brk(u->io->u);
    if (u->tui && tui_poll(u->tui))
        stop = 1;
    return stop;
}

void ui_user(cl_ui *u, const char *line)
{
    if (u->tui)
        show_user(u->show, line);
}

int ui_pick(cl_ui *u, const char *title, const char *const *opt, int n, int sel)
{
    if (!u->tui)
        return -1;
    return tui_menu(u->tui, title, "", opt, n, sel, -1);
}
