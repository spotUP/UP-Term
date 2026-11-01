/* segstack: the stack a program asks for in its file, the "$STACK: n" cookie
 * (AmigaOS 3.2 and 4 honour it; vsh does on 3.1 too).
 *
 * A launcher that starts a loaded program (NP_Seglist) must give it at least
 * that much: upgetty gave vsh ($STACK: 262144) a fixed 16000 and the shell on
 * the serial line crashed (#80000006 on the owner's A1200). vsh's command
 * starter and upgetty both read the cookie here (RULES.md rule 6).
 * segstack_scan is pure (host-tested); seg_stack walks a seglist and needs
 * SEGSTACK_DOS defined after proto/dos.h. */
#ifndef SEGSTACK_H
#define SEGSTACK_H

#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define SEGSTACK_FN static __attribute__((unused))
#else
#define SEGSTACK_FN static
#endif

/* The first non-zero "$STACK: n" value in [p, end), 0: none. */
SEGSTACK_FN unsigned long segstack_scan(const unsigned char *p, const unsigned char *end)
{
    static const char key[] = "$STACK:";
    for (; p + 8 < end; p++) {
        if (*p == '$' && !memcmp(p, key, 7)) {
            const unsigned char *d = p + 7;
            unsigned long v = 0;
            while (d < end && *d == ' ')
                d++;
            while (d < end && *d >= '0' && *d <= '9')
                v = v * 10 + (unsigned long)(*d++ - '0');
            if (v)
                return v;
        }
    }
    return 0;
}

#ifdef SEGSTACK_DOS
/* The cookie in a loaded program's hunks, 0: none. */
SEGSTACK_FN ULONG seg_stack(BPTR seg)
{
    for (; seg; seg = *(BPTR *)BADDR(seg)) {
        ULONG n = ((ULONG *)BADDR(seg))[-1], v;
        v = segstack_scan((const unsigned char *)BADDR(seg) + 4, (const unsigned char *)BADDR(seg) - 4 + n);
        if (v)
            return v;
    }
    return 0;
}
#endif

#endif
