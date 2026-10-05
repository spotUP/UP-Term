/* hooks -- see hooks.h. */
#include <stdlib.h>
#include <string.h>
#include "hooks.h"
#include "path.h"
#include "util.h"

#define HOOK_OUT 16384

void hookres_init(cl_hookres *r)
{
    memset(r, 0, sizeof(*r));
    jw_init(&r->reason);
    jw_init(&r->context);
    jw_init(&r->shown);
    jw_init(&r->updated);
}

void hookres_free(cl_hookres *r)
{
    jw_free(&r->reason);
    jw_free(&r->context);
    jw_free(&r->shown);
    jw_free(&r->updated);
    hookres_init(r);
}

/* * and .* stand for any text */
static int wild(const char *p, const char *pe, const char *s)
{
    while (p < pe) {
        if (*p == '*' || (p[0] == '.' && p + 1 < pe && p[1] == '*')) {
            p += *p == '*' ? 1 : 2;
            for (;; s++) {
                if (wild(p, pe, s))
                    return 1;
                if (!*s)
                    return 0;
            }
        }
        if (*p != *s)
            return 0;
        p++;
        s++;
    }
    return !*s;
}

int hooks_match(const char *m, const char *name)
{
    const char *cc = cfg_cc_tool(name);
    if (!m[0] || !strcmp(m, "*"))
        return 1;
    while (*m) {
        const char *e = m;
        while (*e && *e != '|')
            e++;
        if (wild(m, e, name) || wild(m, e, cc) || (!strcmp(cc, "Task") && wild(m, e, "Agent")))
            return 1;               /* "Agent": Claude Code's newer name of Task */
        m = *e ? e + 1 : e;
    }
    return 0;
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
           event == HK_POST_TOOL_FAILURE;
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
        project_cmd(h, k->cmd, line, sizeof(line) - 320);
        cl_cat(line, " < ", sizeof(line));
        cl_cat(line, file, sizeof(line));
        st = h->sys->run(h->sys->u, line, k->timeout_s, o, HOOK_OUT - 1, &on, &rc);
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
