/* ixfionread -- FIONREAD on standard input when it is a vsh pipe (PTY:<id>/r):
 * the bytes the pipe holds, and 0 at its end of file. Run as
 *   echo hello | VTC:ixfionread
 * It reads one byte, asks FIONREAD (5 expected: "ello\n"), reads to the end
 * of file, asks again (0 expected). ixemul answered a pipe that does not
 * seek with WaitForChar() != 0: 1 and 1, and coreutils cat -n, which writes
 * its buffer only when FIONREAD says 0, printed nothing for `echo y | cat -n`.
 * Prints the two counts and PASS or FAIL. */
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main(void)
{
    char c, buf[64];
    int mid = -1, end = -1;
    if (read(0, &c, 1) != 1) {
        printf("FAIL no input\n");
        return 1;
    }
    if (ioctl(0, FIONREAD, &mid) < 0)
        perror("FIONREAD");
    while (read(0, buf, sizeof(buf)) > 0)
        ;
    if (ioctl(0, FIONREAD, &end) < 0)
        perror("FIONREAD");
    printf("fionread after one byte %d, at end of file %d: %s\n", mid, end,
           mid == 5 && end == 0 ? "PASS" : "FAIL");
    return !(mid == 5 && end == 0);
}
