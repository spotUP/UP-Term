/* The session daemon of a key login (net/upagent.c; plan nas-claude-local NL14): the script the
 * asynchronous shell runs and the AGENT message to the NAS. */
#include "harness.h"
#include "../net/tn.h"
#include "../net/upagent.h"
#include <string.h>

#define HEX "d83a7b587a7c53f235d33e5804971f7ee49132f59cac9cc9fefcc717528da353"

static void the_script_sets_the_key_as_a_local_variable_then_runs_the_daemon_on_its_port(void)
{
    char s[256];
    int n = upagent_script(s, sizeof(s), "UP-Term:bin/wasabid", 1240, HEX);
    CHECK_STR(s, "Set wasabi.key " HEX "\n\"UP-Term:bin/wasabid\" 1240 name session\n");
    CHECK_INT(n, (int)strlen(s));
    /* never ENV: (SetEnv): that would replace the boot daemon's key for every program */
    CHECK(!strstr(s, "SetEnv"));
    CHECK_INT(upagent_script(s, 60, "C:wasabid", 1240, HEX), 0);
    CHECK_INT(upagent_script(s, sizeof(s), "C:wasabid", 0, HEX) > 0, 1);
}

static void the_agent_message_carries_the_port_in_decimal_and_zero_means_none(void)
{
    unsigned char m[32];
    int n = upagent_message(m, sizeof(m), 1240);
    static const unsigned char want[] = { 255, 250, 37, 0, 0x75, 'A', 'G', 'E', 'N', 'T', 0, '1', '2', '4', '0', 255, 240 };
    CHECK_INT(n, (int)sizeof(want));
    CHECK(n == (int)sizeof(want) && !memcmp(m, want, sizeof(want)));
    n = upagent_message(m, sizeof(m), 0);
    CHECK_INT(n, 14);
    CHECK_INT(m[11], '0');
    CHECK_INT(upagent_message(m, 10, 1240), 0);
}

static int seen_default, seen_fallback;
static int only_default(const char *p)
{
    return !strcmp(p, UPAGENT_DEFAULT);
}
static int only_fallback(const char *p)
{
    return !strcmp(p, UPAGENT_FALLBACK);
}
static int both(const char *p)
{
    (void)p;
    return 1;
}
static int none(const char *p)
{
    (void)p;
    return 0;
}

static void the_wasabid_is_the_up_term_one_else_the_one_in_c_else_none(void)
{
    CHECK_STR(upagent_pick(both), "UP-Term:bin/wasabid");
    CHECK_STR(upagent_pick(only_default), "UP-Term:bin/wasabid");
    CHECK_STR(upagent_pick(only_fallback), "C:wasabid");
    CHECK(upagent_pick(none) == 0);
    (void)seen_default;
    (void)seen_fallback;
}

void suite_upagent(void)
{
    the_script_sets_the_key_as_a_local_variable_then_runs_the_daemon_on_its_port();
    the_wasabid_is_the_up_term_one_else_the_one_in_c_else_none();
    the_agent_message_carries_the_port_in_decimal_and_zero_means_none();
}
