/* upmru.c -- the most-recently-focused list of UP-Term request ports and a port's name
 * (declared in keymap_act.h). Its own file so the Amarchy commodity, which reaches the windows
 * only through handler/upactive.c, links these few functions and not the rest of keymap_act.c. */
#include <string.h>
#include "keymap_act.h"

void upmru_forget(unsigned long l[UPMRU_N], unsigned long id)
{
    int i, j;
    for (i = j = 0; i < UPMRU_N; i++)
        if (l[i] && l[i] != id)
            l[j++] = l[i];
    while (j < UPMRU_N)
        l[j++] = 0;
}

void upmru_focus(unsigned long l[UPMRU_N], unsigned long id)
{
    int i;
    if (!id)
        return;
    upmru_forget(l, id);
    for (i = UPMRU_N - 1; i > 0; i--)
        l[i] = l[i - 1];
    l[0] = id;
}

unsigned long upmru_target(const unsigned long l[UPMRU_N], int self_in_mru)
{
    int n = self_in_mru ? 1 : 0;
    return l[0] ? l[n] : 0;
}

void upreq_portname(unsigned long id, char out[24])
{
    static const char hex[] = "0123456789abcdef";
    int i;
    strcpy(out, "UPTERM.req.");
    for (i = 0; i < 8; i++)
        out[11 + i] = hex[(id >> (28 - 4 * i)) & 15];
    out[19] = 0;
}
