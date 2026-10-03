#include "otag.h"

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) |
           (unsigned long)p[3];
}

int otag_check(const unsigned char *buf, long len, otag_info *out)
{
    long at, n = 0, eoff = -1, i;
    out->ntags = 0;
    out->engine_off = -1;
    if (len < 16)
        return OTAG_SHORT;
    if (be32(buf) != OTAG_FILEIDENT)
        return OTAG_NO_IDENT;
    if ((long)be32(buf + 4) != len)
        return OTAG_SIZE;
    for (at = 0; at + 8 <= len; at += 8) {
        unsigned long tag = be32(buf + at), data = be32(buf + at + 4);
        if (tag == OTAG_TAG_DONE)
            break;
        n++;
        if ((tag & OTAG_TAG_USER) && (tag & OTAG_INDIRECT) && data >= (unsigned long)len)
            return OTAG_BAD_OFFSET;
        if (tag == OTAG_ENGINE)
            eoff = (long)data;
    }
    if (at + 8 > len)
        return OTAG_NO_END;
    if (eoff < 0)
        return OTAG_NO_ENGINE;
    for (i = eoff; i < len && buf[i]; i++)
        ;
    if (i >= len || i == eoff)
        return OTAG_NO_ENGINE;
    out->ntags = n;
    out->engine_off = eoff;
    return OTAG_OK;
}
