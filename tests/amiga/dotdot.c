/* dotdot -- what ".." means to an ixemul program (plan
 * 2026-10-02_unix-userland: fix .. in the patched ixemul). For each start
 * directory given: chdir there, print getcwd, list ".." (first entries),
 * chdir(".."), print getcwd again. On Unix the parent of a mount's root is
 * the directory above it; under ixemul an assign X: is /X, so ".." from its
 * root should be "/" (the volume list). */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static void show(const char *start)
{
    char buf[512];
    DIR *d;
    struct dirent *e;
    int n = 0;
    printf("== %s\n", start);
    if (chdir(start) != 0) {
        printf("chdir failed\n");
        return;
    }
    printf("cwd %s\n", getcwd(buf, sizeof(buf)) ? buf : "?");
    d = opendir("..");
    printf("ls ..:");
    if (d) {
        while ((e = readdir(d)) && n < 8) {
            printf(" %s", e->d_name);
            n++;
        }
        closedir(d);
    } else
        printf(" (opendir failed)");
    printf("\n");
    printf("chdir .. -> %d\n", chdir(".."));
    printf("cwd %s\n", getcwd(buf, sizeof(buf)) ? buf : "?");
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
        show(argv[i]);
    return 0;
}
