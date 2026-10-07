/* ixumask -- prints the file creation mask this ixemul program started with
 * and the permission bits of a file it creates with mode 0666 in T:
 * (tools/rig/umask_rig.py: vsh's umask builtin must reach the commands vsh
 * starts). Output: "[UMASK] 0077" and "[CREATE] 0600". */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(void)
{
    struct stat st;
    mode_t m = umask(0);
    int fd;
    umask(m);
    printf("[UMASK] %04o\n", (unsigned)m);
    unlink("T:ixumask.tmp");
    fd = open("T:ixumask.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        printf("[CREATE] failed\n");
        return 1;
    }
    close(fd);
    if (stat("T:ixumask.tmp", &st) < 0) {
        printf("[CREATE] stat failed\n");
        return 1;
    }
    printf("[CREATE] %04o\n", (unsigned)(st.st_mode & 0777));
    unlink("T:ixumask.tmp");
    return 0;
}
