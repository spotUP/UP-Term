/* readitem ... -- how dos.library reads an AmigaDOS argument line: each item
 * ReadItem() returns from this command's argument line (GetArgStr), with
 * its code, then what ReadArgs("ARGS/M") makes of the same line. The
 * reference for ixemul's argument parser (ixemul-vtcon library/_cli_parse.c,
 * tools/rig/ixargv_rig.py). Bytes outside 33..126 print as <hex>. */
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

static void show(const char *s)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c > 32 && c < 127 && c != '<' && c != '>')
            Printf("%lc", (long)c);
        else
            Printf("<%02lx>", (long)c);
    }
}

int main(void)
{
    static char line[1024];
    struct CSource cs;
    char buf[256];
    LONG r, n = 0;
    LONG arr[1] = { 0 };
    struct RDArgs *ra;

    /* the line as RunCommand passes it, ending in a newline (added when the
       starter left it out: ReadItem does not stop at the end of a CSource) */
    strncpy(line, (const char *)GetArgStr(), sizeof(line) - 2);
    if (!*line || line[strlen(line) - 1] != '\n')
        strcat(line, "\n");
    Printf("line [");
    show(line);
    Printf("]\n");
    cs.CS_Buffer = (STRPTR)line;
    cs.CS_Length = strlen((const char *)line);
    cs.CS_CurChr = 0;
    while (n++ < 40 && cs.CS_CurChr < cs.CS_Length) {
        buf[0] = 0;
        r = ReadItem((STRPTR)buf, sizeof(buf), &cs);
        Printf("item %ld [", r);
        show(buf);
        Printf("] at %ld\n", cs.CS_CurChr);
        if (r == ITEM_ERROR)
            break;
        if (r == ITEM_NOTHING) {
            /* ReadItem stops at a newline without consuming it */
            Printf("skip <%02lx>\n", (long)(unsigned char)line[cs.CS_CurChr]);
            cs.CS_CurChr++;
        }
    }
    ra = ReadArgs((STRPTR)"ARGS/M", arr, 0);
    if (!ra) {
        Printf("readargs fail %ld\n", IoErr());
        return 5;
    }
    if (arr[0]) {
        STRPTR *a;
        for (a = (STRPTR *)arr[0]; *a; a++) {
            Printf("arg [");
            show((const char *)*a);
            Printf("]\n");
        }
    }
    FreeArgs(ra);
    return 0;
}
