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
