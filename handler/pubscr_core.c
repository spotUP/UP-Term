/* pubscr_core: see pubscr_core.h. */
#include "pubscr_core.h"

int ip_pub_lockable(int listed, int has_screen, unsigned flags)
{
    return listed && has_screen && !(flags & IP_PSN_PRIVATE);
}
