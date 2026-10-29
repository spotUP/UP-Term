/* The Amarchy notice queue, the portable core (note_core.h). */
#include <string.h>
#include "note_core.h"

void note_init(note_q *q)
{
    memset(q, 0, sizeof *q);
    q->next_id = 1;
}

/* Copy one field of line[*pos..] up to an unescaped '|' into out (cap bytes, cut when longer); *pos stops on
 * the '|' or the end. */
static void field(const char *line, long *pos, char *out, int cap)
{
    int n = 0;
    long i = *pos;
    while (line[i] && line[i] != '|') {
        char c = line[i];
        if (c == '\\' && (line[i + 1] == '|' || line[i + 1] == '\\')) {
            c = line[i + 1];
            i++;
        }
        if (n < cap - 1)
            out[n++] = c;
        i++;
    }
    out[n] = 0;
    *pos = i;
}

int note_parse(const char *line, char *title, char *body, long *out_secs)
{
    long pos = 0;
    char secs[16];
    int i;
    long v = 0;
    title[0] = body[0] = 0;
    *out_secs = -1;
    field(line, &pos, title, NOTE_TITLE);
    if (title[0] == 0)
        return NOTE_ERR_EMPTY;
    if (line[pos] == '|') {
        pos++;
        field(line, &pos, body, NOTE_BODY);
    }
    if (line[pos] == '|') {
        pos++;
        field(line, &pos, secs, (int)sizeof secs);
        if (secs[0] == 0)
            return NOTE_OK;
        for (i = 0; secs[i]; i++) {
            if (secs[i] < '0' || secs[i] > '9')
                return NOTE_ERR_SECS;
            v = v * 10 + (secs[i] - '0');
            if (v > NOTE_SECS_MAX)
                return NOTE_ERR_SECS;
        }
        *out_secs = v;
    }
    if (line[pos])
        return NOTE_ERR_SECS;       /* a fourth field */
    return NOTE_OK;
}

static void copy_cut(char *dst, const char *src, int cap)
{
    strncpy(dst, src ? src : "", (size_t)cap - 1);
    dst[cap - 1] = 0;
}

long note_post(note_q *q, const char *title, const char *body, long secs, long now)
{
    note *n;
    if (q->n >= NOTE_MAX_SHOWN) {
        memmove(&q->item[0], &q->item[1], sizeof(note) * (NOTE_MAX_SHOWN - 1));
        q->n--;
        q->dropped++;
    }
    n = &q->item[q->n++];
    memset(n, 0, sizeof *n);
    n->id = q->next_id++;
    copy_cut(n->title, title, NOTE_TITLE);
    copy_cut(n->body, body, NOTE_BODY);
    copy_cut(q->last, n->title, NOTE_TITLE);
    if (secs < 0)
        secs = NOTE_SECS_DEFAULT;
    n->sticky = secs == 0;
    n->expires = now + secs;
    q->posted++;
    return n->id;
}

int note_dismiss(note_q *q, int idx)
{
    if (idx < 0 || idx >= q->n)
        return -1;
    memmove(&q->item[idx], &q->item[idx + 1], sizeof(note) * (size_t)(q->n - idx - 1));
    q->n--;
    return 0;
}

int note_tick(note_q *q, long now)
{
    int i = 0, gone = 0;
    while (i < q->n) {
        if (!q->item[i].sticky && q->item[i].expires <= now) {
            note_dismiss(q, i);
            q->expired++;
            gone++;
        } else
            i++;
    }
    return gone;
}

int note_clear(note_q *q)
{
    int n = q->n;
    q->n = 0;
    return n;
}

int note_slot(int corner, const note_box *box, int w, int h, int slot, int *x, int *y)
{
    int right = corner == NOTE_TOP_RIGHT || corner == NOTE_BOTTOM_RIGHT;
    int top = corner == NOTE_TOP_RIGHT || corner == NOTE_TOP_LEFT;
    int step = slot * (h + NOTE_GAP);
    *x = right ? box->x1 - NOTE_GAP - w + 1 : box->x0 + NOTE_GAP;
    *y = top ? box->y0 + NOTE_GAP + step : box->y1 - NOTE_GAP - h + 1 - step;
    if (*x < box->x0 || *x + w - 1 > box->x1 || *y < box->y0 || *y + h - 1 > box->y1)
        return -1;
    return 0;
}

int note_hit(const note_q *q, int corner, const note_box *box, int w, int h, int px, int py)
{
    int k, x, y;
    for (k = 0; k < q->n; k++) {
        if (note_slot(corner, box, w, h, q->n - 1 - k, &x, &y) != 0)
            continue;
        if (px >= x && px < x + w && py >= y && py < y + h)
            return k;
    }
    return -1;
}

static int ieq(const char *s, const char *word, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        char a = s[i];
        if (a >= 'a' && a <= 'z')
            a -= 32;
        if (a != word[i])
            return 0;
    }
    return 1;
}

static void put(char *out, int cap, const char *s)
{
    int n = (int)strlen(out);
    while (*s && n < cap - 1)
        out[n++] = *s++;
    out[n] = 0;
}

static void putnum(char *out, int cap, long v)
{
    char d[12];
    int n = 0;
    if (v == 0)
        d[n++] = '0';
    while (v > 0 && n < 11) {
        d[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n > 0) {
        char s[2];
        s[0] = d[--n];
        s[1] = 0;
        put(out, cap, s);
    }
}

long note_verb(note_q *q, const char *cmd, long now, char *out, int cap)
{
    out[0] = 0;
    while (*cmd == ' ' || *cmd == '\t')
        cmd++;
    if (ieq(cmd, "NOTIFY", 6) && (cmd[6] == ' ' || cmd[6] == '\t')) {
        char t[NOTE_TITLE], b[NOTE_BODY];
        long s;
        cmd += 7;
        while (*cmd == ' ' || *cmd == '\t')
            cmd++;
        if (note_parse(cmd, t, b, &s) != NOTE_OK)
            return 10;
        note_post(q, t, b, s, now);
        return 0;
    }
    if (ieq(cmd, "CLEAR", 5)) {
        note_clear(q);
        return 0;
    }
    if (ieq(cmd, "COUNT", 5)) {
        putnum(out, cap, q->posted);
        return 0;
    }
    if (ieq(cmd, "NOTES", 5)) {
        put(out, cap, "notes=");
        putnum(out, cap, q->posted);
        put(out, cap, " shown=");
        putnum(out, cap, q->n);
        put(out, cap, " last=");
        put(out, cap, q->last);
        return 0;
    }
    return 10;
}

int note_line(const char *title, const char *body, long secs, char *line, int cap)
{
    int n = 0, f;
    const char *p;
    for (f = 0; f < 2; f++) {
        for (p = f ? body : title; *p; p++) {
            if ((*p == '|' || *p == '\\') && n < cap - 1)
                line[n++] = '\\';
            if (n >= cap - 1)
                return -1;
            line[n++] = *p;
        }
        if (f == 0 || secs >= 0) {
            if (n >= cap - 1)
                return -1;
            line[n++] = '|';
        }
    }
    if (secs >= 0) {
        char d[12];
        int k = 0;
        long v = secs;
        do {
            d[k++] = (char)('0' + v % 10);
            v /= 10;
        } while (v > 0 && k < 11);
        while (k > 0) {
            if (n >= cap - 1)
                return -1;
            line[n++] = d[--k];
        }
    }
    line[n] = 0;
    return n;
}

/* The next word of args from *pos: quotes group, quoted = 1 when it was in quotes. 0 at the end. */
static int word(const char *a, long *pos, char *w, int cap, int *quoted)
{
    long i = *pos;
    int n = 0;
    while (a[i] == ' ' || a[i] == '\t' || a[i] == '\n')
        i++;
    if (!a[i])
        return 0;
    *quoted = a[i] == '"';
    if (*quoted) {
        i++;
        while (a[i] && a[i] != '"') {
            if (a[i] == '*' && (a[i + 1] == '"' || a[i + 1] == '*'))
                i++;                                    /* AmigaDOS escape inside quotes */
            if (n < cap - 1)
                w[n++] = a[i];
            i++;
        }
        if (a[i])
            i++;
    } else {
        while (a[i] && a[i] != ' ' && a[i] != '\t' && a[i] != '\n') {
            if (n < cap - 1)
                w[n++] = a[i];
            i++;
        }
    }
    w[n] = 0;
    *pos = i;
    return 1;
}

static int digits(const char *s, long *v)
{
    long x = 0;
    if (!*s)
        return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            return 0;
        x = x * 10 + (*s - '0');
        if (x > NOTE_SECS_MAX)
            return 0;
    }
    *v = x;
    return 1;
}

int note_cli_parse(const char *args, char *title, char *body, long *secs, char *bodyfile)
{
    char w[NOTE_BODY + 8], v[NOTE_BODY + 8];
    long pos = 0;
    int quoted, npos = 0, which;
    title[0] = body[0] = bodyfile[0] = 0;
    *secs = -1;
    while (word(args, &pos, w, sizeof w, &quoted)) {
        const char *val = 0;
        which = 0;                                      /* 1 seconds, 2 bodyfile */
        if (!quoted) {
            if (w[0] == '-' && (w[1] == 't' || w[1] == 'f') && w[2] == 0) {
                which = w[1] == 't' ? 1 : 2;
            } else if (w[0] == '-' && (w[1] == 't' || w[1] == 'f') && w[2]) {
                which = w[1] == 't' ? 1 : 2;
                val = w + 2;
            } else if (ieq(w, "SECONDS", 7) && (w[7] == 0 || w[7] == '=')) {
                which = 1;
                if (w[7])
                    val = w + 8;
            } else if (ieq(w, "BODYFILE", 8) && (w[8] == 0 || w[8] == '=')) {
                which = 2;
                if (w[8])
                    val = w + 9;
            }
        }
        if (which) {
            if (!val) {
                if (!word(args, &pos, v, sizeof v, &quoted))
                    return NOTE_ERR_EMPTY;
                val = v;
            }
            if (which == 1) {
                if (!digits(val, secs))
                    return NOTE_ERR_SECS;
            } else {
                strncpy(bodyfile, val, NOTE_ARG_BODYFILE - 1);
                bodyfile[NOTE_ARG_BODYFILE - 1] = 0;
            }
        } else if (npos == 0) {
            strncpy(title, w, NOTE_TITLE - 1);
            title[NOTE_TITLE - 1] = 0;
            npos++;
        } else if (npos == 1) {
            strncpy(body, w, NOTE_BODY - 1);
            body[NOTE_BODY - 1] = 0;
            npos++;
        } else
            return NOTE_ERR_EMPTY;                      /* a third word: too many arguments */
    }
    return title[0] ? NOTE_OK : NOTE_ERR_EMPTY;
}
