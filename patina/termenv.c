#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "termenv.h"

void termenv_ensure(void)
{
    char buf[8];
    LONG n = GetVar((STRPTR)"TERM", (STRPTR)buf, sizeof buf, GVF_GLOBAL_ONLY);
    if (n <= 0)
        SetVar((STRPTR)"TERM", (STRPTR)"vtcon", 5, GVF_GLOBAL_ONLY);
}
