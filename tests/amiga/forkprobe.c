/* forkprobe -- a program for GNU screen's non-window children (backtick,
 * printcmd, blankerprg, LOCKPRG: the fork sites the Amiga port turned into
 * vfork). It appends one line to /RAM/forkprobe.log -- its argv[0], how
 * many arguments, and the first line of its standard input when given
 * "read" -- and prints "forkprobe <argv0>" on its standard output, which
 * screen shows (backtick) or which the blanker's pty receives. */
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char line[200] = "";
    FILE *f;
    int i;

    if (argc > 1 && !strcmp(argv[1], "read") && fgets(line, sizeof(line), stdin))
        line[strcspn(line, "\r\n")] = 0;
    if ((f = fopen("/RAM/forkprobe.log", "a"))) {
        fprintf(f, "argv0=%s argc=%d", argv[0], argc);
        for (i = 1; i < argc; i++)
            fprintf(f, " [%s]", argv[i]);
        if (*line)
            fprintf(f, " stdin=%s", line);
        fputc('\n', f);
        fclose(f);
    }
    printf("forkprobe %s\n", argv[0]);
    fflush(stdout);
    return 0;
}
