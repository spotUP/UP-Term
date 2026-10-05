/* config -- see config.h. */
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "path.h"
#include "util.h"

const char *const cfg_hook_events[HK_COUNT] = {
    "PreToolUse", "PostToolUse", "UserPromptSubmit", "Stop", "SubagentStop", "SessionStart", "SessionEnd",
    "PreCompact", "Notification", "PermissionRequest", "PostToolUseFailure", "SubagentStart", "PostCompact",
    "StopFailure", "UserPromptExpansion", "CwdChanged", "DirectoryAdded", "PreModelSwitch", "PostModelSwitch",
    "InstructionsLoaded", "PostToolBatch", "ConfigChange", "Setup", "FileChanged", "MessageDisplay"
};

static char *dupn(const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (d) {
        memcpy(d, s, (size_t)n);
        d[n] = 0;
    }
    return d;
}

static char *dupz(const char *s)
{
    return dupn(s, (long)strlen(s));
}

void cfg_init(cl_settings *s)
{
    memset(s, 0, sizeof(*s));
    s->auto_compact = -1;
    s->web_search = -1;
    s->verbose = -1;
    s->auto_memory = -1;
    s->model_src = -1;
    s->cleanup_days = -1;
}

static void free_hook(cl_hook *k);

void cfg_drop_hooks(cl_settings *s)
{
    int i;
    for (i = 0; i < s->nhooks; i++)
        free_hook(&s->hooks[i]);
    s->nhooks = 0;
}

long cfg_window_parse(const char *v)
{
    long t = 0;
    const char *p = v;
    if (!strcmp(v, "auto"))
        return -1;
    while (*p >= '0' && *p <= '9' && t < 100000000L)
        t = t * 10 + (*p++ - '0');
    if (p == v)
        return 0;
    if (*p == 'k' || *p == 'K') {
        t *= 1000;
        p++;
    } else if (*p == 'm' || *p == 'M') {
        t *= 1000000L;
        p++;
    } else if (t >= 100 && t <= 1000)
        t *= 1000;              /* a bare 100..1000: thousands */
    if (*p || t < 100000L || t > 1000000L)
        return 0;
    return t;
}

void cfg_free(cl_settings *s)
{
    int i;
    for (i = 0; i < s->nrules; i++)
        free(s->rules[i].text);
    cfg_drop_hooks(s);
    for (i = 0; i < s->nmdx; i++)
        free(s->md_excludes[i]);
    free(s->md_excludes);
    for (i = 0; i < s->nenv; i++) {
        free(s->env[i].k);
        free(s->env[i].v);
    }
    for (i = 0; i < s->ndirs; i++)
        free(s->dirs[i]);
    for (i = 0; i < s->nsko; i++) {
        free(s->sk_over[i].k);
        free(s->sk_over[i].v);
    }
    free(s->sk_over);
    free(s->rules);
    free(s->hooks);
    free(s->env);
    free(s->dirs);
    cfg_init(s);
}

/* grows an array of n used, *cap allocated, by one element of size sz */
static int grow(void **p, int n, int *cap, size_t sz)
{
    void *q;
    int nc;
    if (n < *cap)
        return 0;
    nc = *cap ? *cap * 2 : 8;
    q = realloc(*p, (size_t)nc * sz);
    if (!q)
        return -1;
    *p = q;
    *cap = nc;
    return 0;
}

int cfg_add_rule(cl_settings *s, int kind, int src, const char *text)
{
    int i;
    for (i = 0; i < s->nrules; i++)
        if (s->rules[i].kind == kind && !strcmp(s->rules[i].text, text))
            return 0;
    if (grow((void **)&s->rules, s->nrules, &s->caprules, sizeof(cl_rule)))
        return -1;
    s->rules[s->nrules].kind = kind;
    s->rules[s->nrules].src = src;
    s->rules[s->nrules].text = dupz(text);
    if (!s->rules[s->nrules].text)
        return -1;
    s->nrules++;
    return 0;
}

const char *cfg_kind_name(int kind)
{
    return kind == RULE_DENY ? "deny" : kind == RULE_ASK ? "ask" : "allow";
}

static void str_into(jv v, char *out, long cap)
{
    if (json_type(v) == J_STR)
        json_str(v, out, cap);
}

static void rules_of(cl_settings *s, jv perm, const char *key, int kind, int src, const char *where)
{
    jv arr, e;
    jit it;
    if (!json_get(perm, key, &arr))
        return;
    if (json_type(arr) != J_ARR) {
        char w[120];
        cl_copy(w, "permissions.", sizeof(w));
        cl_cat(w, key, sizeof(w));
        cl_cat(w, " is not an array; skipped", sizeof(w));
        cfg_warn(s, where, w);
        return;
    }
    json_iter(arr, &it);
    while (json_next(&it, 0, &e)) {
        char r[300], tool[64], pat[300], w[400];
        if (json_type(e) != J_STR || json_str(e, r, sizeof(r)) < 1 ||
            cfg_rule_parse(r, tool, sizeof(tool), pat, sizeof(pat))) {
            /* Claude Code's Settings Warning: a malformed rule is skipped, the rest stays */
            cl_copy(w, "malformed permission rule in permissions.", sizeof(w));
            cl_cat(w, key, sizeof(w));
            if (json_type(e) == J_STR) {
                cl_cat(w, ": \"", sizeof(w));
                cl_cat(w, r, sizeof(w));
                cl_cat(w, "\"", sizeof(w));
            }
            cl_cat(w, " skipped", sizeof(w));
            cfg_warn(s, where, w);
            continue;
        }
        if (pat[0] && (!strcmp(tool, "Write") || !strcmp(tool, "MultiEdit") || !strcmp(tool, "NotebookEdit") ||
                       !strcmp(tool, "Glob"))) {
            /* Claude Code: path rules are Edit(...) and Read(...) only; such a rule is kept but never consulted */
            cl_copy(w, "the rule ", sizeof(w));
            cl_cat(w, r, sizeof(w));
            cl_cat(w, " is not matched by file permission checks: use ", sizeof(w));
            cl_cat(w, !strcmp(tool, "Glob") ? "Read(...)" : "Edit(...)", sizeof(w));
            cfg_warn(s, where, w);
        }
        if (kind == RULE_ALLOW && src == CFG_PROJECT && s->untrusted)
            continue;               /* Claude Code: a project's allow rules wait for workspace trust */
        cfg_add_rule(s, kind, src, r);
    }
}

void cfg_warn(cl_settings *s, const char *where, const char *what)
{
    if (s->warn[0])
        cl_cat(s->warn, "\n", sizeof(s->warn));
    cl_cat(s->warn, where && *where ? where : "settings", sizeof(s->warn));
    cl_cat(s->warn, ": ", sizeof(s->warn));
    cl_cat(s->warn, what, sizeof(s->warn));
    s->nwarn++;
}

/* the events Claude Code has that cannot happen here (no warning: the
 * entry is valid, it only never runs) */
static int known_elsewhere(const char *name)
{
    static const char *const ev[] = { "PermissionDenied", "TaskCreated", "TaskCompleted", "TeammateIdle",
                                      "WorktreeCreate", "WorktreeRemove", "Elicitation", "ElicitationResult", 0 };
    int i;
    for (i = 0; ev[i]; i++)
        if (!strcmp(name, ev[i]))
            return 1;
    return 0;
}

static void free_hook(cl_hook *k)
{
    free(k->matcher);
    free(k->cmd);
    free(k->cond);
    free(k->model);
    free(k->status);
    free(k->headers);
    free(k->env_ok);
}

void cfg_drop_owner(cl_settings *s, int owner)
{
    int i, k = 0;
    for (i = 0; i < s->nhooks; i++) {
        if (s->hooks[i].owner == owner) {
            free_hook(&s->hooks[i]);
            continue;
        }
        s->hooks[k++] = s->hooks[i];
    }
    s->nhooks = k;
}

int cfg_add_hooks(cl_settings *s, jv hooks, int src, int owner, const char *where)
{
    jit et;
    jv key, arr;
    int added = 0;
    /* an event name nobody knows is a warning (Claude Code's Settings Warning) */
    json_iter(hooks, &et);
    while (json_next(&et, &key, &arr)) {
        char name[64], w[160];
        int ev;
        json_str(key, name, sizeof(name));
        for (ev = 0; ev < HK_COUNT && strcmp(name, cfg_hook_events[ev]); ev++)
            ;
        if (ev == HK_COUNT) {
            if (!known_elsewhere(name)) {
                cl_copy(w, "unknown hook event \"", sizeof(w));
                cl_cat(w, name, sizeof(w));
                cl_cat(w, "\" skipped", sizeof(w));
                cfg_warn(s, where, w);
            }
            continue;
        }
        if (json_type(arr) != J_ARR) {
            cl_copy(w, name, sizeof(w));
            cl_cat(w, ": not an array of matcher groups; skipped", sizeof(w));
            cfg_warn(s, where, w);
            continue;
        }
        {
            jv grp;
            jit it;
            json_iter(arr, &it);
            while (json_next(&it, 0, &grp)) {
                jv m, hs, h, x;
                jit hi;
                char matcher[128];
                matcher[0] = 0;
                if (json_get(grp, "matcher", &m))
                    str_into(m, matcher, sizeof(matcher));
                if (!json_get(grp, "hooks", &hs) || json_type(hs) != J_ARR) {
                    cl_copy(w, name, sizeof(w));
                    cl_cat(w, ": a matcher group without a \"hooks\" array; skipped", sizeof(w));
                    cfg_warn(s, where, w);
                    continue;
                }
                json_iter(hs, &hi);
                while (json_next(&hi, 0, &h)) {
                    long cl;
                    char *cmd = 0, type[24];
                    cl_hook *k;
                    int kind, i, dflt;
                    type[0] = 0;
                    if (json_get(h, "type", &x))
                        str_into(x, type, sizeof(type));
                    kind = !strcmp(type, "prompt") ? HOOK_PROMPT : !strcmp(type, "http") ? HOOK_HTTP
                           : !strcmp(type, "agent") ? HOOK_AGENT : !strcmp(type, "command") || !type[0] ? HOOK_COMMAND
                           : -1;
                    if (kind < 0 || (kind == HOOK_AGENT && ev == HK_PERMISSION_REQUEST)) {
                        cl_copy(w, name, sizeof(w));
                        cl_cat(w, kind < 0 ? ": hook type \"" : ": an agent hook (not on PermissionRequest), type \"",
                               sizeof(w));
                        cl_cat(w, type, sizeof(w));
                        cl_cat(w, kind < 0 && !strcmp(type, "mcp_tool") ? "\" needs MCP, which is not on the Amiga; skipped"
                                                                      : "\" skipped", sizeof(w));
                        cfg_warn(s, where, w);
                        continue;
                    }
                    if (!json_get(h, kind == HOOK_COMMAND ? "command" : kind == HOOK_HTTP ? "url" : "prompt", &x) ||
                        json_type(x) != J_STR || (cmd = json_strdup(x, &cl)) == 0 || !cl) {
                        free(cmd);
                        cl_copy(w, name, sizeof(w));
                        cl_cat(w, kind == HOOK_COMMAND ? ": a command hook without \"command\"; skipped"
                                  : kind == HOOK_HTTP ? ": an http hook without \"url\"; skipped"
                                                      : ": a hook without \"prompt\"; skipped", sizeof(w));
                        cfg_warn(s, where, w);
                        continue;
                    }
                    for (i = 0; i < s->nhooks; i++)
                        if (s->hooks[i].event == ev && s->hooks[i].kind == kind && s->hooks[i].owner == owner &&
                            !strcmp(s->hooks[i].cmd, cmd) && !strcmp(s->hooks[i].matcher, matcher))
                            break;
                    if (i < s->nhooks) {
                        free(cmd);      /* Claude Code: the same handler from several files runs once */
                        continue;
                    }
                    if (grow((void **)&s->hooks, s->nhooks, &s->caphooks, sizeof(cl_hook))) {
                        free(cmd);
                        return added;
                    }
                    k = &s->hooks[s->nhooks];
                    memset(k, 0, sizeof(*k));
                    k->event = ev;
                    k->src = src;
                    k->owner = owner;
                    k->cmd = cmd;
                    k->kind = kind;
                    k->matcher = dupz(matcher);
                    /* Claude Code's defaults: 600 s (30 on UserPromptSubmit and the
                     * model switches, 10 on MessageDisplay), a prompt hook 30, an agent 60 */
                    dflt = kind == HOOK_PROMPT ? 30 : kind == HOOK_AGENT ? 60
                           : ev == HK_MESSAGE_DISPLAY ? 10
                           : ev == HK_PROMPT || ev == HK_PRE_MODEL_SWITCH || ev == HK_POST_MODEL_SWITCH ? 30 : 600;
                    k->timeout_s = json_get(h, "timeout", &x) ? (int)json_long(x, dflt) : dflt;
                    if (k->timeout_s <= 0)
                        k->timeout_s = dflt;
                    if (json_get(h, "if", &x) && json_type(x) == J_STR)
                        k->cond = json_strdup(x, &cl);
                    k->model = json_get(h, "model", &x) && json_type(x) == J_STR ? json_strdup(x, &cl) : 0;
                    k->status = json_get(h, "statusMessage", &x) && json_type(x) == J_STR ? json_strdup(x, &cl) : 0;
                    k->once = json_get(h, "once", &x) && json_type(x) == J_TRUE;
                    k->async = kind == HOOK_COMMAND && json_get(h, "async", &x) && json_type(x) == J_TRUE;
                    k->rewake = kind == HOOK_COMMAND && json_get(h, "asyncRewake", &x) && json_type(x) == J_TRUE;
                    if (k->rewake)
                        k->async = 1;
                    if (kind == HOOK_HTTP && json_get(h, "headers", &x) && json_type(x) == J_OBJ) {
                        k->headers = (char *)malloc((size_t)x.n + 1);
                        if (k->headers) {
                            memcpy(k->headers, x.p, (size_t)x.n);
                            k->headers[x.n] = 0;
                        }
                    }
                    if (kind == HOOK_HTTP && json_get(h, "allowedEnvVars", &x) && json_type(x) == J_ARR) {
                        jw l;
                        jv e;
                        jit ei;
                        jw_init(&l);
                        json_iter(x, &ei);
                        while (json_next(&ei, 0, &e)) {
                            char v[64];
                            if (json_type(e) != J_STR || json_str(e, v, sizeof(v)) < 1)
                                continue;
                            if (l.n)
                                jw_raw(&l, ",", 1);
                            jw_rawz(&l, v);
                        }
                        if (l.n)
                            k->env_ok = l.p;
                        else
                            jw_free(&l);
                    }
                    if (!k->matcher) {
                        free_hook(k);
                        return added;
                    }
                    s->nhooks++;
                    added++;
                }
            }
        }
    }
    return added;
}

static void hooks_of(cl_settings *s, jv hooks, int src, const char *where)
{
    cfg_add_hooks(s, hooks, src, 0, where);
}

static void env_of(cl_settings *s, jv env)
{
    jit it;
    jv k, v;
    json_iter(env, &it);
    while (json_next(&it, &k, &v)) {
        long kl, vl;
        char *ks, *vs;
        int i;
        if (json_type(v) != J_STR)
            continue;
        ks = json_strdup(k, &kl);
        vs = json_strdup(v, &vl);
        if (!ks || !vs) {
            free(ks);
            free(vs);
            return;
        }
        for (i = 0; i < s->nenv; i++)
            if (!strcmp(s->env[i].k, ks))
                break;
        if (i < s->nenv) {
            free(s->env[i].v);
            s->env[i].v = vs;
            free(ks);
            continue;
        }
        if (grow((void **)&s->env, s->nenv, &s->capenv, sizeof(cl_kv))) {
            free(ks);
            free(vs);
            return;
        }
        s->env[s->nenv].k = ks;
        s->env[s->nenv].v = vs;
        s->nenv++;
    }
}

/* the keys read from the user's settings and --settings only (Claude
 * Code's scope "User or managed": a project cannot remap keys or let
 * questions go on alone) */
static void user_keys(cl_settings *s, jv o)
{
    jv x;
    if (json_get(o, "vimInsertModeRemaps", &x) && json_type(x) == J_OBJ) {
        /* two printable characters -> "<Esc>"; anything else is ignored */
        jit it;
        jv k, v;
        long n = 0;
        json_iter(x, &it);
        while (json_next(&it, &k, &v) && n + 2 < (long)sizeof(s->vim_remaps)) {
            char key[8];
            if (json_type(k) != J_STR || json_str(k, key, sizeof(key)) != 2 || !json_streq(v, "<Esc>") ||
                (unsigned char)key[0] < 0x21 || (unsigned char)key[0] > 0x7e ||
                (unsigned char)key[1] < 0x21 || (unsigned char)key[1] > 0x7e)
                continue;
            s->vim_remaps[n++] = key[0];
            s->vim_remaps[n++] = key[1];
            s->vim_remaps[n] = 0;
        }
    }
    if (json_get(o, "askUserQuestionTimeout", &x)) {
        /* A4 gaps 3: "60s", "5m", "10m" or "never" (user settings and
         * --settings only, as Claude Code's scope "User or managed") */
        if (json_streq(x, "60s"))
            s->ask_timeout_ms = 60000L;
        else if (json_streq(x, "5m"))
            s->ask_timeout_ms = 300000L;
        else if (json_streq(x, "10m"))
            s->ask_timeout_ms = 600000L;
        else if (json_streq(x, "never"))
            s->ask_timeout_ms = 0;
    }
}

int cfg_merge(cl_settings *s, int src, const char *json, long n, const char *name)
{
    jv o, x, p;
    if (json_parse(json, n, &o) || json_type(o) != J_OBJ) {
        cl_copy(s->err, name, sizeof(s->err));
        cl_cat(s->err, " is not a JSON object; it was skipped", sizeof(s->err));
        return -1;
    }
    if (json_get(o, "model", &x)) {
        str_into(x, s->model, sizeof(s->model));
        s->model_src = src;
    }
    if (json_get(o, "effortLevel", &x) || json_get(o, "effort", &x))
        str_into(x, s->effort, sizeof(s->effort));
    if (json_get(o, "outputStyle", &x))
        str_into(x, s->output_style, sizeof(s->output_style));
    if (json_get(o, "editorMode", &x))
        str_into(x, s->editor_mode, sizeof(s->editor_mode));
    if (src == CFG_USER || src == CFG_SESSION)
        user_keys(s, o);
    if (json_get(o, "theme", &x))
        str_into(x, s->theme, sizeof(s->theme));
    if (json_get(o, "fallbackModel", &x))
        str_into(x, s->fallback_model, sizeof(s->fallback_model));
    if (json_get(o, "autoCompactEnabled", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->auto_compact = json_type(x) == J_TRUE;
    if (json_get(o, "webSearch", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->web_search = json_type(x) == J_TRUE;
    if (json_get(o, "statusLine", &x)) {
        jv c;
        if (json_type(x) == J_OBJ && json_get(x, "command", &c))
            str_into(c, s->status_cmd, sizeof(s->status_cmd));
        if (json_type(x) == J_OBJ && json_get(x, "padding", &c) && json_type(c) == J_NUM)
            s->status_pad = (int)json_long(c, 0);
        if (json_type(x) == J_OBJ && json_get(x, "refreshInterval", &c) && json_type(c) == J_NUM)
            s->status_refresh_s = (int)json_long(c, 0);
        if (s->status_pad < 0 || s->status_pad > 40)
            s->status_pad = 0;
        if (s->status_refresh_s < 0)
            s->status_refresh_s = 0;
    }
    if (json_get(o, "disableAllHooks", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->no_hooks = json_type(x) == J_TRUE;
    if (json_get(o, "verbose", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->verbose = json_type(x) == J_TRUE;
    if (json_get(o, "autoMemoryEnabled", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->auto_memory = json_type(x) == J_TRUE;
    if (json_get(o, "agent", &x))
        str_into(x, s->agent, sizeof(s->agent));
    if (json_get(o, "apiKeyHelper", &x))
        str_into(x, s->key_helper, sizeof(s->key_helper));
    if (json_get(o, "bashOutputMaxChars", &x) && json_type(x) == J_NUM)
        s->bash_max_chars = json_long(x, 0);
    if (json_get(o, "cleanupPeriodDays", &x) && json_type(x) == J_NUM)
        s->cleanup_days = (int)json_long(x, -1);
    if (json_get(o, "availableModels", &x) && json_type(x) == J_ARR) {
        jit it;
        jv e;
        s->avail_models[0] = 0;
        json_iter(x, &it);
        while (json_next(&it, 0, &e)) {
            char m[64];
            if (json_type(e) != J_STR || json_str(e, m, sizeof(m)) < 1)
                continue;
            if (s->avail_models[0])
                cl_cat(s->avail_models, ",", sizeof(s->avail_models));
            cl_cat(s->avail_models, m, sizeof(s->avail_models));
        }
    }
    if (json_get(o, "autoCompactWindow", &x)) {
        long w = 0;
        if (json_type(x) == J_NUM)
            w = json_long(x, 0);
        else if (json_type(x) == J_STR) {
            char b[24];
            json_str(x, b, sizeof(b));
            w = cfg_window_parse(b);
        }
        s->compact_window = w > 0 ? w : 0;
    }
    if (json_get(o, "statusLine", &x) && json_type(x) == J_OBJ) {
        jv c;
        if (json_get(x, "hideVimModeIndicator", &c))
            s->hide_vim = json_type(c) == J_TRUE;
    }
    if (json_get(o, "claudeMdExcludes", &x) && json_type(x) == J_ARR) {
        jit it;
        jv e;
        json_iter(x, &it);
        while (json_next(&it, 0, &e)) {
            long l;
            char *g = json_type(e) == J_STR ? json_strdup(e, &l) : 0;
            if (!g)
                continue;
            if (grow((void **)&s->md_excludes, s->nmdx, &s->capmdx, sizeof(char *))) {
                free(g);
                break;
            }
            s->md_excludes[s->nmdx++] = g;
        }
    }
    if (json_get(o, "env", &x) && json_type(x) == J_OBJ)
        env_of(s, x);
    if (json_get(o, "hooks", &x) && json_type(x) == J_OBJ)
        hooks_of(s, x, src, name);
    else if (json_get(o, "hooks", &x))
        cfg_warn(s, name, "\"hooks\" is not an object; skipped");
    if (json_get(o, "disableSkillShellExecution", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->no_skill_shell = json_type(x) == J_TRUE;
    if (json_get(o, "advisorModel", &x))
        str_into(x, s->advisor, sizeof(s->advisor));
    if (json_get(o, "skillOverrides", &x) && json_type(x) == J_OBJ) {
        jit it;
        jv k, v;
        json_iter(x, &it);
        while (json_next(&it, &k, &v)) {
            char key[64], val[24];
            int i;
            json_str(k, key, sizeof(key));
            val[0] = 0;
            if (json_type(v) == J_STR)
                json_str(v, val, sizeof(val));
            if (strcmp(val, "on") && strcmp(val, "name-only") && strcmp(val, "user-invocable-only") &&
                strcmp(val, "off")) {
                char w[160];
                cl_copy(w, "skillOverrides.", sizeof(w));
                cl_cat(w, key, sizeof(w));
                cl_cat(w, " is not on, name-only, user-invocable-only or off; skipped", sizeof(w));
                cfg_warn(s, name, w);
                continue;
            }
            for (i = 0; i < s->nsko && strcmp(s->sk_over[i].k, key); i++)
                ;
            if (i == s->nsko) {
                if (grow((void **)&s->sk_over, s->nsko, &s->capsko, sizeof(cl_kv)))
                    break;
                s->sk_over[i].k = dupz(key);
                s->sk_over[i].v = 0;
                s->nsko++;
            }
            free(s->sk_over[i].v);
            s->sk_over[i].v = dupz(val);
        }
    }
    if (json_get(o, "permissions", &p) && json_type(p) == J_OBJ) {
        rules_of(s, p, "allow", RULE_ALLOW, src, name);
        rules_of(s, p, "ask", RULE_ASK, src, name);
        rules_of(s, p, "deny", RULE_DENY, src, name);
        if (json_get(p, "defaultMode", &x)) {
            char m[24];
            m[0] = 0;
            str_into(x, m, sizeof(m));
            /* Claude Code: bypassPermissions and auto do not take effect from
             * a project's files (a cloned repository could set them) */
            if ((src == CFG_PROJECT || src == CFG_LOCAL) && (!strcmp(m, "bypassPermissions") || !strcmp(m, "auto")))
                ;
            else if (m[0])
                cl_copy(s->default_mode, m, sizeof(s->default_mode));
        }
        if (json_get(p, "additionalDirectories", &x) && json_type(x) == J_ARR && !(src == CFG_PROJECT && s->untrusted)) {
            jit it;
            jv e;
            json_iter(x, &it);
            while (json_next(&it, 0, &e)) {
                long l;
                char *d = json_strdup(e, &l);
                if (!d)
                    continue;
                if (grow((void **)&s->dirs, s->ndirs, &s->capdirs, sizeof(char *))) {
                    free(d);
                    break;
                }
                s->dirs[s->ndirs++] = d;
            }
        }
    }
    return 0;
}

const char *cfg_file(const cl_settings *s, int src)
{
    return src >= 0 && src < CFG_NSRC ? s->path[src] : "";
}

int cfg_load(cl_settings *s, cl_sys *sys, const char *home, const char *root)
{
    static const char *const rel[3] = { "settings.json", ".claude/settings.json", ".claude/settings.local.json" };
    int i, got = 0;
    for (i = 0; i < 3; i++) {
        char *b = 0;
        long n = 0;
        if (path_join(i == CFG_USER ? home : root, rel[i], s->path[i], sizeof(s->path[i])))
            continue;
        if (s->skip & (1u << i))
            continue;               /* --setting-sources leaves it out */
        if (sys->kind(sys->u, s->path[i]) != 1 || sys->read(sys->u, s->path[i], 256L * 1024, &b, &n))
            continue;
        if (cfg_merge(s, i, b, n, s->path[i]) == 0) {
            s->found[i] = 1;
            got++;
        }
        free(b);
    }
    return got;
}

/* ---- writing ---- */

/* obj with key set to value (raw JSON) or removed (value 0), into w, one
 * member per line */
static void obj_with(jv obj, const char *key, const char *value, jw *w, const char *ind)
{
    jit it;
    jv k, v;
    int first = 1, done = 0;
    jw_raw(w, "{", 1);
    if (json_type(obj) == J_OBJ) {
        json_iter(obj, &it);
        while (json_next(&it, &k, &v)) {
            int same = json_streq(k, key);
            if (same && (!value || done))
                continue;
            jw_rawz(w, first ? "\n" : ",\n");
            jw_rawz(w, ind);
            jw_rawz(w, "  ");
            jw_raw(w, k.p, k.n);
            jw_rawz(w, ": ");
            if (same) {
                jw_rawz(w, value);
                done = 1;
            } else
                jw_raw(w, v.p, v.n);
            first = 0;
        }
    }
    if (value && !done) {
        jw_rawz(w, first ? "\n" : ",\n");
        jw_rawz(w, ind);
        jw_rawz(w, "  ");
        jw_strz(w, key);
        jw_rawz(w, ": ");
        jw_rawz(w, value);
        first = 0;
    }
    if (!first) {
        jw_raw(w, "\n", 1);
        jw_rawz(w, ind);
    }
    jw_raw(w, "}", 1);
}

/* the file's object, "{}" when it is missing; -1 when it is there but
 * not an object (it is not overwritten then) */
static int read_obj(cl_sys *sys, const char *file, char **buf, jv *o)
{
    long n = 0;
    *buf = 0;
    if (sys->kind(sys->u, file) != 1) {
        o->p = "{}";
        o->n = 2;
        return 0;
    }
    if (sys->read(sys->u, file, 256L * 1024, buf, &n))
        return -1;
    if (json_parse(*buf, n, o) || json_type(*o) != J_OBJ) {
        free(*buf);
        *buf = 0;
        return -1;
    }
    return 0;
}

/* the directory part of a file name made, when it is missing (.claude/) */
static void make_parent(cl_sys *sys, const char *file)
{
    char d[300];
    if (sys->mkdir && path_parent(file, d, sizeof(d)) == 0 && sys->kind(sys->u, d) == 0)
        sys->mkdir(sys->u, d);
}

int cfg_write_key(cl_sys *sys, const char *file, const char *key, const char *value)
{
    char *b;
    jv o;
    jw w;
    int rc;
    if (read_obj(sys, file, &b, &o))
        return -1;
    jw_init(&w);
    obj_with(o, key, value, &w, "");
    jw_raw(&w, "\n", 1);
    free(b);
    make_parent(sys, file);
    rc = w.oom ? -1 : sys->write(sys->u, file, w.p, w.n);
    jw_free(&w);
    return rc;
}

int cfg_write_sub(cl_sys *sys, const char *file, const char *obj, const char *key, const char *value)
{
    char *b;
    jv o, sub;
    jw w, s;
    int rc;
    if (read_obj(sys, file, &b, &o))
        return -1;
    if (!json_get(o, obj, &sub) || json_type(sub) != J_OBJ) {
        sub.p = "{}";
        sub.n = 2;
    }
    jw_init(&s);
    obj_with(sub, key, value, &s, "  ");
    jw_init(&w);
    if (!s.oom) {
        /* the inner object written whole as the outer key's value */
        while (s.n && (s.p[s.n - 1] == '\n' || s.p[s.n - 1] == ' '))
            s.p[--s.n] = 0;
        obj_with(o, obj, s.p, &w, "");
    }
    jw_raw(&w, "\n", 1);
    free(b);
    make_parent(sys, file);
    rc = w.oom || s.oom ? -1 : sys->write(sys->u, file, w.p, w.n);
    jw_free(&w);
    jw_free(&s);
    return rc;
}

int cfg_write_rule(cl_sys *sys, const char *file, int kind, const char *rule, int add)
{
    char *b;
    jv o, p, arr, e;
    jit it;
    jw na, np, w;
    int rc = 0, found = 0, first = 1;
    const char *kn = cfg_kind_name(kind);
    if (read_obj(sys, file, &b, &o))
        return -1;
    if (!json_get(o, "permissions", &p) || json_type(p) != J_OBJ) {
        p.p = "{}";
        p.n = 2;
    }
    jw_init(&na);
    jw_raw(&na, "[", 1);
    if (json_get(p, kn, &arr) && json_type(arr) == J_ARR) {
        json_iter(arr, &it);
        while (json_next(&it, 0, &e)) {
            if (json_streq(e, rule)) {
                found = 1;
                if (!add)
                    continue;
            }
            if (!first)
                jw_rawz(&na, ", ");
            jw_raw(&na, e.p, e.n);
            first = 0;
        }
    }
    if (add && !found) {
        if (!first)
            jw_rawz(&na, ", ");
        jw_strz(&na, rule);
    }
    jw_raw(&na, "]", 1);
    jw_init(&np);
    obj_with(p, kn, na.p, &np, "  ");
    jw_init(&w);
    obj_with(o, "permissions", np.p, &w, "");
    jw_raw(&w, "\n", 1);
    free(b);
    if (!add && !found)
        rc = -1;
    else if (na.oom || np.oom || w.oom)
        rc = -1;
    else {
        make_parent(sys, file);
        rc = sys->write(sys->u, file, w.p, w.n);
    }
    jw_free(&na);
    jw_free(&np);
    jw_free(&w);
    return rc;
}

/* ---- rules ---- */

int cfg_rule_parse(const char *rule, char *tool, long tcap, char *pat, long pcap)
{
    const char *o = strchr(rule, '(');
    long tl = o ? (long)(o - rule) : (long)strlen(rule);
    while (tl && rule[tl - 1] == ' ')
        tl--;
    if (!tl || tl >= tcap)
        return -1;
    memcpy(tool, rule, (size_t)tl);
    tool[tl] = 0;
    pat[0] = 0;
    if (o) {
        long n = (long)strlen(o + 1);
        if (!n || o[n] != ')')
            return -1;
        n--;
        if (n >= pcap)
            return -1;
        memcpy(pat, o + 1, (size_t)n);
        pat[n] = 0;
        if (!strcmp(pat, "*"))
            pat[0] = 0;
    }
    return 0;
}

const char *cfg_cc_tool(const char *name)
{
    static const char *const map[][2] = {
        { "read_file", "Read" }, { "list_dir", "LS" }, { "grep", "Grep" }, { "write_file", "Write" },
        { "edit_file", "Edit" }, { "run_command", "Bash" }, { "todo_write", "TodoWrite" }, { 0, 0 }
    };
    int i;
    for (i = 0; map[i][0]; i++)
        if (!strcmp(name, map[i][0]))
            return map[i][1];
    return name;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int cfg_glob(const char *p, const char *s, int icase)
{
    while (*p) {
        if (p[0] == '*' && p[1] == '*') {
            p += 2;
            if (*p == '/')
                p++;        /* "**" and "**" + "/" match zero or more whole directories */
            for (;; s++) {
                if (cfg_glob(p, s, icase))
                    return 1;
                if (!*s)
                    return 0;
            }
        }
        if (*p == '*' || (p[0] == '#' && p[1] == '?')) {
            p += *p == '*' ? 1 : 2;
            for (;; s++) {
                if (cfg_glob(p, s, icase))
                    return 1;
                if (!*s || *s == '/')
                    return 0;
            }
        }
        if (!*s)
            return 0;
        if (*p == '?') {
            if (*s == '/')
                return 0;
        } else if (*p == '[') {
            const char *q = p + 1;
            int neg = *q == '!' || *q == '^', hit = 0;
            if (neg)
                q++;
            while (*q && *q != ']') {
                int a = (unsigned char)*q, b = a;
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    b = (unsigned char)q[2];
                    q += 2;
                }
                if ((unsigned char)*s >= a && (unsigned char)*s <= b)
                    hit = 1;
                if (icase && lower((unsigned char)*s) >= lower(a) && lower((unsigned char)*s) <= lower(b))
                    hit = 1;
                q++;
            }
            if (!*q || hit == neg)
                return 0;
            p = q;
        } else if (icase ? lower((unsigned char)*p) != lower((unsigned char)*s) : *p != *s)
            return 0;
        p++;
        s++;
    }
    return !*s;
}

/* a string member of the input, decoded */
static int in_str(jv in, const char *key, char *out, long cap)
{
    jv x;
    out[0] = 0;
    return json_get(in, key, &x) && json_type(x) == J_STR && json_str(x, out, cap) >= 0;
}

/* is the tool one the rule's tool covers? */
static int tool_covers(const char *rt, const char *ct)
{
    if (!strcmp(rt, ct))
        return 1;
    if (!strcmp(rt, "Agent") || !strcmp(rt, "Task"))
        return !strcmp(ct, "Task") || !strcmp(ct, "Agent");    /* Claude Code's newer name of Task */
    if (!strcmp(rt, "Edit"))
        return !strcmp(ct, "Write") || !strcmp(ct, "MultiEdit") || !strcmp(ct, "NotebookEdit");
    if (!strcmp(rt, "Bash"))
        return !strcmp(ct, "Monitor");     /* Claude Code: Monitor's command goes by Bash's rules */
    if (!strcmp(rt, "TaskStop") || !strcmp(rt, "KillShell"))
        return !strcmp(ct, "TaskStop") || !strcmp(ct, "KillShell");
    if (!strcmp(rt, "Read"))
        return !strcmp(ct, "LS") || !strcmp(ct, "Grep") || !strcmp(ct, "Glob");
    return 0;
}

static int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

/* a command pattern: * is any text, / included */
static int wild(const char *p, const char *s)
{
    while (*p) {
        if (*p == '*') {
            while (*p == '*')
                p++;
            for (;; s++) {
                if (wild(p, s))
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

/* one simple command against a Bash pattern */
static int bash_one(const char *pat, const char *cmd, long n)
{
    char c[1024], p[300];
    long pl = (long)strlen(pat);
    while (n && is_space((unsigned char)*cmd)) {
        cmd++;
        n--;
    }
    while (n && is_space((unsigned char)cmd[n - 1]))
        n--;
    if (n >= (long)sizeof(c) || pl >= (long)sizeof(p))
        return 0;
    memcpy(c, cmd, (size_t)n);
    c[n] = 0;
    cl_copy(p, pat, sizeof(p));
    if (pl >= 2 && p[pl - 2] == ':' && p[pl - 1] == '*') {
        /* the legacy prefix form: "make:*" is "make" or "make ..." */
        p[pl - 2] = 0;
        pl -= 2;
        return !strncmp(c, p, (size_t)pl) && (!c[pl] || is_space((unsigned char)c[pl]));
    }
    if (strchr(p, '*')) {
        /* "make *" also covers a bare "make" */
        if (pl >= 2 && p[pl - 2] == ' ' && p[pl - 1] == '*' && !strncmp(c, p, (size_t)pl - 2) && !c[pl - 2])
            return 1;
        return wild(p, c);
    }
    return !strcmp(c, p);
}

/* a compound line: for an allow every part must match (all 1); for a
 * deny or an ask any part (all 0) */
static int bash_match(const char *pat, const char *cmd, int all)
{
    const char *s = cmd, *e;
    int any = 0, every = 1, parts = 0;
    for (;;) {
        for (e = s; *e && *e != ';' && *e != '|' && *e != '&' && *e != '\n'; e++)
            ;
        if (e > s) {
            long n = (long)(e - s), k = 0;
            while (k < n && is_space((unsigned char)s[k]))
                k++;
            if (k < n) {
                int m = bash_one(pat, s, n);
                parts++;
                any |= m;
                every &= m;
            }
        }
        if (!*e)
            break;
        s = e + 1;
        while (*s == '|' || *s == '&')
            s++;
    }
    /* one simple command: the pattern against the whole of it */
    if (parts <= 1)
        return bash_one(pat, cmd, (long)strlen(cmd));
    return all ? every : any;
}

/* a path pattern against a call's path, both resolved against root */
static char home_dir[256] = "SYS:";

void cfg_set_home(const char *dir)
{
    cl_copy(home_dir, dir && *dir ? dir : "SYS:", sizeof(home_dir));
}

static int path_match(const char *pat, const char *path, const char *root)
{
    char full[600], pp[600];
    const char *p = pat;
    if (p[0] == '~' && p[1] == '/') {
        /* ~/x: from the user's home (HOME, else SYS:), as Claude Code's */
        cl_copy(pp, home_dir, sizeof(pp));
        if (pp[0] && pp[strlen(pp) - 1] != ':' && pp[strlen(pp) - 1] != '/')
            cl_cat(pp, "/", sizeof(pp));
        cl_cat(pp, p + 2, sizeof(pp));
    } else if (!strchr(p, ':')) {
        if (!strncmp(p, "./", 2))
            p += 2;
        else if (p[0] == '/' && p[1] == '/')
            p += 2;
        else if (p[0] == '/')
            p += 1;
        else if (!strchr(p, '/')) {
            /* a bare name: at any depth */
            cl_copy(pp, root, sizeof(pp));
            cl_cat(pp, pp[0] && pp[strlen(pp) - 1] != ':' && pp[strlen(pp) - 1] != '/' ? "/**/" : "**/",
                   sizeof(pp));
            cl_cat(pp, p, sizeof(pp));
            p = 0;
        }
        if (p) {
            cl_copy(pp, root, sizeof(pp));
            if (pp[0] && pp[strlen(pp) - 1] != ':' && pp[strlen(pp) - 1] != '/')
                cl_cat(pp, "/", sizeof(pp));
            cl_cat(pp, p, sizeof(pp));
        }
    } else
        cl_copy(pp, p, sizeof(pp));
    if (path_join(root, path, full, sizeof(full)))
        return 0;
    if (cfg_glob(pp, full, 1))
        return 1;
    /* a directory pattern covers what is below it, at any depth */
    cl_cat(pp, "/**", sizeof(pp));
    return cfg_glob(pp, full, 1);
}

static int rule_match(const char *rule, const char *tool, jv in, const char *root, int kind)
{
    char rt[64], pat[300], v[1024];
    const char *ct = cfg_cc_tool(tool), *rcc;
    int all = kind == RULE_ALLOW;
    if (cfg_rule_parse(rule, rt, sizeof(rt), pat, sizeof(pat)))
        return 0;
    rcc = cfg_cc_tool(rt);
    /* Claude Code: an Edit allow rule grants Read on its paths too; a Read
     * deny rule blocks Edit and Write there as well */
    if (!tool_covers(rcc, ct) && !(kind == RULE_ALLOW && !strcmp(rcc, "Edit") && tool_covers("Read", ct)) &&
        !(kind == RULE_DENY && !strcmp(rcc, "Read") && (tool_covers("Edit", ct) || !strcmp(ct, "Edit"))))
        return 0;
    if (!pat[0])
        return 1;
    if (!strcmp(ct, "Bash") || !strcmp(ct, "Monitor"))
        return in_str(in, "command", v, sizeof(v)) && bash_match(pat, v, all);
    if (!strcmp(ct, "WebFetch")) {
        const char *h, *e;
        char host[256];
        long hl;
        if (strncmp(pat, "domain:", 7) || !in_str(in, "url", v, sizeof(v)))
            return 0;
        h = strstr(v, "://");
        h = h ? h + 3 : v;
        for (e = h; *e && *e != '/' && *e != ':' && *e != '?'; e++)
            ;
        hl = (long)(e - h);
        if (hl >= (long)sizeof(host))
            return 0;
        memcpy(host, h, (size_t)hl);
        host[hl] = 0;
        if (cl_strieq(host, pat + 7))
            return 1;
        /* a subdomain of it */
        hl = (long)strlen(host) - (long)strlen(pat + 7);
        return hl > 0 && host[hl - 1] == '.' && cl_strieq(host + hl, pat + 7);
    }
    if (tool_covers("Read", ct) || tool_covers("Edit", ct)) {
        if (!in_str(in, "file_path", v, sizeof(v)) && !in_str(in, "path", v, sizeof(v)) &&
            !in_str(in, "notebook_path", v, sizeof(v)))
            cl_copy(v, "", sizeof(v));
        return path_match(pat, v, root);
    }
    /* another tool: its first naming input */
    if (in_str(in, "subagent_type", v, sizeof(v)) || in_str(in, "skill", v, sizeof(v)) ||
        in_str(in, "command", v, sizeof(v)) || in_str(in, "url", v, sizeof(v)) ||
        in_str(in, "query", v, sizeof(v)))
        return cfg_glob(pat, v, 0);
    return 0;
}

int cfg_rule_match(const char *rule, const char *tool, jv input, const char *root)
{
    return rule_match(rule, tool, input, root, RULE_ALLOW);
}

int cfg_rule_match_any(const char *rule, const char *tool, jv input, const char *root)
{
    return rule_match(rule, tool, input, root, RULE_ASK);
}

int cfg_decide(const cl_settings *s, const char *tool, jv input, const char *root, const cl_rule **which)
{
    static const int order[3] = { RULE_DENY, RULE_ASK, RULE_ALLOW };
    int k, i;
    if (which)
        *which = 0;
    for (k = 0; k < 3; k++)
        for (i = 0; i < s->nrules; i++)
            if (s->rules[i].kind == order[k] &&
                rule_match(s->rules[i].text, tool, input, root, order[k])) {
                if (which)
                    *which = &s->rules[i];
                return order[k];
            }
    return RULE_NONE;
}

const char *cfg_skill_state(const cl_settings *s, const char *skill)
{
    int i;
    for (i = 0; i < s->nsko; i++)
        if (!strcmp(s->sk_over[i].k, skill) && s->sk_over[i].v)
            return s->sk_over[i].v;
    return "on";
}

int cfg_web_search(const cl_settings *s)
{
    int i;
    if (s->web_search == 0)
        return 0;
    for (i = 0; i < s->nrules; i++) {
        char tool[40], pat[200];
        if (s->rules[i].kind == RULE_DENY &&
            cfg_rule_parse(s->rules[i].text, tool, sizeof(tool), pat, sizeof(pat)) == 0 &&
            !strcmp(tool, "WebSearch") && (!pat[0] || !strcmp(pat, "*")))
            return 0;
    }
    return 1;
}

const char *cfg_model(const char *name)
{
    static const char *const al[][2] = {
        { "opus", "claude-opus-5-5" },  { "sonnet", "claude-sonnet-5-5" },
        { "haiku", "claude-haiku-4-5-20251001" }, { "fable", "claude-fable-5-1" },
        { "default", "claude-opus-5-5" }, { "opusplan", "claude-opus-5-5" },
        { "best", "claude-fable-5-1" }, { 0, 0 }
    };
    int i;
    for (i = 0; al[i][0]; i++)
        if (cl_strieq(name, al[i][0]))
            return al[i][1];
    return name;
}
