/* tty/bmsg.c: the boundary messages. Each one names the missing piece and
 * the fix (meta-repo plan phase 5). */
#include "harness.h"
#include "../tty/bmsg.h"

static int has(const char *s, const char *part)
{
    return s && strstr(s, part) != 0;
}

void suite_bmsg(void);
void suite_bmsg(void)
{
    /* uptelnet: no stack, and the three connect errors by their meaning */
    CHECK(has(bmsg_no_stack(), "No TCP/IP stack running (Roadshow or AmiTCP)"));
    CHECK(has(bmsg_net_errno(65), "no route to host"));
    CHECK(has(bmsg_net_errno(61), "connection refused"));
    CHECK(has(bmsg_net_errno(60), "timed out"));
    /* any other number is the caller's "error N" */
    CHECK(bmsg_net_errno(0) == 0);
    CHECK(bmsg_net_errno(48) == 0);
    CHECK(bmsg_net_errno(-61) == 0);

    /* C:Claude */
    CHECK(has(bmsg_amissl_missing(), "needs AmiSSL 5 for talking to Anthropic"));
    CHECK(has(bmsg_amissl_missing(), "ENVARC:Claude/remote"));
    CHECK(has(bmsg_amissl_missing(), "NAS"));
    CHECK(has(bmsg_uptelnet_missing(), "C:uptelnet"));
    CHECK(has(bmsg_uptelnet_missing(), "Install"));

    /* XCON: */
    CHECK(has(bmsg_xcon_missing(), "UP-Term is not installed or XCON: is not mounted"));
    CHECK(has(bmsg_xcon_missing(), "reboot after Install"));

    /* wrong CPU: AttnFlags bit 1 is AFF_68020 (68010 is bit 0) */
    CHECK(!bmsg_cpu_ok(0));
    CHECK(!bmsg_cpu_ok(1));
    CHECK(bmsg_cpu_ok(2));
    CHECK(bmsg_cpu_ok(3));
    CHECK(bmsg_cpu_ok(0x87)); /* 68040, 68030, 68020, 68010 */
    CHECK(has(bmsg_need_68020(), "68020"));

    /* vsh: a kit command names its drawer; anything else stays "not found" */
    CHECK_STR(bmsg_kit_drawer("ssh"), "UP-Term:bin");
    CHECK_STR(bmsg_kit_drawer("scp"), "UP-Term:bin");
    CHECK_STR(bmsg_kit_drawer("sort"), "UP-Term:bin");
    CHECK_STR(bmsg_kit_drawer("["), "UP-Term:bin");
    CHECK_STR(bmsg_kit_drawer("yes"), "UP-Term:bin");  /* the last coreutils name */
    CHECK_STR(bmsg_kit_drawer("bebbosshd"), "UP-Term:bin"); /* the last entry */
    CHECK(bmsg_kit_drawer("so") == 0);      /* a prefix is not a name */
    CHECK(bmsg_kit_drawer("sorts") == 0);
    CHECK(bmsg_kit_drawer("nosuch") == 0);
    CHECK(bmsg_kit_drawer("") == 0);
    CHECK(bmsg_kit_drawer(0) == 0);
    CHECK(bmsg_kit_drawer("SORT") == 0);
}
