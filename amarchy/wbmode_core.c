/* See wbmode_core.h. */
#include "wbmode_core.h"

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static unsigned be16(const unsigned char *p)
{
    return (unsigned)((p[0] << 8) | p[1]);
}

static int tag_is(const unsigned char *p, const char *t)
{
    return p[0] == (unsigned char)t[0] && p[1] == (unsigned char)t[1] && p[2] == (unsigned char)t[2] &&
           p[3] == (unsigned char)t[3];
}

int wbm_parse_prefs(const unsigned char *buf, long len, wbm_prefs *out)
{
    long pos, end;
    if (!buf || !out || len < 12 || !tag_is(buf, "FORM") || !tag_is(buf + 8, "PREF"))
        return 0;
    end = (long)be32(buf + 4) + 8;
    if (end > len || end < 12)
        end = len;
    for (pos = 12; pos + 8 <= end;) {
        unsigned long sz = be32(buf + pos + 4);
        long body = pos + 8;
        if (tag_is(buf + pos, "SCRM")) {
            if (sz < 28 || body + 28 > end)
                return 0;
            out->id = be32(buf + body + 16);
            out->width = be16(buf + body + 20);
            out->height = be16(buf + body + 22);
            out->depth = be16(buf + body + 24);
            return 1;
        }
        if (sz > (unsigned long)(end - body))
            return 0;
        pos = body + (long)sz + (long)(sz & 1);   /* chunks are padded to even length */
    }
    return 0;
}

int wb_mode_ready(unsigned long prefs_id, int have_prefs, unsigned long wb_mode_id, long elapsed_ticks,
                  long timeout_ticks)
{
    if (!have_prefs || prefs_id == WBM_INVALID_ID || prefs_id == wb_mode_id)
        return WBM_READY;
    return elapsed_ticks >= timeout_ticks ? WBM_TIMEOUT : WBM_WAIT;
}
