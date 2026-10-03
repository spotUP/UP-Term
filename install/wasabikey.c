/* wasabikey: a key for wasabid (the kit's Files/wasabi), unless one is set.
 *
 * wasabid takes a client whose key equals ENV:wasabi.key -- and with no
 * key set, any client on the network (an empty key equals an empty key).
 * Install runs this so a user who ticks wasabi never has to make one: 20
 * characters from the beam position, the clock, exec's counters and the
 * timing of this very loop, saved to ENVARC: and ENV:. A key already there
 * stays (the user's own, or an earlier install's). Prints the key.
 * RC 0, or 20 when it cannot be saved. */
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>

struct GfxBase *GfxBase;

static ULONG mix(ULONG h, ULONG v)
{
    h ^= v + 0x9E3779B9UL + (h << 6) + (h >> 2);
    h *= 0x85EBCA6BUL;
    return h ^ (h >> 13);
}

int main(void)
{
    static const char abc[] = "abcdefghijkmnpqrstuvwxyz23456789"; /* 32: no l, o, 0, 1 */
    struct ExecBase *eb = *(struct ExecBase **)4;
    char key[24];
    struct DateStamp ds;
    ULONG h[4] = { 0x243F6A88UL, 0x85A308D3UL, 0x13198A2EUL, 0x03707344UL };
    int i, j;
    if (GetVar((STRPTR)"wasabi.key", (STRPTR)key, sizeof(key), 0) > 0) {
        Printf((STRPTR)"%s\n", (LONG)key);
        return 0;
    }
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!GfxBase)
        return 20;
    for (i = 0; i < 20; i++) {
        /* each character: how far the CPU gets in one beam line, the beam
         * after a Delay (its phase drifts against the CPU), the clock,
         * exec's dispatch and idle counts */
        for (j = 0; j < 4; j++) {
            ULONG spin = 0;
            LONG b0 = VBeamPos();
            while (VBeamPos() == b0)
                spin++;
            DateStamp(&ds);
            h[j] = mix(h[j], spin);
            h[j] = mix(h[j], ((ULONG)VBeamPos() << 16) ^ (ULONG)ds.ds_Tick);
            h[j] = mix(h[j], eb->DispCount ^ (eb->IdleCount << 7) ^ (ULONG)ds.ds_Minute);
            h[j] = mix(h[j], AvailMem(MEMF_ANY) ^ (ULONG)FindTask(0));
        }
        key[i] = abc[(h[0] ^ h[1] ^ h[2] ^ h[3]) >> 27];
        Delay(1);
    }
    key[20] = 0;
    CloseLibrary((struct Library *)GfxBase);
    if (!SetVar((STRPTR)"wasabi.key", (STRPTR)key, -1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR))
        return 20;
    Printf((STRPTR)"%s\n", (LONG)key);
    return 0;
}
