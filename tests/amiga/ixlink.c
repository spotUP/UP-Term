/* ixlink -- link() through ixemul: a hard link where the handler makes one,
 * otherwise an errno a portable caller reads as "no hard links here"
 * (EXDEV, EPERM, ENOSYS, EOPNOTSUPP: mutt's safe_rename, git, dpkg fall
 * back to rename on those), never EIO (ledger IXEMUL-LINK).
 *
 * For each directory: chdir there, make .ixl/a, then link the absolute
 * and the bare relative (slash-less, cwd-relative) target, and print rc,
 * errno and the DOS IoErr() the handler gave for the same link through
 * MakeLink directly.
 * Usage: ixlink [dir...]   (default VTC: SYS:T RAM:T) */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <proto/dos.h>

static int fails;

static void raw(const char *from, const char *to)
{
    BPTR l = Lock((STRPTR) from, ACCESS_READ);
    LONG ok, ioerr;
    if (!l) { printf("  raw MakeLink: Lock(%s) failed IoErr %ld\n", from, (long) IoErr()); return; }
    ok = MakeLink((STRPTR) to, (LONG) l, LINK_HARD);
    ioerr = IoErr();
    UnLock(l);
    printf("  raw MakeLink(%s) %s IoErr %ld\n", to, ok ? "ok" : "failed", (long) ioerr);
    if (ok) DeleteFile((STRPTR) to);
}

static void one(const char *what, const char *from, const char *to)
{
    int rc = link(from, to), e = errno;
    int bad = rc != 0 && (e == EIO || e == ENODEV);
    printf("  %-9s link(%s, %s) rc %d errno %d (%s)%s\n", what, from, to, rc,
           rc ? e : 0, rc ? strerror(e) : "-", bad ? ": FAIL" : "");
    fails += bad;
    if (rc == 0) unlink(to);
}

int main(int argc, char **argv)
{
    static const char *def[] = { "VTC:", "SYS:T", "RAM:T" };
    char cwd[256], abs[300];
    int i, n = argc > 1 ? argc - 1 : 3, fd;

    for (i = 0; i < n; i++) {
        const char *d = argc > 1 ? argv[i + 1] : def[i];
        if (chdir(d)) { printf("%s: chdir errno %d, skipped\n", d, errno); continue; }
        getcwd(cwd, sizeof cwd);
        printf("%s (cwd %s)\n", d, cwd);
        mkdir(".ixl", 0777);
        fd = open(".ixl/a", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) { printf("  create .ixl/a errno %d, skipped\n", errno); continue; }
        close(fd);
        snprintf(abs, sizeof abs, "%s/ixl-abs", cwd);
        one("absolute", ".ixl/a", abs);
        one("relative", ".ixl/a", "ixl-rel");
        raw(".ixl/a", "ixl-raw");
        unlink(".ixl/a");
        rmdir(".ixl");
    }
    printf(fails ? "ixlink: FAIL\n" : "ixlink: [OK]\n");
    return fails ? 1 : 0;
}
