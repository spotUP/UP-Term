/* upassign: the assign UP-Term: that exists before anything of UP-Term asks
 * for a file through it.
 *
 * S:User-Startup makes the assign, but XCON:, PTY: and console.device are
 * mounted before it runs: a window opened early (or a kit whose block was
 * lost) reached UP-Term:unifont/ with no assign and AmigaDOS asked for the
 * volume "UP-Term:" in a requester. Install writes the drawer it assigned
 * into ENVARC:up-term/Dir; upassign_ensure() makes the assign from that file
 * when it is missing, so every consumer that includes this header with
 * UPASSIGN_DOS defined (after proto/dos.h) has the assign as early as it
 * needs it. Nothing here may raise a requester: pr_WindowPtr is -1 for
 * every DOS call. The parse is pure (host-tested, tests/test_upassign.c). */
#ifndef UPASSIGN_H
#define UPASSIGN_H

#define UPASSIGN_NAME "UP-Term"
#define UPASSIGN_DIR_FILE "ENVARC:up-term/Dir"
#define UPASSIGN_MAX 256

/* Header-only, so the handler, the device, vsh and UPConsole each take it
 * without another object in four link lines (outline.c is shared by two). */
#if defined(__GNUC__) || defined(__clang__)
#define UPASSIGN_FN static __attribute__((unused))
#else
#define UPASSIGN_FN static
#endif

/* The drawer named by the contents of the Dir file: its first line, without
 * the line end, trailing blanks or quotes around it. Must name a volume or
 * assign (a ':' that is not the first character) and fit in cap. Returns the
 * length, 0 when the file does not hold a usable name. */
UPASSIGN_FN int upassign_parse(const char *buf, long len, char *out, int cap)
{
    long i = 0, e;
    int n;
    if (!buf || !out || cap < 3)
        return 0;
    while (i < len && (buf[i] == ' ' || buf[i] == '\t'))
        i++;
    e = i;
    while (e < len && buf[e] != '\n' && buf[e] != '\r' && buf[e] != 0)
        e++;
    while (e > i && (buf[e - 1] == ' ' || buf[e - 1] == '\t'))
        e--;
    if (e - i >= 2 && buf[i] == '"' && buf[e - 1] == '"') {
        i++;
        e--;
    }
    if (e <= i || e - i >= cap)
        return 0;
    n = (int)(e - i);
    {
        int k, colon = 0;
        for (k = 0; k < n; k++) {
            out[k] = buf[i + k];
            if (out[k] == ':' && k > 0)
                colon = 1;
            if (out[k] == '"' || out[k] == '*')
                return 0; /* a name that needs escaping is not one Install writes */
        }
        out[n] = 0;
        if (!colon || out[0] == ':')
            return 0;
    }
    return n;
}

#ifdef UPASSIGN_DOS
/* 1 when UP-Term: is an assign (or a volume, or a device) afterwards, 0
 * when it is not and cannot be made: no Dir file (not installed, or removed
 * by Uninstall), or the drawer is gone. Never a requester; cheap when the
 * assign is there (one look at the DOS list). */
UPASSIGN_FN int upassign_ensure(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    struct DosList *dl;
    int have;
    ULONG flags = LDF_ASSIGNS | LDF_DEVICES | LDF_VOLUMES;
    BPTR fh, lock;
    char buf[UPASSIGN_MAX], dir[UPASSIGN_MAX];
    LONG n;

    dl = LockDosList(flags | LDF_READ);
    have = FindDosEntry(dl, (STRPTR)UPASSIGN_NAME, flags) != 0;
    UnLockDosList(flags | LDF_READ);
    if (have)
        return 1;
    me->pr_WindowPtr = (APTR)-1;
    fh = Open((STRPTR)UPASSIGN_DIR_FILE, MODE_OLDFILE);
    n = fh ? Read(fh, buf, sizeof(buf)) : -1;
    if (fh)
        Close(fh);
    have = 0;
    if (n > 0 && upassign_parse(buf, n, dir, sizeof(dir)) != 0 && (lock = Lock((STRPTR)dir, SHARED_LOCK)) != 0) {
        if (AssignLock((STRPTR)UPASSIGN_NAME, lock))
            have = 1; /* the assign owns the lock now */
        else
            UnLock(lock);
    }
    me->pr_WindowPtr = oldwin;
    return have;
}
#endif /* UPASSIGN_DOS */

#endif
