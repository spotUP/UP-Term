/* See complete_core.h. Portable C89: no OS calls. */
#include "complete_core.h"

int cc_is_command(long entry_type, unsigned long protection)
{
    if (entry_type >= 0)
        return 0; /* a directory (or no type): never a command */
    return !(protection & CC_FIBF_EXECUTE) || (protection & CC_FIBF_SCRIPT) != 0;
}

int cc_resident_listed(long seg_uc)
{
    return seg_uc >= 0 || seg_uc == CC_CMD_INTERNAL;
}
