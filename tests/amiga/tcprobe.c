/* tcprobe [TERM] -- what the GG ncurses termcap layer answers for a
 * terminal (GNU screen reads Co, AF, AB this way: 256 colours only when
 * tgetnum("Co") is exactly 256). */
#include <stdio.h>
#include <stdlib.h>
#include <termcap.h>

int main(int argc, char **argv)
{
    char buf[4096], area[4096], *ap = area, *s;
    const char *term = argc > 1 ? argv[1] : getenv("TERM");
    int rc = tgetent(buf, term);
    printf("tgetent(%s) = %d\n", term ? term : "(null)", rc);
    printf("Co = %d\n", tgetnum("Co"));
    s = tgetstr("AF", &ap);
    printf("AF = %s\n", s ? "set" : "none");
    return 0;
}
