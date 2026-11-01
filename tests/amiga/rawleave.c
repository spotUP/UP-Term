/* rawleave -- puts its console in raw mode (SetMode 1) and ends without
 * putting it back, as a program that exits on an error path or forgets
 * does (CTRLC-RAW, tools/rig/sessions/ctrlc_copy_session.py CTRLC_RAW=1).
 * The shell's next prompt must still edit lines and Ctrl-C must still
 * break the next command. */
#include <proto/dos.h>

int main(void)
{
    SetMode(Input(), 1);
    return 0;
}
