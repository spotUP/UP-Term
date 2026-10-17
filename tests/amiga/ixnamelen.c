/* ixnamelen -- a create whose name the handler would shorten fails with
 * ENAMETOOLONG (ixemul-vtcon, 2026-10-17).
 *
 * RAM: on OS 3.1 keeps the first 30 characters of a name and matches
 * lookups on those 30 too: git's 38-character loose-object names landed
 * under 30 and the object store broke. ixemul must refuse every create of
 * such a name (open with O_CREAT, mkdir, rename, link, symlink), leave no
 * shortened entry behind, and never touch another file whose name is the
 * shortened one. Prints one line per case and [OK] or FAIL at the end.
 * Usage: ixnamelen [dir]   (default RAM:ixnamelen, made and emptied) */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LONG1 "0123456789abcdef0123456789abcdef012345" /* 38, like git */
#define SHORT30 "0123456789abcdef0123456789abcd"        /* its first 30 */

static int fails;
static char dir[256];

static const char *p(const char *name)
{
    static char buf[2][512];
    static int i;
    i ^= 1;
    sprintf(buf[i], "%s/%s", dir, name);
    return buf[i];
}

static int exists(const char *name)
{
    struct stat st;
    return stat(p(name), &st) == 0;
}

static void write_file(const char *name, const char *text)
{
    int fd = open(p(name), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, text, strlen(text));
        close(fd);
    }
}

static int has_text(const char *name, const char *text)
{
    char buf[64];
    int fd = open(p(name), O_RDONLY), n;
    if (fd < 0)
        return 0;
    n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n < 0)
        return 0;
    buf[n] = 0;
    return strcmp(buf, text) == 0;
}

/* rc and errno of the call, and that nothing named SHORT30 appeared */
static void check(const char *what, int rc, int err, int want_short30)
{
    int ok = rc == -1 && err == ENAMETOOLONG && exists(SHORT30) == want_short30;
    printf("%-34s rc %d errno %d (%s), %s %s: %s\n", what, rc, rc ? err : 0,
           rc ? strerror(err) : "-", SHORT30,
           exists(SHORT30) ? "exists" : "absent", ok ? "PASS" : "FAIL");
    if (!ok)
        fails++;
}

/* removes what a failed case left under SHORT30 */
static void clean(void)
{
    unlink(p(SHORT30));
    rmdir(p(SHORT30));
}

int main(int argc, char **argv)
{
    int rc, fd;

    strcpy(dir, argc > 1 ? argv[1] : "/RAM/ixnamelen");
    mkdir(dir, 0755);
    clean();
    unlink(p("thirty"));
    write_file("keep", "keep");

    /* new names */
    fd = open(p(LONG1), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    check("open O_CREAT|O_TRUNC 38 chars", fd < 0 ? -1 : 0, errno, 0);
    if (fd >= 0) { close(fd); clean(); }

    fd = open(p(LONG1), O_RDWR | O_CREAT, 0644);
    check("open O_CREAT 38 chars", fd < 0 ? -1 : 0, errno, 0);
    if (fd >= 0) { close(fd); clean(); }

    rc = mkdir(p(LONG1), 0755);
    check("mkdir 38 chars", rc, errno, 0);
    if (rc == 0) clean();

    rc = rename(p("keep"), p(LONG1));
    check("rename to 38 chars", rc, errno, 0);
    if (!has_text("keep", "keep")) {
        printf("rename: keep lost: FAIL\n");
        fails++;
        clean();
        write_file("keep", "keep");
    }

    rc = link(p("keep"), p(LONG1));
    if (rc == -1 && errno != ENAMETOOLONG && !exists(SHORT30))
        printf("%-34s rc -1 errno %d (%s): links not supported here, skipped\n",
               "link to 38 chars", errno, strerror(errno));
    else
        check("link to 38 chars", rc, errno, 0);
    if (rc == 0) unlink(p(SHORT30));

    rc = symlink("keep", p(LONG1));
    if (rc == -1 && errno != ENAMETOOLONG && !exists(SHORT30))
        printf("%-34s rc -1 errno %d (%s): links not supported here, skipped\n",
               "symlink to 38 chars", errno, strerror(errno));
    else
        check("symlink to 38 chars", rc, errno, 0);
    if (rc == 0) unlink(p(SHORT30));

    /* another file already holds the shortened name: refused, untouched */
    write_file(SHORT30, "other");
    fd = open(p(LONG1), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    check("open O_TRUNC, 30-char file there", fd < 0 ? -1 : 0, errno, 1);
    if (fd >= 0) close(fd);
    rc = rename(p("keep"), p(LONG1));
    check("rename, 30-char file there", rc, errno, 1);
    if (!has_text(SHORT30, "other") || !has_text("keep", "keep")) {
        printf("the 30-char file or keep changed: FAIL\n");
        fails++;
    }

    /* names that fit still work */
    unlink(p(SHORT30));
    fd = open(p("thirty"), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) close(fd);
    rc = rename(p("thirty"), p(SHORT30));
    printf("%-34s rc %d: %s\n", "create + rename to 30 chars",
           fd < 0 ? -1 : rc, fd >= 0 && rc == 0 && exists(SHORT30) ? "PASS" : "FAIL");
    if (fd < 0 || rc != 0 || !exists(SHORT30))
        fails++;

    clean();
    unlink(p("keep"));
    unlink(p("thirty"));
    rmdir(dir);
    printf(fails ? "ixnamelen: FAIL (%d)\n" : "ixnamelen: [OK]\n", fails);
    return fails ? 5 : 0;
}
