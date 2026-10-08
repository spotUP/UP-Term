/* The argument string vsh gives an ixemul program (shell/sh_ixargv.h): the
 * line, then the argv byte for byte. ixemul-vtcon's tests/host/test_cli_args.c
 * decodes the same bytes (vsh_args there) to the same argv. */
#include "harness.h"
#include <stdlib.h>
#include "../shell/sh_ixargv.h"

/* 'a**b' 'c*"d' $'\n' '' '"' $'x\001\002y', and the line vsh's command_line
 * writes for them */
static void argv_follows_the_line_byte_for_byte(void)
{
    static char *const args[] = { "a**b", "c*\"d", "\n", "", "\"", "x\001\002y", 0 };
    static const char line[] = "\"a****b\" \"c***\"d\" \"*N\" \"\" \"*\"\" x\001\002y";
    static const char want[] =
        "\"a****b\" \"c***\"d\" \"*N\" \"\" \"*\"\" x\001\002y\n"
        "\001IXA1371400000023a**b\001c*\"d\001\002J\001\001\"\001x\002A\002By\001\n";
    long n = ixa_len(line, args);
    char *out = (char *)malloc((size_t)n + 1);
    CHECK_INT(n, (long)sizeof(want) - 1);
    ixa_put(out, line, args);
    CHECK(memcmp(out, want, sizeof(want)) == 0);
    free(out);
}

static void no_arguments_is_the_header_alone(void)
{
    static char *const args[] = { 0 };
    char out[32];
    CHECK_INT(ixa_len("", args), 19);
    ixa_put(out, "", args);
    CHECK_STR(out, "\n\001IXA1000000000000\n");
}

/* ixemul's crt0 header (build/amiga/ixargv, ixkill: 0x600001f8 0x00000107),
 * the jmp forms ixemul's execve also takes, and vsh's own start (native) */
static void ixemul_programs_are_told_from_native_ones(void)
{
    static const unsigned long ix[3] = { 0x600001f8UL, 0x00000107UL, 0x00000e1cUL };
    static const unsigned long jmp16[3] = { 0x4efa0010UL, 0x00000107UL, 0 };
    static const unsigned long jmp32[3] = { 0x4efb0170UL, 0x00000000UL, 0x00000107UL };
    static const unsigned long vsh[3] = { 0x60085642UL, 0x43432030UL, 0x2e392400UL };
    static const unsigned long movem[3] = { 0x48e7fffeUL, 0x00000107UL, 0 };
    CHECK_INT(ixa_is_ixemul(ix), 1);
    CHECK_INT(ixa_is_ixemul(jmp16), 1);
    CHECK_INT(ixa_is_ixemul(jmp32), 1);
    CHECK_INT(ixa_is_ixemul(vsh), 0);
    CHECK_INT(ixa_is_ixemul(movem), 0);
}

void suite_sh_ixargv(void)
{
    argv_follows_the_line_byte_for_byte();
    no_arguments_is_the_header_alone();
    ixemul_programs_are_told_from_native_ones();
}
