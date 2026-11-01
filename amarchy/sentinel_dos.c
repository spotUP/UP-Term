#include <dos/dos.h>
#include <proto/dos.h>
#include "sentinel_dos.h"

void am_sentinel(const char *name, long n)
{
    char b[16];
    int i = 15;
    b[i] = 0;
    do {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    } while (n && i > 0);
    SetVar((STRPTR)name, (STRPTR)(b + i), -1, GVF_GLOBAL_ONLY);
}

void am_sentinel_str(const char *name, const char *value)
{
    SetVar((STRPTR)name, (STRPTR)value, -1, GVF_GLOBAL_ONLY);
}
