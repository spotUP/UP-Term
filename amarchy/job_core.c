/* The Amarchy job view core (job_core.h). */
#include <string.h>
#include "job_core.h"

#define MARKER "@JOBRC"
#define PROGRESS "[PROGRESS "

/* One table for the upkg exit codes (upterm-ports pkgs/opkg/doc/upkg-api.txt). ok = counts as Done. */
static const struct { int rc, ok; const char *word; } rc_words[] = {
    { 0,  1, "Done" },
    { 5,  1, "Nothing to do" },
    { 11, 0, "Usage error" },
    { 12, 0, "Package not found" },
    { 13, 0, "Network error" },
    { 14, 0, "Package script failed" },
    { 15, 0, "Needs answers: run it from a terminal" },
    { 20, 0, "Failed" },
};
#define NRC ((int)(sizeof rc_words / sizeof rc_words[0]))

void jc_init(jc *j)
{
    memset(j, 0, sizeof *j);
    j->state = JC_RUNNING;
    j->follow = 1;
}

static void put_num(char *out, int cap, const char *pre, int v, const char *post)
{
    char d[12];
    int n = 0, len = 0;
    unsigned u = v < 0 ? (unsigned)-v : (unsigned)v;
    do { d[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    #define PUT(c) do { if (len < cap - 1) out[len++] = (c); } while (0)
    while (*pre) PUT(*pre++);
    if (v < 0) PUT('-');
    while (n) PUT(d[--n]);
    while (*post) PUT(*post++);
    out[len] = 0;
    #undef PUT
}

static void set_rc(jc *j, int rc)
{
    int i;
    j->ended = 1;
    j->rc = rc;
    if (j->state != JC_RUNNING) return;      /* cancelled stays cancelled */
    for (i = 0; i < NRC; i++)
        if (rc_words[i].rc == rc) { j->state = rc_words[i].ok ? JC_DONE : JC_FAILED; return; }
    j->state = JC_FAILED;
}

/* "[PROGRESS n/m] rest": keep n, m and rest. */
static void take_progress(jc *j, const char *s)
{
    int n = 0, m = 0, k = (int)strlen(PROGRESS);
    if (strncmp(s, PROGRESS, k)) return;
    s += k;
    if (*s < '0' || *s > '9') return;
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s++ != '/') return;
    if (*s < '0' || *s > '9') return;
    while (*s >= '0' && *s <= '9') m = m * 10 + *s++ - '0';
    if (*s++ != ']') return;
    while (*s == ' ') s++;
    j->prog_n = n;
    j->prog_m = m;
    strcpy(j->prog_text, s);
}

/* Position of an "@JOBRC <n>" that runs to the end of s, or -1. At the line start the number may be missing
   (rc 0); after other text (output that ended without a newline) the number is required. */
static int find_marker(const char *s, int *rc)
{
    int k, len = (int)strlen(s);
    for (k = 0; k + 6 <= len; k++) {
        const char *p = s + k;
        int v = 0, neg = 0, digits = 0;
        if (strncmp(p, MARKER, 6)) continue;
        p += 6;
        if (*p && *p != ' ') continue;
        while (*p == ' ') p++;
        if (*p == '-') neg = 1, p++;
        while (*p >= '0' && *p <= '9') v = v * 10 + *p++ - '0', digits++;
        if (*p || (k > 0 && !digits)) continue;
        *rc = neg ? -v : v;
        return k;
    }
    return -1;
}

static void add_line(jc *j)
{
    take_progress(j, j->cur);
    memcpy(j->line[j->total % JOB_MAX_LINES], j->cur, (size_t)j->curlen + 1);
    j->total++;
}

static void end_line(jc *j)
{
    int rc = 0, k;
    j->cur[j->curlen] = 0;
    k = find_marker(j->cur, &rc);
    if (k >= 0) {
        if (k > 0) { j->cur[k] = 0; j->curlen = k; add_line(j); }
        set_rc(j, rc);
    } else
        add_line(j);
    j->curlen = j->curcol = 0;
}

static void put_char(jc *j, int c)
{
    if (c == '\t') {
        int to = (j->curcol / 8 + 1) * 8;
        while (j->curcol < to) {
            if (j->curlen < JOB_LINE_MAX) j->cur[j->curlen++] = ' ';
            j->curcol++;
        }
        return;
    }
    if (c < 32) return;
    if (j->curlen < JOB_LINE_MAX) j->cur[j->curlen++] = (char)c;
    j->curcol++;
}

void jc_feed(jc *j, const char *buf, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        int c = (unsigned char)buf[i];
        if (j->pending_cr) {
            j->pending_cr = 0;
            if (c == '\n') continue;
        }
        if (c == '\r') { j->pending_cr = 1; end_line(j); }
        else if (c == '\n') end_line(j);
        else put_char(j, c);
    }
}

void jc_cancel(jc *j)
{
    if (j->state == JC_RUNNING && !j->ended) j->state = JC_CANCELLED;
}

int jc_count(const jc *j) { return j->total > JOB_MAX_LINES ? JOB_MAX_LINES : (int)j->total; }
long jc_dropped(const jc *j) { return j->total > JOB_MAX_LINES ? j->total - JOB_MAX_LINES : 0; }

const char *jc_line(const jc *j, int i)
{
    if (i < 0 || i >= jc_count(j)) return "";
    return j->line[(jc_dropped(j) + i) % JOB_MAX_LINES];
}

void jc_footer(const jc *j, char *out, int cap)
{
    int i;
    const char *w = 0;
    if (cap <= 0) return;
    if (j->state == JC_RUNNING) w = "Running...";
    else if (j->state == JC_CANCELLED) w = "Cancelled";
    else for (i = 0; i < NRC; i++)
        if (rc_words[i].rc == j->rc) w = rc_words[i].word;
    if (!w) { put_num(out, cap, "Failed (code ", j->rc, ")"); return; }
    strncpy(out, w, (size_t)cap - 1);
    out[cap - 1] = 0;
}

static int max_top(const jc *j, int h)
{
    int m = jc_count(j) - h;
    return m > 0 ? m : 0;
}

int jc_top(const jc *j, int h)
{
    long t;
    if (j->follow) return max_top(j, h);
    t = j->scroll_abs - jc_dropped(j);
    if (t < 0) t = 0;
    if (t > max_top(j, h)) t = max_top(j, h);
    return (int)t;
}

void jc_scroll(jc *j, int delta, int h)
{
    int t = jc_top(j, h) + delta, m = max_top(j, h);
    if (t < 0) t = 0;
    if (t >= m) { j->follow = 1; return; }
    j->follow = 0;
    j->scroll_abs = jc_dropped(j) + t;
}

void jc_header(const jc *j, char *out, int cap)
{
    char n[12], m[12];
    int len = 0;
    if (cap <= 0) return;
    out[0] = 0;
    if (!j->prog_m) return;
    put_num(n, (int)sizeof n, "", j->prog_n, "");
    put_num(m, (int)sizeof m, "", j->prog_m, "");
    #define PUTS(s) do { const char *p_ = (s); while (*p_ && len < cap - 1) out[len++] = *p_++; } while (0)
    PUTS(n); PUTS(" of "); PUTS(m);
    if (j->prog_text[0]) { PUTS(" "); PUTS(j->prog_text); }
    out[len] = 0;
    #undef PUTS
}

long jc_first_new(const jc *j, long seen)
{
    long d = jc_dropped(j);
    return seen < d ? d : seen;
}

int jc_tone(const jc *j)
{
    if (j->state == JC_RUNNING) return JT_DIM;
    return j->state == JC_DONE ? JT_OK : JT_FAIL;
}

int jc_key_act(const jc *j, int key, int h, int *delta)
{
    *delta = 0;
    switch (key) {
    case JKEY_UP: *delta = -1; return JA_SCROLL;
    case JKEY_DOWN: *delta = 1; return JA_SCROLL;
    case JKEY_PGUP: *delta = -(h > 1 ? h - 1 : 1); return JA_SCROLL;
    case JKEY_PGDN: *delta = h > 1 ? h - 1 : 1; return JA_SCROLL;
    case JKEY_ESC: return j->state == JC_RUNNING ? JA_CANCEL : JA_CLOSE;
    case JKEY_ENTER: return j->state == JC_RUNNING ? JA_NONE : JA_CLOSE;
    }
    return JA_NONE;
}
