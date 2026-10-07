/* config/upassign.h: the drawer ENVARC:up-term/Dir names, as Install writes
 * it (UPTERM-VOLUME-REQUESTER). A line Install did not write makes no
 * assign: a wrong one would send every UP-Term: file to the wrong place. */
#include "harness.h"
#include "../config/upassign.h"

static int parse(const char *s, char *out, int cap)
{
    return upassign_parse(s, (long)strlen(s), out, cap);
}

void suite_upassign(void);
void suite_upassign(void)
{
    char out[64];
    /* what Install writes: the drawer and a line end */
    CHECK(parse("SYS:UP-Term\n", out, sizeof(out)) == 11 && !strcmp(out, "SYS:UP-Term"));
    CHECK(parse("VTC:Apps/UP-Term", out, sizeof(out)) == 16 && !strcmp(out, "VTC:Apps/UP-Term"));
    /* a CR LF, trailing blanks, quotes round the name, a second line: the first name */
    CHECK(parse("SYS:UP-Term \r\nignored\n", out, sizeof(out)) == 11 && !strcmp(out, "SYS:UP-Term"));
    CHECK(parse("\"Work:My Apps/UP-Term\"\n", out, sizeof(out)) == 20 && !strcmp(out, "Work:My Apps/UP-Term"));
    /* a bare volume is a drawer too */
    CHECK(parse("UPT:", out, sizeof(out)) == 4);
    /* not a name: empty, no volume, a leading colon, an escape, a lone quote */
    CHECK(parse("", out, sizeof(out)) == 0);
    CHECK(parse("\n", out, sizeof(out)) == 0);
    CHECK(parse("UP-Term\n", out, sizeof(out)) == 0);
    CHECK(parse(":UP-Term\n", out, sizeof(out)) == 0);
    CHECK(parse("SYS:UP*\"Term\n", out, sizeof(out)) == 0);
    CHECK(parse("\"\n", out, sizeof(out)) == 0);
    /* too long for the caller's buffer: refused, never cut */
    CHECK(parse("SYS:0123456789", out, 8) == 0);
    CHECK(parse("SYS:0123456789", out, 15) == 14);
    CHECK(upassign_parse(0, 3, out, sizeof(out)) == 0);
    /* a file that holds binary garbage with a NUL stops there */
    CHECK(upassign_parse("SYS:X\0junk", 10, out, sizeof(out)) == 5);
    /* vsh: UP-Term:bin/vsh first, C:vsh for an older install, none at all */
    CHECK(!strcmp(upassign_vsh_pick(1, 1), "UP-Term:bin/vsh"));
    CHECK(!strcmp(upassign_vsh_pick(1, 0), "UP-Term:bin/vsh"));
    CHECK(!strcmp(upassign_vsh_pick(0, 1), "C:vsh"));
    CHECK(upassign_vsh_pick(0, 0) == 0);
}
