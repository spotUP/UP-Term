/* IPrefsWait: returns once IPrefs sits idle (RIG-IPREFS-SYSREQ). The rig's S:Startup-Sequence runs it right after
 * C:IPrefs, so nothing opens a window on the Workbench screen while IPrefs still resets it from ScreenMode prefs
 * (Intuition then asks "close all windows" and Retry never succeeds). The idle test is Amarchy's (DESK-31). */
#include <proto/dos.h>
#include "../../amarchy/iprefs_wait.h"

#define IPW_SECS 120

long __stack = 8192;
const char iprefswait_stack_cookie[] = "$STACK: 8192";

int main(void)
{
    /* a boot starved by BOOTX:go lets IPrefs reach its idle Wait() only after ~40 s */
    desk_iprefs_wait_for(desk_iprefs_idle_mask_for(IPW_SECS), IPW_SECS);
    return 0;
}
