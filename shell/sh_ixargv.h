/* The argument string vsh gives an ixemul program: the AmigaDOS line every
 * program gets, and after its newline the argv byte for byte (the
 * out-of-band argv; owner's decision 2026-10-08). ixemul-vtcon's startup
 * (library/cli_args.c, which documents the format) takes the argv when it
 * matches the line; a native program and an older ixemul stop at the
 * line's newline and read the line as before. Apart from AmigaOS so the
 * host tests it (tests/test_sh_ixargv.c).
 *
 *   <line>\n \001IXA1 <hhhh> <llllllll> <arg>\001 <arg>\001 ... \n
 *
 * hhhh: 16-bit hash of the line's bytes (h = h * 31 + byte), llllllll its
 * length, lower-case hex; inside an argument 0x00 0x01 0x02 \n are written
 * 0x02 and the byte + 0x40. */
#ifndef SH_IXARGV_H
#define SH_IXARGV_H

#include <string.h>

/* Does a program start with ixemul's crt0 header? The first three longs of
 * its first hunk: a bra or jmp, then OMAGIC (0407) in the exec header that
 * follows; the test ixemul's own execve() makes (stdlib/execve.c MAGIC_16,
 * MAGIC_32). */
static int ixa_is_ixemul(const unsigned long *code)
{
    unsigned long op = code[0] & 0xffff0000UL;
    if ((op == 0x60000000UL || op == 0x4efa0000UL) && (code[1] & 0xffffUL) == 0407)
        return 1;
    return op == 0x4efb0000UL && (code[2] & 0xffffUL) == 0407;
}

static int ixa_special(unsigned char c)
{
    return c == 0 || c == 1 || c == 2 || c == '\n';
}

/* Bytes of the whole argument string for the line and args (argv after the
 * program's name, ending in 0), without a NUL. */
static long ixa_len(const char *line, char *const *args)
{
    long n = (long)strlen(line) + 1 + 5 + 4 + 8 + 1;
    const char *a;
    for (; *args; args++) {
        for (a = *args; *a; a++)
            n += ixa_special((unsigned char)*a) ? 2 : 1;
        n++;
    }
    return n;
}

/* Writes it to out (ixa_len bytes and a NUL). */
static void ixa_put(char *out, const char *line, char *const *args)
{
    static const char hex[] = "0123456789abcdef";
    unsigned long h = 0, len = (unsigned long)strlen(line);
    const char *a;
    int i;
    for (a = line; *a; a++)
        h = (h * 31 + (unsigned char)*a) & 0xffffUL;
    memcpy(out, line, len);
    out += len;
    memcpy(out, "\n\001IXA1", 6);
    out += 6;
    for (i = 3; i >= 0; i--)
        *out++ = hex[(h >> (4 * i)) & 15];
    for (i = 7; i >= 0; i--)
        *out++ = hex[(len >> (4 * i)) & 15];
    for (; *args; args++) {
        for (a = *args; *a; a++) {
            if (ixa_special((unsigned char)*a)) {
                *out++ = 2;
                *out++ = (char)(*a + 0x40);
            } else
                *out++ = *a;
        }
        *out++ = 1;
    }
    *out++ = '\n';
    *out = 0;
}

#endif
