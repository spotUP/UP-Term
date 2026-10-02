/* fstatprobe -- what fstat() says about descriptors 0 and 1 (coreutils wc
 * -c trusts st_size when the mode says a regular file: a pipe that reads as
 * one counts 0 bytes). Prints the file type and size of each. */
#include <proto/dos.h>
#include <stdio.h>
#include <sys/stat.h>

static const char *kind(mode_t m)
{
    if (S_ISREG(m)) return "regular";
    if (S_ISFIFO(m)) return "fifo";
    if (S_ISCHR(m)) return "char";
    if (S_ISDIR(m)) return "dir";
    if (S_ISSOCK(m)) return "socket";
    return "other";
}

int main(void)
{
    struct stat st;
    int fd;
    for (fd = 0; fd < 2; fd++) {
        if (fstat(fd, &st) != 0)
            fprintf(stderr, "fd %d: fstat failed\n", fd);
        else
            fprintf(stderr, "fd %d: %s mode %o size %ld\n", fd, kind(st.st_mode),
                    (unsigned)st.st_mode, (long)st.st_size);
    }
    {
        /* the DOS view of the input handle: what __fstat has to go on */
        static struct FileInfoBlock fib __attribute__((aligned(4)));
        BPTR in = Input();
        LONG ex = ExamineFH(in, &fib), exerr = IoErr();
        LONG cur = Seek(in, 0, OFFSET_CURRENT), curerr = IoErr();
        LONG end = Seek(in, 0, OFFSET_END), enderr = IoErr();
        fprintf(stderr, "dos in: interactive %ld examinefh %ld (err %ld type %ld) "
                "seek cur %ld (err %ld) seek end %ld (err %ld)\n",
                (long)IsInteractive(in), (long)ex, (long)exerr, (long)fib.fib_DirEntryType,
                (long)cur, (long)curerr, (long)end, (long)enderr);
    }
    return 0;
}
