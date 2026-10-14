/* The lone question mark: every Amiga-native command answers `name ?` by
 * printing its template (or usage) and ending with return code 0 (owner rule
 * 2026-10-14). ReadArgs would print the template too, but then waits for the
 * arguments on the next line; the kit's commands show and exit.
 *
 * Pure text tests with no OS calls (host-tested, tests/test_qmark.c), static
 * so a command takes it with one #include and no link-line change, vbcc and
 * gcc (ixemul) alike. The command prints its own template:
 *
 *     if (qmark_line((const char *)GetArgStr())) { PutStr(TEMPLATE "\n"); return 0; }
 *     if (qmark_argv(argc, argv)) { usage(); return 0; }       (argv-style)
 *
 * tests/test_qmark_commands.py pins that every command of the Makefile's
 * `amiga` target (and the dist targets) makes the call. */
#ifndef QMARK_H
#define QMARK_H

/* a program that takes one of the two (hl on the host) must not trip -Wunused-function */
#ifdef __GNUC__
#define QMARK_FN static __attribute__((unused))
#else
#define QMARK_FN static
#endif

/* 1 when the AmigaDOS argument string (GetArgStr(): leading blanks kept, a
 * trailing newline) holds one word, `?`. `?x`, `x ?`, `? x` and "" are not. */
QMARK_FN int qmark_line(const char *line)
{
    if (!line)
        return 0;
    while (*line == ' ' || *line == '\t')
        line++;
    if (*line != '?')
        return 0;
    line++;
    while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r')
        line++;
    return *line == 0;
}

/* 1 when argv holds exactly one argument, `?` (the Unix-style entry, for the
 * programs that get argc and argv, ixemul or not). */
QMARK_FN int qmark_argv(int argc, char **argv)
{
    return argc == 2 && argv && argv[1] && argv[1][0] == '?' && argv[1][1] == 0;
}

#endif
