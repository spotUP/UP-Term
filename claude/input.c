/* input -- what a line typed at the prompt can be besides a prompt (A4
 * WP1), as Claude Code reads it:
 *   !cmd     bash mode (1.3): the command runs (cl_sys run: vsh, or the
 *            AmigaDOS shell), its output is shown under the corner and
 *            sent to Claude as <bash-input>/<bash-stdout>/<bash-stderr>,
 *            and Claude answers it (respondToBashCommands' default);
 *   #note    the memory shortcut (1.4): which memory file, then "- note"
 *            appended to it;
 *   @path    a mention (1.2): the file's text (64 KB at most) or the
 *            directory's listing goes with the prompt; Tab completes
 *            the path in the box (input_complete);
 *   /theme   the colours (1.10), /vim vim mode (1.8).
 * And the rewind menu's stub source over the conversation (1.7) until
 * WP3's checkpoints replace it. Portable C89 over sys.h and ui.h,
 * host-tested (tests/test_claude_tui.c, tests/test_claude_repl.c). */
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "tui.h"
#include "show.h"
#include "tools.h"
#include "ext.h"
#include "path.h"
#include "conv.h"
#include "util.h"

#define BASH_OUT   32768L       /* a ! command's output kept at most */
#define MENTION_MAX 65536L      /* one @file's text at most */
#define MENTIONS_MAX 262144L    /* all of a prompt's */

static const char *skip_blanks(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

/* ---- ! bash mode ---- */

static int bash(cl_ui *u, const char *cmd, jw *out)
{
    char *buf;
    long n = 0, rc = 0;
    int r;
    cmd = skip_blanks(cmd);
    if (!*cmd)
        return IN_DONE;
    buf = (char *)malloc((size_t)BASH_OUT + 32);
    if (!buf) {
        ui_line(u, "Out of memory.");
        return IN_DONE;
    }
    if (u->show)
        u->show->tool = -1;     /* the result goes under the "! cmd" line, no header */
    ui_status(u, "Running");
    if (u->tools && u->tools->wait) {
        /* as Bash runs: Esc stops it, Ctrl+B leaves it running */
        char id[24];
        ui_busy(u, 1);
        r = tools_run_fg(u->tools, cmd, 120, buf, BASH_OUT, &n, &rc, id, sizeof(id));
        ui_busy(u, 0);
        if (r == SHELL_MOVED) {
            char m[120];
            free(buf);
            cl_copy(m, "Moved to the background as ", sizeof(m));
            cl_cat(m, id, sizeof(m));
            cl_cat(m, ".", sizeof(m));
            ui_line(u, m);
            jw_rawz(out, "<bash-input>");
            jw_rawz(out, cmd);
            jw_rawz(out, "</bash-input>\n<bash-stdout>The command was moved to the background with ID: ");
            jw_rawz(out, id);
            jw_rawz(out, " (you are told when it ends; TaskStop stops it).</bash-stdout><bash-stderr></bash-stderr>");
            return IN_SEND;
        }
    } else {
        r = u->sys->run(u->sys->u, cmd, 120, buf, BASH_OUT, &n, &rc);
    }
    ui_status_clear(u);
    if (r == -1 || r == SYS_BREAK || r == SYS_TIMEOUT) {
        const char *why = r == SYS_BREAK ? "Stopped." : r == SYS_TIMEOUT ? "Timed out after 120 seconds." : 0;
        if (!why) {
            char m[300];
            cl_copy(m, "Could not run it: ", sizeof(m));
            cl_cat(m, u->sys->err(u->sys->u), sizeof(m));
            ui_line(u, m);
        } else {
            ui_line(u, why);
        }
        free(buf);
        return IN_DONE;
    }
    buf[n] = 0;
    {
        /* the screen: show.c's command result ("Return code N.\n" + output) */
        jw t;
        char num[16];
        jw_init(&t);
        jw_rawz(&t, "Return code ");
        cl_ltoa(rc, num);
        jw_rawz(&t, num);
        jw_rawz(&t, ".\n");
        jw_raw(&t, buf, n);
        if (u->tui)
            ui_result(u, T_BASH, "", 0, 0, t.p, t.n);
        else
            ui_line(u, buf);
        jw_free(&t);
    }
    /* Claude Code's form of a ! command in the conversation */
    jw_rawz(out, "<bash-input>");
    jw_rawz(out, cmd);
    jw_rawz(out, "</bash-input>\n<bash-stdout>");
    jw_raw(out, buf, n);
    jw_rawz(out, "</bash-stdout><bash-stderr>");
    if (rc) {
        char num[16];
        jw_rawz(out, "Return code ");
        cl_ltoa(rc, num);
        jw_rawz(out, num);
    }
    jw_rawz(out, "</bash-stderr>");
    free(buf);
    return IN_SEND;
}

/* ---- # memory ---- */

static int stub_memory(cl_ui *u, cl_memfile *f, int max)
{
    char a[256], c[256];
    if (max < 2)
        return 0;
    /* the project's: CLAUDE.md, or AMIGA.md where only that exists (A3's /init) */
    if (path_join(u->root ? u->root : "", "CLAUDE.md", c, sizeof(c)) ||
        path_join(u->root ? u->root : "", "AMIGA.md", a, sizeof(a)))
        return 0;
    cl_copy(f[0].label, "Project memory", sizeof(f[0].label));
    cl_copy(f[0].path, u->sys->kind(u->sys->u, c) != 1 && u->sys->kind(u->sys->u, a) == 1 ? a : c, sizeof(f[0].path));
    cl_copy(f[1].label, "User memory", sizeof(f[1].label));
    cl_copy(f[1].path, "ENVARC:Claude/CLAUDE.md", sizeof(f[1].path));
    return 2;
}

static int memory(cl_ui *u, const char *note)
{
    cl_memfile f[6];
    char opt[6][330], m[400];
    const char *op[6];
    int n, i, c = 0;
    char *old = 0;
    long on = 0;
    jw w;
    note = skip_blanks(note);
    if (!*note)
        return IN_DONE;
    n = u->memory_files ? u->memory_files(u->mu, f, 6) : stub_memory(u, f, 6);
    if (n <= 0) {
        ui_line(u, "No memory file to save to.");
        return IN_DONE;
    }
    for (i = 0; i < n; i++) {
        cl_copy(opt[i], f[i].label, sizeof(opt[i]));
        cl_cat(opt[i], "  ", sizeof(opt[i]));
        cl_cat(opt[i], f[i].path, sizeof(opt[i]));
        op[i] = opt[i];
    }
    if (u->tui) {
        c = tui_menu(u->tui, "Where should this memory be saved?", "", op, n, 0, -1);
        if (c < 0) {
            ui_line(u, "Not saved.");
            return IN_DONE;
        }
    }
    if (u->sys->kind(u->sys->u, f[c].path) == 1 && u->sys->read(u->sys->u, f[c].path, 1024L * 1024, &old, &on)) {
        ui_line(u, "Cannot read the memory file; nothing was saved.");
        return IN_DONE;
    }
    jw_init(&w);
    if (old)
        jw_raw(&w, old, on);
    if (on && old[on - 1] != '\n')
        jw_raw(&w, "\n", 1);
    jw_rawz(&w, "- ");
    jw_rawz(&w, note);
    jw_raw(&w, "\n", 1);
    free(old);
    if (w.oom || u->sys->write(u->sys->u, f[c].path, w.p, w.n)) {
        cl_copy(m, "Cannot save to ", sizeof(m));
        cl_cat(m, f[c].path, sizeof(m));
        cl_cat(m, ": ", sizeof(m));
        cl_cat(m, u->sys->err(u->sys->u), sizeof(m));
        ui_line(u, m);
    } else {
        cl_copy(m, "Saved to ", sizeof(m));
        cl_cat(m, f[c].label, sizeof(m));
        cl_cat(m, " (", sizeof(m));
        cl_cat(m, f[c].path, sizeof(m));
        cl_cat(m, ").", sizeof(m));
        ui_line(u, m);
        if (u->memory_changed)
            u->memory_changed(u->mu, f[c].path);
    }
    jw_free(&w);
    return IN_DONE;
}

/* ---- /theme, /vim ---- */

static int theme_cmd(cl_ui *u, const char *arg)
{
    const cl_theme *th = 0;
    char m[120];
    int i;
    arg = skip_blanks(arg);
    if (!u->tui) {
        ui_line(u, "Themes are for the screen mode.");
        return IN_DONE;
    }
    if (*arg) {
        for (i = 0; i < THEME_COUNT; i++)
            if (!strcmp(cl_themes[i].name, arg))
                th = &cl_themes[i];
        if (!th) {
            cl_copy(m, "The themes:", sizeof(m));
            for (i = 0; i < THEME_COUNT; i++) {
                cl_cat(m, " ", sizeof(m));
                cl_cat(m, cl_themes[i].name, sizeof(m));
            }
            ui_line(u, m);
            return IN_DONE;
        }
    } else {
        const char *op[THEME_COUNT];
        int c;
        for (i = 0; i < THEME_COUNT; i++)
            op[i] = cl_themes[i].label;
        u->tui->m_ctx = KC_THEME;   /* ThemePicker's bindings first */
        c = tui_menu(u->tui, "Theme", "Choose the text style that looks best with your terminal", op, THEME_COUNT,
                     theme_index(u->tui->th), -1);
        if (c < 0)
            return IN_DONE;
        th = &cl_themes[c];
    }
    u->tui->th = th;
    u->tui->full = 1;
    if (u->set_setting)
        u->set_setting(u->su, "theme", th->name);
    cl_copy(m, "Theme: ", sizeof(m));
    cl_cat(m, th->label, sizeof(m));
    ui_line(u, m);
    return IN_DONE;
}

static int vim_cmd(cl_ui *u)
{
    int on;
    if (!u->tui) {
        ui_line(u, "Vim mode is for the screen mode.");
        return IN_DONE;
    }
    on = u->tui->ed.vim == VIM_OFF;
    ed_set_vim(&u->tui->ed, on);
    if (u->set_setting)
        u->set_setting(u->su, "editorMode", on ? "vim" : "normal");
    ui_line(u, on ? "Vim mode on: Esc for NORMAL mode, i for INSERT. /vim again turns it off."
                  : "Vim mode off.");
    return IN_DONE;
}

/* ---- @ mentions ---- */

typedef struct lister {
    jw *w;
    int n;
} lister;

static int list_one(void *c, const cl_dirent *e)
{
    lister *l = (lister *)c;
    if (l->n >= 200)
        return 1;
    jw_rawz(l->w, e->name);
    if (e->dir)
        jw_raw(l->w, "/", 1);
    jw_raw(l->w, "\n", 1);
    l->n++;
    return 0;
}

static long count_lines(const char *p, long n)
{
    long i, c = 0;
    for (i = 0; i < n; i++)
        c += p[i] == '\n';
    return c + (n && p[n - 1] != '\n');
}

/* one mention: attached to out, a note on the screen; 1 when it took */
static int mention(cl_ui *u, const char *p, long pn, jw *att, long *total)
{
    char rel[256], full[512], m[600], num[16];
    int kind;
    if (pn <= 0 || pn >= (long)sizeof(rel))
        return 0;
    memcpy(rel, p, (size_t)pn);
    rel[pn] = 0;
    if (path_join(u->root ? u->root : "", rel, full, sizeof(full)))
        return 0;
    kind = u->sys->kind(u->sys->u, full);
    if (!kind && pn > 1 && strchr(",.;:)!?", rel[pn - 1])) {
        rel[--pn] = 0;          /* "see @a.txt." : the full stop is not the name's */
        if (path_join(u->root ? u->root : "", rel, full, sizeof(full)))
            return 0;
        kind = u->sys->kind(u->sys->u, full);
    }
    if (kind == 2) {
        lister l;
        jw lw;
        jw_init(&lw);
        l.w = &lw;
        l.n = 0;
        u->sys->list(u->sys->u, full, list_one, &l);
        jw_rawz(att, "\n\n<directory path=\"");
        jw_rawz(att, rel);
        jw_rawz(att, "\">\n");
        jw_raw(att, lw.p ? lw.p : "", lw.n);
        jw_rawz(att, "</directory>");
        *total += lw.n;
        jw_free(&lw);
        cl_copy(m, "Listed ", sizeof(m));
        cl_cat(m, rel, sizeof(m));
        cl_cat(m, " (", sizeof(m));
        cl_ltoa(l.n, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, l.n == 1 ? " entry)" : " entries)", sizeof(m));
        ui_line(u, m);
        return 1;
    }
    if (kind == 1) {
        char *b = 0;
        long n = 0;
        int r = u->sys->read(u->sys->u, full, MENTION_MAX, &b, &n);
        if (r == SYS_TOO_BIG || (r == 0 && *total + n > MENTIONS_MAX)) {
            free(b);
            cl_copy(m, rel, sizeof(m));
            cl_cat(m, " is too big to attach (64 KB a file); Claude can read it with read_file.", sizeof(m));
            ui_line(u, m);
            jw_rawz(att, "\n\n(The user mentioned the file ");
            jw_rawz(att, rel);
            jw_rawz(att, "; it is too big to attach: read it with read_file, in parts.)");
            return 1;
        }
        if (r) {
            free(b);
            return 0;
        }
        if (memchr(b, 0, (size_t)(n < 4096 ? n : 4096))) {
            free(b);
            cl_copy(m, rel, sizeof(m));
            cl_cat(m, " is not text; not attached.", sizeof(m));
            ui_line(u, m);
            return 1;
        }
        jw_rawz(att, "\n\n<file path=\"");
        jw_rawz(att, rel);
        jw_rawz(att, "\">\n");
        jw_raw(att, b, n);
        if (n && b[n - 1] != '\n')
            jw_raw(att, "\n", 1);
        jw_rawz(att, "</file>");
        *total += n;
        cl_copy(m, "Read ", sizeof(m));
        cl_cat(m, rel, sizeof(m));
        cl_cat(m, " (", sizeof(m));
        cl_ltoa(count_lines(b, n), num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " lines)", sizeof(m));
        ui_line(u, m);
        free(b);
        return 1;
    }
    return 0;
}

/* "@agent-NAME" (A4 gaps 3): a subagent the user wants run for this
 * prompt. Claude Code adds a note that makes Claude invoke that agent; the
 * prompt itself is unchanged. 1 when NAME is an agent. */
static int agent_mention(cl_ui *u, const char *p, long pn, jw *att)
{
    char name[96];
    const cl_agent *a;
    if (!u->tools || pn <= 6 || strncmp(p, "agent-", 6))
        return 0;
    p += 6;
    pn -= 6;
    if (pn >= (long)sizeof(name))
        return 0;
    memcpy(name, p, (size_t)pn);
    name[pn] = 0;
    a = tools_agent(u->tools, name);
    if (!a && pn > 1 && strchr(",.;:)!?", name[pn - 1])) {
        name[pn - 1] = 0;           /* "ask @agent-x." : the full stop is not the name's */
        a = tools_agent(u->tools, name);
    }
    if (!a)
        return 0;
    /* Claude Code's agent_mention attachment (wording from memory of its
     * source, not checked against it) */
    jw_rawz(att, "\n\n<system-reminder>\nThe user has expressed a desire to invoke the agent \"");
    jw_rawz(att, a->name);
    jw_rawz(att, "\". Please invoke the agent appropriately, passing in the required context to it.\n"
                 "</system-reminder>");
    return 1;
}

static int mentions(cl_ui *u, const char *line, jw *out)
{
    const char *p = line;
    jw att;
    long total = 0;
    int any = 0;
    jw_init(&att);
    while (*p) {
        if (*p == '@' && (p == line || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n')) {
            const char *e = p + 1;
            while (*e && *e != ' ' && *e != '\t' && *e != '\n')
                e++;
            if (agent_mention(u, p + 1, (long)(e - p - 1), &att)) {
                any = 1;
                p = e;
                continue;
            }
            any |= mention(u, p + 1, (long)(e - p - 1), &att, &total);
            p = e;
            continue;
        }
        p++;
    }
    if (any) {
        jw_rawz(out, line);
        jw_raw(out, att.p ? att.p : "", att.n);
    }
    jw_free(&att);
    return any ? IN_SEND : IN_PASS;
}

int ui_input(cl_ui *u, const char *line, jw *out)
{
    if (!u->sys)
        return IN_PASS;
    if (line[0] == '!')
        return bash(u, line + 1, out);
    if (line[0] == '#')
        return memory(u, line + 1);
    if (!strncmp(line, "/theme", 6) && (line[6] == 0 || line[6] == ' '))
        return theme_cmd(u, line + 6);
    if (!strncmp(line, "/vim", 4) && (line[4] == 0 || line[4] == ' '))
        return vim_cmd(u);
    if (line[0] != '/' && strchr(line, '@'))
        return mentions(u, line, out);
    return IN_PASS;
}

/* ---- Tab after @: the paths that start with what is typed ---- */

typedef struct comp {
    const char *dir;            /* the typed directory part, kept */
    const char *base;           /* the typed name's start */
    char (*out)[128];
    int n, max;
    int dirs_only;              /* /add-dir, /cd: directories only (A4 gaps 3) */
} comp;

static int comp_one(void *c, const cl_dirent *e)
{
    comp *k = (comp *)c;
    long bl = (long)strlen(k->base);
    int i;
    char cand[128];
    if (k->n >= k->max)
        return 1;
    if (k->dirs_only && !e->dir)
        return 0;
    if (!cl_strnieq(e->name, k->base, bl))
        return 0;
    cl_copy(cand, k->dir, sizeof(cand));
    cl_cat(cand, e->name, sizeof(cand));
    if (e->dir)
        cl_cat(cand, "/", sizeof(cand));
    /* kept in order, as a listing reads */
    for (i = k->n; i > 0 && strcmp(k->out[i - 1], cand) > 0; i--)
        memcpy(k->out[i], k->out[i - 1], 128);
    cl_copy(k->out[i], cand, 128);
    k->n++;
    return 0;
}

/* a directory's names into the screen's cache (tui_dir) */
static int cache_one(void *c, const cl_dirent *e)
{
    tui_dir *d = (tui_dir *)c;
    if (d->n >= TUI_DIR_NAMES)
        return 1;
    jw_rawz(&d->names, e->name);
    if (e->dir)
        jw_raw(&d->names, "/", 1);
    jw_raw(&d->names, "", 1);
    d->n++;
    return 0;
}

/* The list as the screen asks for it per key: the directory read once a
 * prompt (68020 and a floppy: a listing per key would be seconds), the
 * names filtered from the copy */
static void cached(cl_ui *u, const char *full, comp *k)
{
    tui_dir *d = tui_dir_get(u->tui, full);
    const char *p, *e;
    if (!d) {
        d = tui_dir_put(u->tui, full);
        u->sys->list(u->sys->u, full, cache_one, d);
    }
    p = d->names.p;
    e = p ? p + d->names.n : 0;
    while (p && p < e && k->n < k->max) {
        cl_dirent de;
        long l = (long)strlen(p);
        memset(&de, 0, sizeof(de));
        cl_copy(de.name, p, sizeof(de.name));
        if (l && de.name[l - 1] == '/') {
            de.name[l - 1] = 0;
            de.dir = 1;
        }
        comp_one(k, &de);
        p += l + 1;
    }
}

/* the subagents whose name starts with tok, as "agent-NAME" (Claude Code's
 * typeahead offers them for an @ prompt token; while "@agent-" is typed it
 * shows files, the mention still resolves on submit) */
static void agents_matching(cl_ui *u, const char *tok, comp *k)
{
    int i, n = agent_count(u->tools);
    long tl = (long)strlen(tok);
    for (i = 0; i < n && k->n < k->max; i++) {
        const cl_agent *a = agent_get(u->tools, i);
        if (!a || !cl_strnieq(a->name, tok, tl))
            continue;
        cl_copy(k->out[k->n], "agent-", 128);
        cl_cat(k->out[k->n], a->name, 128);
        k->n++;
    }
}

int input_complete(void *uu, const char *tok, char out[][128], int max)
{
    cl_ui *u = (cl_ui *)uu;
    char dir[128], full[512];
    long i, cut = 0;
    comp k;
    if (!u->sys)
        return 0;
    for (i = 0; tok[i]; i++)
        if (tok[i] == '/' || tok[i] == ':')
            cut = i + 1;
    if (cut >= (long)sizeof(dir))
        return 0;
    memcpy(dir, tok, (size_t)cut);
    dir[cut] = 0;
    if (path_join(u->root ? u->root : "", cut ? dir : "", full, sizeof(full)))
        return 0;
    k.dir = dir;
    k.base = tok + cut;
    k.out = out;
    k.n = 0;
    k.max = max;
    k.dirs_only = u->tui && u->tui->comp_dirs;
    if (u->tui && u->tui->cskip && !cut && u->tools && cl_strnieq(tok, "agent-", 6) == 0)
        agents_matching(u, tok, &k);    /* @-mentionable subagents first (A4 gaps 3) */
    if (u->tui)
        cached(u, full, &k);
    else
        u->sys->list(u->sys->u, full, comp_one, &k);
    return k.n;
}

/* ---- the rewind menu's stub: the conversation's own prompts ---- */

/* the conversation's message index of prompt point i, -1 none */
static int point_msg(cl_conv *c, int i)
{
    int k;
    for (k = 0; k < c->n; k++) {
        static const char text[] = "[{\"type\":\"text\"";
        if (!c->m[k].user || strncmp(c->m[k].json, text, sizeof(text) - 1))
            continue;
        if (!i--)
            return k;
    }
    return -1;
}

static int rw_count(void *uu)
{
    cl_ui *u = (cl_ui *)uu;
    int n = 0;
    if (!u->conv)
        return 0;
    while (point_msg(u->conv, n) >= 0)
        n++;
    return n;
}

static int rw_label(void *uu, int i, char *out, long cap)
{
    cl_ui *u = (cl_ui *)uu;
    int k = u->conv ? point_msg(u->conv, i) : -1;
    jv v, b, x;
    jit it;
    out[0] = 0;
    if (k < 0 || json_parse(u->conv->m[k].json, u->conv->m[k].n, &v))
        return -1;
    json_iter(v, &it);
    if (!json_next(&it, 0, &b) || !json_get(b, "text", &x))
        return -1;
    json_str(x, out, cap);
    return 0;
}

static int rw_can(void *uu, int i)
{
    (void)uu;
    (void)i;
    return RW_CONV;
}

static int rw_restore(void *uu, int i, int what)
{
    cl_ui *u = (cl_ui *)uu;
    int k = u->conv ? point_msg(u->conv, i) : -1;
    cl_mark m;
    if (k < 0 || (what & RW_CODE))
        return -1;
    m.n = k;
    m.last = 0x7fffffffL;
    conv_rollback(u->conv, m);
    return 0;
}

void input_rewind_stub(cl_ui *u)
{
    u->rw.u = u;
    u->rw.count = rw_count;
    u->rw.label = rw_label;
    u->rw.can = rw_can;
    u->rw.restore = rw_restore;
}
