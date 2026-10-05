/* hooks -- see hooks.h. */
#include <stdlib.h>
#include <string.h>
#include "hooks.h"
#include "path.h"
#include "regex.h"
#include "util.h"

#define HOOK_OUT 16384
#define ASYNC_MAX 8

/* an async hook in flight (A4 gaps 2) */
typedef struct hk_async {
    long job;
    int event;
    char file[300];             /* its input file (removed when it is collected) */
    char cmd[120];
} hk_async;

void hookres_init(cl_hookres *r)
{
    memset(r, 0, sizeof(*r));
    jw_init(&r->reason);
    jw_init(&r->context);
    jw_init(&r->shown);
    jw_init(&r->updated);
    jw_init(&r->output);
    jw_init(&r->first);
    jw_init(&r->watch);
    jw_init(&r->display);
}

void hookres_free(cl_hookres *r)
{
    jw_free(&r->reason);
    jw_free(&r->context);
    jw_free(&r->shown);
    jw_free(&r->updated);
    jw_free(&r->output);
    jw_free(&r->first);
    jw_free(&r->watch);
    jw_free(&r->display);
    hookres_init(r);
}

/* is the matcher a plain list (letters, digits, _ - space , |)? */
static int plain(const char *m)
{
    for (; *m; m++)
        if (!((*m >= 'a' && *m <= 'z') || (*m >= 'A' && *m <= 'Z') || (*m >= '0' && *m <= '9') || *m == '_' ||
              *m == '-' || *m == ' ' || *m == ',' || *m == '|'))
            return 0;
    return 1;
}

static int is_name(const char *a, long n, const char *name)
{
    return (long)strlen(name) == n && !strncmp(a, name, (size_t)n);
}

/* Claude Code's matcher rules (hooks.md "Matcher patterns"): "" or "*"
 * all; only letters, digits, _ - space , | an exact name or a list of
 * them; anything else a regular expression, unanchored. The name is
 * tried as given and as Claude Code calls it (Task also as Agent). */
int hooks_match(const char *m, const char *name)
{
    const char *cc = cfg_cc_tool(name), *alias = !strcmp(cc, "Task") ? "Agent" : 0;
    if (!m[0] || !strcmp(m, "*"))
        return 1;
    if (plain(m)) {
        while (*m) {
            const char *e;
            long n;
            while (*m == ' ' || *m == ',' || *m == '|')
                m++;
            for (e = m; *e && *e != ',' && *e != '|'; e++)
                ;
            n = (long)(e - m);
            while (n && m[n - 1] == ' ')
                n--;
            if (n && (is_name(m, n, name) || is_name(m, n, cc) || (alias && is_name(m, n, alias))))
                return 1;
            m = e;
        }
        return 0;
    }
    {
        char err[100];
        cl_re *re = re_compile(m, 0, err, sizeof(err));
        long ms, me;
        int hit;
        if (!re)
            return 0;               /* not a usable expression: matches nothing */
        hit = re_search(re, name, (long)strlen(name), 0, &ms, &me) == 1 ||
              re_search(re, cc, (long)strlen(cc), 0, &ms, &me) == 1 ||
              (alias && re_search(re, alias, (long)strlen(alias), 0, &ms, &me) == 1);
        re_free(re);
        return hit;
    }
}

/* the i-th hook of both lists (the settings', then the frontmatter's); 0
 * past the end. The settings' are skipped while they are held. */
static cl_hook *hook_at(const cl_hooks *h, int i)
{
    int n = h->cfg && !h->held ? h->cfg->nhooks : 0;
    if (i < n)
        return &h->cfg->hooks[i];
    i -= n;
    return h->extra && i < h->extra->nhooks ? &h->extra->hooks[i] : 0;
}

/* a "once" hook already done? (Claude Code honours once in a skill's frontmatter only) */
static unsigned long once_hash(const cl_hook *k)
{
    unsigned long hs = 5381;
    const char *c;
    for (c = k->cmd; *c; c++)
        hs = hs * 33 + (unsigned char)*c;
    return hs * 33 + (unsigned long)k->event + (unsigned long)k->owner * 7;
}

static int once_done(const cl_hooks *h, const cl_hook *k)
{
    int j;
    unsigned long hs;
    if (!k->once || k->owner <= 0)
        return 0;
    hs = once_hash(k);
    for (j = 0; j < h->nonce; j++)
        if (h->once_done[j] == hs)
            return 1;
    return 0;
}

static void once_mark(cl_hooks *h, const cl_hook *k)
{
    if (k->once && k->owner > 0 && h->nonce < 16 && !once_done(h, k))
        h->once_done[h->nonce++] = once_hash(k);
}

int hooks_has(const cl_hooks *h, int event)
{
    int i;
    cl_hook *k;
    for (i = 0; (k = hook_at(h, i)) != 0; i++)
        if (k->event == event && !once_done(h, k))
            return 1;
    return 0;
}

int hooks_any(const cl_hooks *h, int event, const char *name)
{
    int i;
    cl_hook *k;
    for (i = 0; (k = hook_at(h, i)) != 0; i++)
        if (k->event == event && hooks_match(k->matcher, name ? name : "") && !once_done(h, k))
            return 1;
    return 0;
}

static void add_line(jw *w, const char *s, long n)
{
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        n--;
    if (!n)
        return;
    if (w->n)
        jw_raw(w, "\n", 1);
    jw_raw(w, s, n);
}

/* terminalSequence: only Claude Code's allowlist -- OSC 0/1/2/9/99/777
 * (ended by BEL or ESC \) and BEL; anything else and it is ignored */
static int term_ok(const char *s, long n)
{
    long i = 0;
    while (i < n) {
        if (s[i] == 7) {
            i++;
            continue;
        }
        if (s[i] == 27 && i + 1 < n && s[i + 1] == ']') {
            long k = i + 2, num = 0, d = 0;
            while (k < n && s[k] >= '0' && s[k] <= '9' && d < 4) {
                num = num * 10 + (s[k++] - '0');
                d++;
            }
            if (!d || k >= n || s[k] != ';' ||
                !(num == 0 || num == 1 || num == 2 || num == 9 || num == 99 || num == 777))
                return 0;
            while (k < n && s[k] != 7 && !(s[k] == 27 && k + 1 < n && s[k + 1] == '\\')) {
                if ((unsigned char)s[k] < 32)
                    return 0;
                k++;
            }
            if (k >= n)
                return 0;
            i = s[k] == 7 ? k + 1 : k + 2;
            continue;
        }
        return 0;
    }
    return n > 0;
}

static cl_hooks *cur_hooks;     /* the json_answer below hands terminalSequence to it */

/* the output read as Claude Code's JSON answer: 1 when it was one */
static int json_answer(int event, const char *o, long n, cl_hookres *r)
{
    jv v, x, hs;
    long i = 0;
    while (i < n && (o[i] == ' ' || o[i] == '\n' || o[i] == '\r' || o[i] == '\t'))
        i++;
    if (i >= n || o[i] != '{' || json_parse(o + i, n - i, &v) || json_type(v) != J_OBJ)
        return 0;
    if (json_get(v, "continue", &x) && json_type(x) == J_FALSE && event != HK_MESSAGE_DISPLAY &&
        event != HK_FILE_CHANGED && event != HK_CWD_CHANGED) {
        r->stop = 1;
        if (json_get(v, "stopReason", &x) && json_type(x) == J_STR) {
            char m[300];
            json_str(x, m, sizeof(m));
            add_line(&r->shown, m, (long)strlen(m));
        }
    }
    if (json_get(v, "terminalSequence", &x) && json_type(x) == J_STR && cur_hooks && cur_hooks->term) {
        long l;
        char *t = json_strdup(x, &l);
        if (t && term_ok(t, l))
            cur_hooks->term(cur_hooks->u, t, l);
        free(t);
    }
    if (json_get(v, "systemMessage", &x) && json_type(x) == J_STR && event != HK_MESSAGE_DISPLAY) {
        char m[600];
        json_str(x, m, sizeof(m));
        add_line(&r->shown, m, (long)strlen(m));    /* for the user */
    }
    if (json_get(v, "decision", &x) && json_streq(x, "block")) {
        char m[1000];
        r->blocked = 1;
        m[0] = 0;
        if (json_get(v, "reason", &x))
            json_str(x, m, sizeof(m));
        add_line(&r->reason, m, (long)strlen(m));
    }
    if (json_get(v, "hookSpecificOutput", &hs) && json_type(hs) == J_OBJ) {
        if (json_get(hs, "permissionDecision", &x) && event == HK_PRE_TOOL) {
            char m[1000];
            m[0] = 0;
            if (json_get(hs, "permissionDecisionReason", &v))
                json_str(v, m, sizeof(m));
            if (json_streq(x, "deny")) {
                r->decision = RULE_DENY;
                r->blocked = 1;
                add_line(&r->reason, m, (long)strlen(m));
            } else if (json_streq(x, "defer"))
                r->defer = 1;       /* print mode: the run stops at this call (tool_deferred) */
            else if (json_streq(x, "ask") && r->decision != RULE_DENY)
                r->decision = RULE_ASK;
            else if (json_streq(x, "allow") && r->decision == RULE_NONE)
                r->decision = RULE_ALLOW;
        }
        if (json_get(hs, "updatedToolOutput", &x) && event == HK_POST_TOOL) {
            jw_reset(&r->output);
            jw_raw(&r->output, x.p, x.n);
        }
        if (json_get(hs, "watchPaths", &x) && json_type(x) == J_ARR &&
            (event == HK_SESSION_START || event == HK_CWD_CHANGED || event == HK_FILE_CHANGED)) {
            jw_reset(&r->watch);
            jw_raw(&r->watch, x.p, x.n);
            r->has_watch = 1;
        }
        if (json_get(hs, "displayContent", &x) && json_type(x) == J_STR && event == HK_MESSAGE_DISPLAY) {
            long l;
            char *t = json_strdup(x, &l);
            if (t) {
                jw_reset(&r->display);
                jw_raw(&r->display, t, l);
                r->has_display = 1;
            }
            free(t);
        }
        if (event == HK_SESSION_START) {
            jv y;
            if (json_get(hs, "sessionTitle", &y) && json_type(y) == J_STR)
                json_str(y, r->title, sizeof(r->title));
            if (json_get(hs, "initialUserMessage", &y) && json_type(y) == J_STR) {
                long l;
                char *t = json_strdup(y, &l);
                if (t) {
                    jw_reset(&r->first);
                    jw_raw(&r->first, t, l > 10000 ? 10000 : l);
                }
                free(t);
            }
            if (json_get(hs, "reloadSkills", &y) && json_type(y) == J_TRUE)
                r->reload = 1;
        }
        if (json_get(hs, "updatedInput", &x) && json_type(x) == J_OBJ && event == HK_PRE_TOOL) {
            jw_reset(&r->updated);
            jw_raw(&r->updated, x.p, x.n);
        }
        if (json_get(hs, "decision", &x) && json_type(x) == J_OBJ && event == HK_PERMISSION_REQUEST) {
            jv b;
            if (json_get(x, "behavior", &b) && json_streq(b, "deny")) {
                char m[600];
                r->behavior = RULE_DENY;
                m[0] = 0;
                if (json_get(x, "message", &b))
                    json_str(b, m, sizeof(m));
                add_line(&r->reason, m, (long)strlen(m));
                if (json_get(x, "interrupt", &b) && json_type(b) == J_TRUE)
                    r->stop = 1;
            } else if (json_get(x, "behavior", &b) && json_streq(b, "allow") && r->behavior != RULE_DENY) {
                r->behavior = RULE_ALLOW;
                /* "allow" with updatedInput: the call runs with it (A4 gaps 2) */
                if (json_get(x, "updatedInput", &b) && json_type(b) == J_OBJ) {
                    jw_reset(&r->updated);
                    jw_raw(&r->updated, b.p, b.n);
                }
            }
        }
        if (json_get(hs, "additionalContext", &x) && json_type(x) == J_STR) {
            long l;
            char *t = json_strdup(x, &l);
            if (t)
                add_line(&r->context, t, l);
            free(t);
        }
    }
    return 1;
}

static int can_block(int event)
{
    return event == HK_PRE_TOOL || event == HK_POST_TOOL || event == HK_PROMPT || event == HK_STOP ||
           event == HK_SUBAGENT_STOP || event == HK_PRE_COMPACT || event == HK_PROMPT_EXPANSION ||
           event == HK_POST_TOOL_FAILURE || event == HK_PRE_MODEL_SWITCH || event == HK_POST_TOOL_BATCH ||
           event == HK_CONFIG_CHANGE;
}

/* the command with ${CLAUDE_PROJECT_DIR} and $CLAUDE_PROJECT_DIR put in
 * (the AmigaShell knows no ${...}), the variable also set for it */
static void project_cmd(cl_hooks *h, const char *cmd, char *out, long cap)
{
    const char *pd = h->project_dir ? h->project_dir : h->cwd ? h->cwd : "";
    long k = 0;
    if (h->sys->setenv)
        h->sys->setenv(h->sys->u, "CLAUDE_PROJECT_DIR", pd);
    while (*cmd && k < cap - 1) {
        long l = !strncmp(cmd, "${CLAUDE_PROJECT_DIR}", 21) ? 21 : !strncmp(cmd, "$CLAUDE_PROJECT_DIR", 19) ? 19 : 0;
        if (l) {
            long pl = (long)strlen(pd);
            if (k + pl >= cap - 1)
                break;
            memcpy(out + k, pd, (size_t)pl);
            k += pl;
            cmd += l;
        } else
            out[k++] = *cmd++;
    }
    out[k] = 0;
}

/* $ARGUMENTS in a prompt or agent hook's text: the event's JSON (else
 * appended); \$ a dollar sign */
static void with_args(const char *text, const char *in, long n, jw *q)
{
    long i;
    for (i = 0; text[i];) {
        if (text[i] == '\\' && text[i + 1] == '$') {
            jw_raw(q, "$", 1);
            i += 2;
        } else if (!strncmp(text + i, "$ARGUMENTS", 10)) {
            jw_raw(q, in, n);
            i += 10;
        } else
            jw_raw(q, text + i++, 1);
    }
    if (!strstr(text, "$ARGUMENTS")) {
        jw_rawz(q, "\n\n");
        jw_raw(q, in, n);
    }
}

/* the {"ok": ...} answer of a prompt or an agent hook: 1 run (ok or not),
 * 0 no usable answer */
static int ok_answer(const char *a, long an, int event, int impossible_ok, cl_hookres *r)
{
    long s = 0, e = an;
    jv v, x;
    while (s < e && a[s] != '{')
        s++;
    while (e > s && a[e - 1] != '}')
        e--;
    if (e <= s || json_parse(a + s, e - s, &v) || !json_get(v, "ok", &x))
        return 0;
    if (json_type(x) == J_FALSE) {
        jv im;
        if (!(impossible_ok && json_get(v, "impossible", &im) && json_type(im) == J_TRUE) && can_block(event)) {
            char m[600];
            m[0] = 0;
            if (json_get(v, "reason", &x))
                json_str(x, m, sizeof(m));
            r->blocked = 1;
            add_line(&r->reason, m[0] ? m : "a hook said no", m[0] ? (long)strlen(m) : 15);
        }
    }
    return 1;
}

/* A prompt hook: its text with $ARGUMENTS (else appended) the event's
 * JSON, asked of a model; {"ok": false, "reason": ...} blocks (Stop: sends
 * Claude on), unless "impossible" says the condition can never be met.
 * An agent hook: the same question to a subagent that can Read, Grep and
 * Glob (Claude Code's 50 turns); no "impossible". */
static int model_hook(cl_hooks *h, const cl_hook *k, int event, const char *file, cl_hookres *r)
{
    char *in = 0;
    long n = 0;
    int agent = k->kind == HOOK_AGENT, rc, ok = 0;
    jw q, a;
    if ((agent ? !h->ask_agent : !h->ask_model) || h->sys->read(h->sys->u, file, 256L * 1024, &in, &n))
        return 0;
    jw_init(&q);
    jw_init(&a);
    with_args(k->cmd, in, n, &q);
    jw_rawz(&q, "\n\nRespond with JSON only: {\"ok\": true} or {\"ok\": false, \"reason\": \"...\"}.");
    free(in);
    h->n_run++;
    r->ran++;
    rc = q.oom ? -1 : agent ? h->ask_agent(h->u, k->model, q.p, &a) : h->ask_model(h->u, k->model, q.p, &a);
    if (rc == 0 && ok_answer(a.p ? a.p : "", a.n, event, !agent, r)) {
        ok = !r->blocked;
        if (h->seen)
            h->seen(h->u, event, k->cmd, 1, 0, a.p ? a.p : "", a.n);
    } else {
        add_line(&r->shown, agent ? "An agent hook could not reach a decision." : "A prompt hook could not ask its model.",
                 agent ? 41 : 38);
        if (h->seen)
            h->seen(h->u, event, k->cmd, 1, -1, "", 0);
    }
    jw_free(&q);
    jw_free(&a);
    return ok;
}

/* an http hook's headers: the object's values with $VAR / ${VAR} put in for
 * the variables allowedEnvVars names (the others empty), as header lines */
static void http_headers(cl_hooks *h, const cl_hook *k, jw *out)
{
    jv o, key, val;
    jit it;
    if (!k->headers || json_parse(k->headers, (long)strlen(k->headers), &o) || json_type(o) != J_OBJ)
        return;
    json_iter(o, &it);
    while (json_next(&it, &key, &val)) {
        char name[64], v[512];
        long i;
        if (json_type(val) != J_STR)
            continue;
        json_str(key, name, sizeof(name));
        json_str(val, v, sizeof(v));
        if (strpbrk(name, "\r\n:") || strpbrk(v, "\r\n"))
            continue;               /* no header injection */
        jw_rawz(out, name);
        jw_rawz(out, ": ");
        for (i = 0; v[i];) {
            if (v[i] == '$' && (v[i + 1] == '{' || (v[i + 1] >= 'A' && v[i + 1] <= 'Z') || v[i + 1] == '_')) {
                char var[64], got[256];
                long s = i + 1, e, l;
                int brace = v[s] == '{';
                if (brace)
                    s++;
                for (e = s; v[e] && ((v[e] >= 'A' && v[e] <= 'Z') || (v[e] >= 'a' && v[e] <= 'z') ||
                                     (v[e] >= '0' && v[e] <= '9') || v[e] == '_');
                     e++)
                    ;
                l = e - s;
                if (l <= 0 || l >= (long)sizeof(var) || (brace && v[e] != '}')) {
                    jw_raw(out, v + i++, 1);
                    continue;
                }
                memcpy(var, v + s, (size_t)l);
                var[l] = 0;
                got[0] = 0;
                if (k->env_ok) {
                    /* only a variable named in allowedEnvVars is read */
                    const char *p = k->env_ok;
                    while (*p) {
                        long wl = (long)strcspn(p, ",");
                        if (wl == l && !strncmp(p, var, (size_t)l)) {
                            if (!h->sys->getenv || h->sys->getenv(h->sys->u, var, got, sizeof(got)) < 0)
                                got[0] = 0;
                            break;
                        }
                        p += wl;
                        if (*p)
                            p++;
                    }
                }
                if (!strpbrk(got, "\r\n"))
                    jw_rawz(out, got);
                i = brace ? e + 1 : e;
            } else
                jw_raw(out, v + i++, 1);
        }
        jw_rawz(out, "\r\n");
    }
}

/* An http hook: the event's JSON POSTed to its URL. 2xx and an empty body
 * is success, 2xx and a JSON object is read as a command hook's answer;
 * anything else is a non-blocking error (shown), never a block. 1 when it
 * succeeded. */
static int http_hook(cl_hooks *h, const cl_hook *k, int event, const char *body, long bn, cl_hookres *r)
{
    jw hd, resp;
    char err[200];
    int status = 0, rc, ok = 0;
    if (!h->post) {
        add_line(&r->shown, "An http hook could not run: there is no network here.", 54);
        return 0;
    }
    jw_init(&hd);
    jw_init(&resp);
    http_headers(h, k, &hd);
    h->n_run++;
    r->ran++;
    rc = h->post(h->u, k->cmd, hd.p ? hd.p : "", body, bn, k->timeout_s, &status, &resp, err, sizeof(err));
    if (h->seen)
        h->seen(h->u, event, k->cmd, 1, rc ? -1 : status, resp.p ? resp.p : "", resp.n);
    if (rc) {
        char m[400];
        cl_copy(m, "The http hook ", sizeof(m));
        cl_cat(m, k->cmd, sizeof(m));
        cl_cat(m, " failed: ", sizeof(m));
        cl_cat(m, err, sizeof(m));
        add_line(&r->shown, m, (long)strlen(m));
    } else if (status < 200 || status > 299) {
        char m[400], num[16];
        cl_copy(m, "The http hook ", sizeof(m));
        cl_cat(m, k->cmd, sizeof(m));
        cl_cat(m, " answered HTTP ", sizeof(m));
        cl_ltoa(status, num);
        cl_cat(m, num, sizeof(m));
        add_line(&r->shown, m, (long)strlen(m));
    } else {
        long i = 0;
        while (i < resp.n && (resp.p[i] == ' ' || resp.p[i] == '\n' || resp.p[i] == '\r' || resp.p[i] == '\t'))
            i++;
        if (i >= resp.n || json_answer(event, resp.p, resp.n, r))
            ok = 1;
        else {
            char m[300];
            cl_copy(m, "The http hook ", sizeof(m));
            cl_cat(m, k->cmd, sizeof(m));
            cl_cat(m, " answered with something that is not a JSON object; ignored", sizeof(m));
            add_line(&r->shown, m, (long)strlen(m));
        }
    }
    jw_free(&hd);
    jw_free(&resp);
    return ok;
}

/* An async hook: started in the background on a copy of the event file;
 * its answer is collected later (hooks_async_poll). 1 started. */
static int async_hook(cl_hooks *h, const cl_hook *k, int event, const char *file, const char *line_fmt)
{
    hk_async *a;
    char copy[300], num[16], line[1200];
    char *b = 0;
    long n = 0, job;
    if (!h->sys->bg_start || h->nasync >= ASYNC_MAX)
        return 0;
    if (!h->async) {
        h->async = (hk_async *)calloc(ASYNC_MAX, sizeof(hk_async));
        if (!h->async)
            return 0;
    }
    /* the input file of its own (the shared one is removed when the event's hooks are done) */
    cl_copy(copy, file, sizeof(copy) - 16);
    cl_cat(copy, "-a", sizeof(copy));
    cl_ltoa(h->n_async + 1, num);
    cl_cat(copy, num, sizeof(copy));
    if (h->sys->read(h->sys->u, file, 256L * 1024, &b, &n) || h->sys->write(h->sys->u, copy, b, n)) {
        free(b);
        return 0;
    }
    free(b);
    cl_copy(line, line_fmt, sizeof(line) - 320);
    cl_cat(line, " < ", sizeof(line));
    cl_cat(line, copy, sizeof(line));
    if (h->sys->bg_start(h->sys->u, line, &job)) {
        if (h->sys->remove)
            h->sys->remove(h->sys->u, copy);
        return 0;
    }
    a = &h->async[h->nasync++];
    a->job = job;
    a->event = event;
    cl_copy(a->file, copy, sizeof(a->file));
    cl_copy(a->cmd, k->cmd, sizeof(a->cmd));
    h->n_async++;
    return 1;
}

int hooks_async_poll(cl_hooks *h, jw *w)
{
    int i, got = 0;
    if (!h->async || !h->sys->bg_read)
        return 0;
    for (i = 0; i < h->nasync;) {
        hk_async *a = &h->async[i];
        char *o = (char *)malloc(HOOK_OUT);
        long n = 0, rc = 0;
        int running = 1;
        if (!o)
            return got;
        if (h->sys->bg_read(h->sys->u, a->job, 0, o, HOOK_OUT - 1, &n, &running, &rc) || running) {
            free(o);
            i++;
            continue;
        }
        o[n] = 0;
        {
            /* Claude Code: additionalContext and systemMessage reach Claude on the next turn */
            cl_hookres r;
            hookres_init(&r);
            json_answer(a->event, o, n, &r);
            if (r.shown.n || r.context.n || (rc == 2 && n)) {
                if (w->n)
                    jw_raw(w, "\n", 1);
                jw_rawz(w, "Async hook (");
                jw_rawz(w, cfg_hook_events[a->event]);
                jw_rawz(w, ") \"");
                jw_rawz(w, a->cmd);
                jw_rawz(w, rc == 2 ? "\" exited with 2: " : "\" finished: ");
                if (r.shown.n)
                    jw_raw(w, r.shown.p, r.shown.n);
                if (r.context.n) {
                    if (r.shown.n)
                        jw_raw(w, "\n", 1);
                    jw_raw(w, r.context.p, r.context.n);
                }
                if (!r.shown.n && !r.context.n)
                    jw_raw(w, o, n > 2000 ? 2000 : n);
                got++;
            }
            hookres_free(&r);
        }
        free(o);
        if (h->sys->bg_drop)
            h->sys->bg_drop(h->sys->u, a->job);
        if (h->sys->remove)
            h->sys->remove(h->sys->u, a->file);
        h->async[i] = h->async[--h->nasync];
    }
    return got;
}

void hooks_async_stop(cl_hooks *h)
{
    int i;
    for (i = 0; h->async && i < h->nasync; i++) {
        if (h->sys->bg_kill)
            h->sys->bg_kill(h->sys->u, h->async[i].job);
        if (h->sys->bg_drop)
            h->sys->bg_drop(h->sys->u, h->async[i].job);
        if (h->sys->remove)
            h->sys->remove(h->sys->u, h->async[i].file);
    }
    free(h->async);
    h->async = 0;
    h->nasync = 0;
}

int hooks_watch_names(const cl_hooks *h, void (*fn)(void *c, const char *name), void *c)
{
    int i, n = 0;
    cl_hook *k;
    for (i = 0; (k = hook_at(h, i)) != 0; i++) {
        const char *p = k->matcher;
        if (k->event != HK_FILE_CHANGED)
            continue;
        while (*p) {
            char name[200];
            long l = (long)strcspn(p, "|");
            if (l > 0 && l < (long)sizeof(name)) {
                memcpy(name, p, (size_t)l);
                name[l] = 0;
                fn(c, name);
                n++;
            }
            p += l;
            if (*p)
                p++;
        }
    }
    return n;
}

int hooks_run(cl_hooks *h, int event, const char *name, const char *extra, cl_hookres *r)
{
    char file[300], line[1200], num[16];
    char *o = 0;
    jw ev;
    int i;
    cl_hook *k;
    if (!hooks_any(h, event, name))
        return 0;
    if (path_join(h->tmp ? h->tmp : "T:", "Claude-hook.json", file, sizeof(file)))
        return 0;
    cur_hooks = h;
    jw_init(&ev);
    jw_rawz(&ev, "{\"session_id\":");
    jw_strz(&ev, h->session_id ? h->session_id : "");
    jw_rawz(&ev, ",\"transcript_path\":");
    jw_strz(&ev, h->transcript ? h->transcript : "");
    jw_rawz(&ev, ",\"cwd\":");
    jw_strz(&ev, h->cwd ? h->cwd : "");
    jw_rawz(&ev, ",\"hook_event_name\":");
    jw_strz(&ev, cfg_hook_events[event]);
    jw_rawz(&ev, extra ? extra : "");
    jw_rawz(&ev, "}\n");
    o = (char *)malloc(HOOK_OUT);
    if (ev.oom || !o || h->sys->write(h->sys->u, file, ev.p, ev.n)) {
        add_line(&r->shown, "A hook could not be run: its input file could not be written.", 62);
        jw_free(&ev);
        free(o);
        return 0;
    }
    for (i = 0; (k = hook_at(h, i)) != 0; i++) {
        long on = 0, rc = 0;
        int st, ok = 0;
        if (k->event != event || !hooks_match(k->matcher, name ? name : "") || once_done(h, k))
            continue;
        if (k->cond && *k->cond) {
            /* "if": a permission rule the tool call must match */
            jv in;
            if (!h->tool || !h->input || json_parse(h->input, h->input_n, &in) ||
                !cfg_rule_match_any(k->cond, h->tool, in, h->cwd ? h->cwd : ""))
                continue;
        }
        if (k->status && h->status)
            h->status(h->u, k->status);
        if (h->seen)
            h->seen(h->u, event, k->cmd, 0, -1, "", 0);
        if (k->kind == HOOK_PROMPT || k->kind == HOOK_AGENT) {
            /* the model (or a subagent) says {"ok": ...}; ok false is a block */
            ok = model_hook(h, k, event, file, r);
            if (k->status && h->status)
                h->status(h->u, 0);
            if (ok)
                once_mark(h, k);
            continue;
        }
        if (k->kind == HOOK_HTTP) {
            ok = http_hook(h, k, event, ev.p, ev.n - 1, r);
            if (k->status && h->status)
                h->status(h->u, 0);
            if (ok)
                once_mark(h, k);
            continue;
        }
        project_cmd(h, k->cmd, line, sizeof(line) - 320);
        if (k->async) {
            /* async: true -- started, not waited for; its answer comes on a later turn */
            if (k->status && h->status)
                h->status(h->u, 0);
            if (async_hook(h, k, event, file, line)) {
                h->n_run++;
                r->ran++;
                once_mark(h, k);
            } else
                add_line(&r->shown, "An async hook could not be started in the background.", 53);
            continue;
        }
        cl_cat(line, " < ", sizeof(line));
        cl_cat(line, file, sizeof(line));
        st = h->sys->run(h->sys->u, line, k->timeout_s, o, HOOK_OUT - 1, &on, &rc);
        if (k->status && h->status)
            h->status(h->u, 0);
        if (h->seen)
            h->seen(h->u, event, k->cmd, 1, st < 0 ? -1 : rc, o, st < 0 ? 0 : on);
        h->n_run++;
        r->ran++;
        if (st < 0) {
            char m[400];
            cl_copy(m, "The hook \"", sizeof(m));
            cl_cat(m, k->cmd, sizeof(m));
            cl_cat(m, st == SYS_BREAK ? "\" was stopped." : st == SYS_TIMEOUT ? "\" ran out of time." : "\" did not run: ",
                   sizeof(m));
            if (st == -1)
                cl_cat(m, h->sys->err(h->sys->u), sizeof(m));
            add_line(&r->shown, m, (long)strlen(m));
            continue;
        }
        o[on] = 0;
        if (rc == 0) {
            if (!json_answer(event, o, on, r) && (event == HK_PROMPT || event == HK_SESSION_START))
                add_line(&r->context, o, on);
            once_mark(h, k);        /* Claude Code: a once hook goes after its first successful run */
        } else if (rc == 2 && can_block(event)) {
            /* Claude Code reads JSON on every exit code; exit 2 blocks whatever it says */
            long had = r->reason.n;
            r->blocked = 1;
            if (!json_answer(event, o, on, r) || r->reason.n == had)
                add_line(&r->reason, o, on);
        } else if (json_answer(event, o, on, r)) {
            ;                       /* a JSON answer: its fields say what happens */
        } else if (event != HK_MESSAGE_DISPLAY) {
            char m[400];
            cl_copy(m, cfg_hook_events[event], sizeof(m));
            cl_cat(m, " hook \"", sizeof(m));
            cl_cat(m, k->cmd, sizeof(m));
            cl_cat(m, "\" failed with ", sizeof(m));
            cl_ltoa(rc, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, on ? ": " : ".", sizeof(m));
            {
                jw t;
                jw_init(&t);
                jw_rawz(&t, m);
                jw_raw(&t, o, on > 600 ? 600 : on);
                add_line(&r->shown, t.p ? t.p : m, t.p ? t.n : (long)strlen(m));
                jw_free(&t);
            }
        }
    }
    jw_free(&ev);
    free(o);
    if (h->sys->remove)
        h->sys->remove(h->sys->u, file);
    return r->ran;
}
