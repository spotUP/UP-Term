/* handler/complete_core: the rules of the completion worker's command
 * lookup, fed what a host fake of AmigaDOS reports. */
#include "harness.h"
#include "../handler/complete_core.h"

/* protection as fib_Protection: R W E D set = not allowed */
#define P_NOEXEC CC_FIBF_EXECUTE

/* H8.2: Alt+Tab lists only files with the execute or the script bit,
 * never directories (KingCON-handler.asm lbC007E08) */
static void command_list_skips_directories_and_files_without_e_or_s(void)
{
    CHECK_INT(cc_is_command(-3, 0), 1);                         /* rwed file */
    CHECK_INT(cc_is_command(-3, P_NOEXEC), 0);                  /* rw-d: data */
    CHECK_INT(cc_is_command(-3, P_NOEXEC | CC_FIBF_SCRIPT), 1); /* s, no e */
    CHECK_INT(cc_is_command(-3, CC_FIBF_SCRIPT), 1);
    CHECK_INT(cc_is_command(2, 0), 0);                          /* a directory */
    CHECK_INT(cc_is_command(4, 0), 0);                          /* a soft link to one */
}

void suite_complete(void)
{
    command_list_skips_directories_and_files_without_e_or_s();
}
