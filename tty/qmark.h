/* The lone question mark: every Amiga-native command answers `name ?` by
 * printing its template (or usage) and ending with return code 0 (owner rule
 * 2026-10-14). ReadArgs would print the template too, but then waits for the
 * arguments on the next line; the kit's commands show and exit. The Unix forms
 * `name --help` and `name -h` do the same (owner rule 2026-10-20: the suite
 * takes both conventions); a bare `help` does not, since it can name a file
 * or a host.
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

/* 1 when the n characters at w are `?`, `-h` or `--help` */
QMARK_FN int qmark_word(const char *w, int n)
{
    return (n == 1 && w[0] == '?') || (n == 2 && w[0] == '-' && w[1] == 'h') ||
           (n == 6 && w[0] == '-' && w[1] == '-' && w[2] == 'h' && w[3] == 'e' && w[4] == 'l' && w[5] == 'p');
}

/* 1 when the AmigaDOS argument string (GetArgStr(): leading blanks kept, a
 * trailing newline) holds one word, `?`, `-h` or `--help`. `?x`, `x ?`, `? x`
 * and "" are not. */
QMARK_FN int qmark_line(const char *line)
{
    int n = 0;
    if (!line)
        return 0;
    while (*line == ' ' || *line == '\t')
        line++;
    while (line[n] && line[n] != ' ' && line[n] != '\t' && line[n] != '\n' && line[n] != '\r')
        n++;
    if (!qmark_word(line, n))
        return 0;
    line += n;
    while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r')
        line++;
    return *line == 0;
}

/* 1 when argv holds exactly one argument, `?`, `-h` or `--help` (the
 * Unix-style entry, for the programs that get argc and argv, ixemul or not). */
QMARK_FN int qmark_argv(int argc, char **argv)
{
    int n = 0;
    if (argc != 2 || !argv || !argv[1])
        return 0;
    while (argv[1][n])
        n++;
    return qmark_word(argv[1], n);
}

#endif
