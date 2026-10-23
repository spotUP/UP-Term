/* upsame -- is a file exactly the one the kit knows by size and CRC-32?
 *
 *   upsame FILE SIZE CRC          (CRC in hex, as zlib.crc32 prints it)
 *
 * Stock AmigaDOS has no byte compare and the cmp port needs ixemul, which
 * the Install puts in after the fonts; this runs in a plain Shell as well as
 * under the Installer. The Install uses it to tell the original TopazPro 16
 * of the dS-tprf.lha archive from a user's own copy (install.dos, Lab fonts).
 * Return code 0 the file is that one, 5 it is another or cannot be read,
 * 20 the arguments. */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "../tty/qmark.h"

static unsigned long crc_step(unsigned long c, const UBYTE *p, LONG n)
{
    int k;
    while (n-- > 0) {
        c ^= *p++;
        for (k = 0; k < 8; k++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : (c >> 1);
    }
    return c;
}

static int hexval(const char *s, unsigned long *out)
{
    unsigned long v = 0;
    int n = 0;
    for (; *s; s++, n++) {
        char ch = *s;
        int d = ch >= '0' && ch <= '9' ? ch - '0' : (ch | 0x20) >= 'a' && (ch | 0x20) <= 'f' ? (ch | 0x20) - 'a' + 10 : -1;
        if (d < 0)
            return 0;
        v = (v << 4) | (unsigned long)d;
    }
    *out = v;
    return n > 0 && n <= 8;
}

int main(void)
{
    static const char tmpl[] = "FILE/A,SIZE/A/N,CRC/A";
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rd;
    unsigned long want, crc = 0xFFFFFFFFUL, total = 0;
    static UBYTE buf[4096];
    BPTR f;
    LONG n = -1;
    int rc = 5;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)tmpl);
        PutStr((STRPTR)"\n");
        return RETURN_OK;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"upsame");
        return 20;
    }
    if (!hexval((const char *)args[2], &want)) {
        FreeArgs(rd);
        return 20;
    }
    f = Open((STRPTR)args[0], MODE_OLDFILE);
    if (f) {
        while ((n = Read(f, buf, sizeof buf)) > 0) {
            crc = crc_step(crc, buf, n);
            total += (unsigned long)n;
        }
        Close(f);
        if (n == 0 && total == (unsigned long)*(LONG *)args[1] && (crc ^ 0xFFFFFFFFUL) == want)
            rc = 0;
    }
    FreeArgs(rd);
    return rc;
}
