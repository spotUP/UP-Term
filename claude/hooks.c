/* hooks -- see hooks.h. */
#include <stdlib.h>
#include <string.h>
#include "hooks.h"
#include "path.h"
#include "regex.h"
#include "util.h"

#define HOOK_OUT 16384

void hookres_init(cl_hookres *r)
{
    memset(r, 0, sizeof(*r));
    jw_init(&r->reason);
    jw_init(&r->context);
    jw_init(&r->shown);
    jw_init(&r->updated);
    jw_init(&r->output);
    jw_init(&r->first);
}

void hookres_free(cl_hookres *r)
{
    jw_free(&r->reason);
    jw_free(&r->context);
    jw_free(&r->shown);
    jw_free(&r->updated);
    jw_free(&r->output);
    jw_free(&r->first);
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

int hooks_any(const cl_hooks *h, int event, const char *name)
{
    int i;
    if (!h->cfg)
        return 0;
    for (i = 0; i < h->cfg->nhooks; i++)
        if (h->cfg->hooks[i].event == event && hooks_match(h->cfg->hooks[i].matcher, name ? name : ""))
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
    if (json_get(v, "continue", &x) && json_type(x) == J_FALSE) {
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
    if (json_get(v, "systemMessage", &x) && json_type(x) == J_STR) {
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
            } else if (json_streq(x, "ask") && r->decision != RULE_DENY)
                r->decision = RULE_ASK;
            else if (json_streq(x, "allow") && r->decision == RULE_NONE)
                r->decision = RULE_ALLOW;
        }
        if (json_get(hs, "updatedToolOutput", &x) && event == HK_POST_TOOL) {
            jw_reset(&r->output);
            jw_raw(&r->output, x.p, x.n);
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
            } else if (json_get(x, "behavior", &b) && json_streq(b, "allow") && r->behavior != RULE_DENY)
                r->behavior = RULE_ALLOW;
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

/* A prompt hook: its text with $ARGUMENTS (else appended) the event's
 * JSON, asked of a model; {"ok": false, "reason": ...} blocks (Stop: sends
 * Claude on), unless "impossible" says the condition can never be met. */
static void prompt_hook(cl_hooks *h, const cl_hook *k, int event, const char *file, cl_hookres *r)
{
    char *in = 0;
    long n = 0, i;
    jw q, a;
    jv v, x;
    if (!h->ask_model || h->sys->read(h->sys->u, file, 256L * 1024, &in, &n))
        return;
    jw_init(&q);
    jw_init(&a);
    for (i = 0; k->cmd[i];) {
        if (!strncmp(k->cmd + i, "$ARGUMENTS", 10)) {
            jw_raw(&q, in, n);
            i += 10;
        } else
            jw_raw(&q, k->cmd + i++, 1);
    }
    if (!strstr(k->cmd, "$ARGUMENTS")) {
        jw_rawz(&q, "\n\n");
        jw_raw(&q, in, n);
    }
    jw_rawz(&q, "\n\nRespond with JSON only: {\"ok\": true} or {\"ok\": false, \"reason\": \"...\"}.");
    free(in);
    h->n_run++;
    r->ran++;
    if (!q.oom && h->ask_model(h->u, k->model, q.p, &a) == 0) {
        long s = 0, e = a.n;
        while (s < e && a.p[s] != '{')
            s++;
        while (e > s && a.p[e - 1] != '}')
            e--;
        if (e > s && json_parse(a.p + s, e - s, &v) == 0 && json_get(v, "ok", &x) && json_type(x) == J_FALSE) {
            jv im;
            if (!(json_get(v, "impossible", &im) && json_type(im) == J_TRUE) && can_block(event)) {
                char m[600];
                m[0] = 0;
                if (json_get(v, "reason", &x))
                    json_str(x, m, sizeof(m));
                r->blocked = 1;
                add_line(&r->reason, m[0] ? m : "a prompt hook said no", m[0] ? (long)strlen(m) : 20);
            }
        }
        if (h->seen)
            h->seen(h->u, event, k->cmd, 1, 0, a.p ? a.p : "", a.n);
    } else {
        add_line(&r->shown, "A prompt hook could not ask its model.", 38);
        if (h->seen)
            h->seen(h->u, event, k->cmd, 1, -1, "", 0);
    }
    jw_free(&q);
    jw_free(&a);
}

int hooks_run(cl_hooks *h, int event, const char *name, const char *extra, cl_hookres *r)
{
    char file[300], line[1200], num[16];
    char *o = 0;
    jw ev;
    int i;
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
    jw_free(&ev);
    for (i = 0; i < h->cfg->nhooks; i++) {
        const cl_hook *k = &h->cfg->hooks[i];
        long on = 0, rc = 0;
        int st;
        if (k->event != event || !hooks_match(k->matcher, name ? name : ""))
            continue;
        if (k->cond && *k->cond) {
            /* "if": a permission rule the tool call must match */
            jv in;
            if (!h->tool || !h->input || json_parse(h->input, h->input_n, &in) ||
                !cfg_rule_match(k->cond, h->tool, in, h->cwd ? h->cwd : ""))
                continue;
        }
        if (k->once) {
            /* "once": the first run of the session only */
            unsigned long hs = 5381;
            const char *c;
            int j, done = 0;
            for (c = k->cmd; *c; c++)
                hs = hs * 33 + (unsigned char)*c;
            hs = hs * 33 + (unsigned long)event;
            for (j = 0; j < h->nonce; j++)
                done |= h->once_done[j] == hs;
            if (done)
                continue;
            if (h->nonce < 16)
                h->once_done[h->nonce++] = hs;
        }
        if (k->status && h->status)
            h->status(h->u, k->status);
        if (h->seen)
            h->seen(h->u, event, k->cmd, 0, -1, "", 0);
        if (k->kind == HOOK_PROMPT) {
            /* a prompt hook: the model says {"ok": ...}; ok false is a block */
            prompt_hook(h, k, event, file, r);
            if (k->status && h->status)
                h->status(h->u, 0);
            continue;
        }
        project_cmd(h, k->cmd, line, sizeof(line) - 320);
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
        } else if (rc == 2 && can_block(event)) {
            /* Claude Code reads JSON on every exit code; exit 2 blocks whatever it says */
            long had = r->reason.n;
            r->blocked = 1;
            if (!json_answer(event, o, on, r) || r->reason.n == had)
                add_line(&r->reason, o, on);
        } else if (json_answer(event, o, on, r)) {
            ;                       /* a JSON answer: its fields say what happens */
        } else {
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
    free(o);
    if (h->sys->remove)
        h->sys->remove(h->sys->u, file);
    return r->ran;
}
