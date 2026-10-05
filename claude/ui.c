/* ui -- see ui.h. With a screen attached (u->tui, ledger A3) each call
 * goes to it (tui.c, show.c); without one, the A2 line mode below. */
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "tools.h"
#include "tui.h"
#include "show.h"
#include "util.h"

static void rewind_cb(void *u)
{
    ui_rewind((cl_ui *)u);
}

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
    const char *opt[3];
    char q[400], yes2[80];
    int n, c;
    show_head(u->show);
    q[0] = 0;
    if (outside)
        cl_copy(q, "Outside the start directory. ", sizeof(q));
    if (tool == T_EDIT || tool == T_MULTIEDIT) {
        cl_cat(q, "Do you want to make this edit to ", sizeof(q));
        cl_cat(q, base(what), sizeof(q));
        cl_cat(q, "?", sizeof(q));
    } else if (tool == T_WRITE) {
        cl_cat(q, "Do you want to write ", sizeof(q));
        cl_cat(q, base(what), sizeof(q));
        cl_cat(q, "?", sizeof(q));
    } else if (tool == T_BASH) {
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
    {
        /* the user may be elsewhere: the bell, a notice, the title */
        char note[120];
        cl_copy(note, "Claude needs your permission to use ", sizeof(note));
        cl_cat(note, show_name(tool), sizeof(note));
        tui_title(u->tui, "Claude - needs your permission");
        tui_notify(u->tui, note);
    }
    c = tui_menu(u->tui, tools_title(tool), q, opt, n, 0, n - 1);
    tui_title(u->tui, u->tui->busy ? "Claude - working" : "Claude");
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

/* ---- A4 (WP1) ---- */

void ui_attach(cl_ui *u, cl_sys *sys, const char *root, struct cl_conv *conv)
{
    cl_tui *t = u->tui;
    const char *v;
    u->sys = sys;
    u->root = root;
    u->conv = conv;
    if (!u->rw.count)
        input_rewind_stub(u);
    if (!t)
        return;
    t->sys = sys;
    t->project = root;
    t->histfile = u->histfile;
    hist_load(&t->hist, sys, u->histfile);
    hist_fill(&t->hist, &t->ed, root ? root : "");
    t->complete = input_complete;
    t->cu = u;
    t->on_rewind = rewind_cb;
    t->ru = u;
    v = u->setting ? u->setting(u->su, "theme") : 0;
    if (v)
        t->th = theme_get(v);
    v = u->setting ? u->setting(u->su, "editorMode") : 0;
    if (v && !strcmp(v, "vim"))
        ed_set_vim(&t->ed, 1);
    t->full = 1;
    tui_frame(t);
}

void ui_thinking(cl_ui *u, const char *s, long n)
{
    if (u->tui)
        show_think(u->show, s, n);
}

int ui_take_queued(cl_ui *u, jw *out)
{
    char *q;
    if (!u->tui || !(q = tui_dequeue(u->tui, 1)))
        return 0;
    ui_user(u, q);
    jw_rawz(out, q);
    free(q);
    return 1;
}

/* Esc Esc: pick a prompt of this conversation, then what goes back */
void ui_rewind(cl_ui *u)
{
    char lab[9][80], q[120];
    const char *opt[9];
    const char *what[6];
    int whatv[6];
    int n, first, k, i, c, can, nw = 0;
    if (!u->tui || !u->rw.count)
        return;
    n = u->rw.count(u->rw.u);
    if (n <= 0) {
        ui_line(u, "Nothing to rewind to yet.");
        return;
    }
    first = n > 9 ? n - 9 : 0;
    for (i = first, k = 0; i < n; i++, k++) {
        char full[400];
        int w = 0;
        long l;
        full[0] = 0;
        u->rw.label(u->rw.u, i, full, sizeof(full));
        /* one line: the first one, cut to the menu's room */
        for (l = 0; full[l] && full[l] != '\n'; l++)
            ;
        full[l] = 0;
        for (l = 0; full[l] && w < u->tui->cols - 14 && l < (long)sizeof(lab[k]) - 4; l++, w++)
            lab[k][l] = full[l];
        lab[k][l] = 0;
        if (full[l])
            cl_cat(lab[k], "...", sizeof(lab[k]));
        opt[k] = lab[k];
    }
    c = tui_menu(u->tui, "Rewind", "Restore the conversation and/or the code to the point before...", opt, k,
                 k - 1, -1);
    if (c < 0)
        return;
    i = first + c;
    can = u->rw.can ? u->rw.can(u->rw.u, i) : RW_CONV;
    if ((can & RW_CODE) && (can & RW_CONV)) {
        what[nw] = "Restore code and conversation";
        whatv[nw++] = RW_CODE | RW_CONV;
    }
    if (can & RW_CONV) {
        what[nw] = "Restore conversation";
        whatv[nw++] = RW_CONV;
    }
    if (can & RW_CODE) {
        what[nw] = "Restore code";
        whatv[nw++] = RW_CODE;
    }
    if (can & RW_CONV) {
        /* Claude Code's summaries: the rest of it, or what came before */
        what[nw] = "Summarize from here";
        whatv[nw++] = RW_SUM;
        what[nw] = "Summarize up to here";
        whatv[nw++] = RW_SUM_UP;
    }
    what[nw] = "Never mind";
    whatv[nw++] = 0;
    cl_copy(q, "Back to before: ", sizeof(q));
    cl_cat(q, lab[c], sizeof(q));
    c = tui_menu(u->tui, "Rewind", q, what, nw, 0, nw - 1);
    if (c < 0 || !whatv[c])
        return;
    {
        char *full = (char *)malloc(8192);
        if (!full)
            return;
        full[0] = 0;
        u->rw.label(u->rw.u, i, full, 8192);
        if (u->rw.restore(u->rw.u, i, whatv[c])) {
            free(full);
            ui_line(u, "Could not rewind.");
            return;
        }
        if (whatv[c] & (RW_SUM | RW_SUM_UP))
            ;                       /* the summary said so itself */
        else if (whatv[c] & RW_CONV) {
            /* the prompt comes back into the box, to send again or change */
            tui_set_text(u->tui, full);
            ui_line(u, (whatv[c] & RW_CODE) ? "Rewound the conversation and the code." : "Rewound the conversation.");
        } else {
            ui_line(u, "Rewound the code.");
        }
        free(full);
    }
}

void ui_server(cl_ui *u, int call, const char *block, long bn, const char *input, long inn)
{
    char line[300];
    jw b;
    /* a server tool's call: its block from the start with the input that
     * streamed in (the start's input is empty) */
    jw_init(&b);
    if (call && input && inn) {
        jw_rawz(&b, "{\"type\":\"server_tool_use\",\"input\":");
        jw_raw(&b, input, inn);
        jw_raw(&b, "}", 1);
    } else
        jw_raw(&b, block, bn);
    if (b.oom || tools_server_line(b.p, b.n, line, sizeof(line))) {
        jw_free(&b);
        return;
    }
    jw_free(&b);
    if (u->tui) {
        show_server(u->show, call, line);
        return;
    }
    ui_status_clear(u);
    if (!u->col0)
        out(u, "\n", 1);
    ui_puts(u, call ? BOLD "Tool " OFF : "  ");
    ui_puts(u, line);
    ui_puts(u, "\n");
}

/* the line mode's question: the options numbered, an answer typed */
static int line_choose(cl_ui *u, const char *header, const char *question, const char *const *labels,
                       const char *const *descs, int n, int flags, unsigned *picked, char *other, long cap)
{
    char ans[400], num[16];
    int i;
    ui_status_clear(u);
    if (!u->col0)
        out(u, "\n", 1);
    if (header && *header) {
        ui_puts(u, BOLD);
        ui_puts(u, header);
        ui_puts(u, OFF "  ");
    }
    ui_puts(u, question);
    ui_puts(u, "\n");
    for (i = 0; i < n; i++) {
        cl_ltoa(i + 1, num);
        ui_puts(u, "  ");
        ui_puts(u, num);
        ui_puts(u, ". ");
        ui_puts(u, labels[i] ? labels[i] : "");
        if (descs && descs[i] && *descs[i]) {
            ui_puts(u, DIM " - ");
            ui_puts(u, descs[i]);
            ui_puts(u, OFF);
        }
        ui_puts(u, "\n");
    }
    for (;;) {
        long k;
        const char *p;
        ui_puts(u, flags & CH_MULTI ? "Numbers (1,3), or your own answer: "
                                    : flags & CH_OTHER ? "A number, or your own answer: " : "A number: ");
        k = u->io->read_line(u->io->u, ans, sizeof(ans));
        u->col0 = 1;
        if (k < 0)
            return -1;
        if (!k)
            continue;
        /* numbers? */
        *picked = 0;
        for (p = ans; *p; p++) {
            int v;
            if (*p == ',' || *p == ' ')
                continue;
            if (*p < '1' || *p > '9')
                break;
            v = *p - '0';
            if (v > n)
                break;
            *picked |= 1u << (v - 1);
        }
        if (!*p && *picked) {
            if (flags & CH_MULTI)
                return 0;
            for (i = 0; i < n; i++)
                if (*picked & (1u << i))
                    return i;
        }
        if (flags & CH_OTHER) {
            *picked = 0;
            cl_copy(other, ans, cap);
            return n;
        }
    }
}

int ui_choose(cl_ui *u, const char *header, const char *question, const char *const *labels,
              const char *const *descs, int n, int flags, unsigned *picked, char *other, long cap)
{
    const char *opt[8];
    char *text[8];
    int i, k, c, sel = 0;
    *picked = 0;
    if (other && cap)
        other[0] = 0;
    if (n > 6)
        n = 6;
    if (!u->tui)
        return line_choose(u, header, question, labels, descs, n, flags, picked, other, cap);
    /* the screen: the menu; for several answers, the options ticked one by
     * one until "Done"; "Type something else" reads a line in the box */
    for (;;) {
        for (i = 0; i < n; i++) {
            long l = (long)strlen(labels[i] ? labels[i] : "") + (descs && descs[i] ? (long)strlen(descs[i]) : 0) + 12;
            text[i] = (char *)malloc((size_t)l);
            if (!text[i]) {
                for (k = 0; k < i; k++)
                    free(text[k]);
                return -1;
            }
            text[i][0] = 0;
            if (flags & CH_MULTI)
                cl_copy(text[i], *picked & (1u << i) ? "[x] " : "[ ] ", l);
            cl_cat(text[i], labels[i] ? labels[i] : "", l);
            if (descs && descs[i] && *descs[i]) {
                cl_cat(text[i], " - ", l);
                cl_cat(text[i], descs[i], l);
            }
            opt[i] = text[i];
        }
        k = n;
        if (flags & CH_OTHER)
            opt[k++] = "Type something else";
        if (flags & CH_MULTI)
            opt[k++] = "Done";
        c = tui_menu(u->tui, header && *header ? header : "Question", question, opt, k, sel, -1);
        for (i = 0; i < n; i++)
            free(text[i]);
        if (c < 0 || c >= k)
            return -1;
        if (c < n) {
            if (!(flags & CH_MULTI))
                return c;
            *picked ^= 1u << c;
            sel = c;
            continue;
        }
        if ((flags & CH_OTHER) && c == n) {
            long l;
            show_note(u->show, "Type your answer and press Enter:");
            l = tui_read(u->tui, other, cap);
            if (l < 0)
                return -1;
            if (!l)
                continue;
            return flags & CH_MULTI ? 0 : n;
        }
        return 0;                   /* Done */
    }
}
