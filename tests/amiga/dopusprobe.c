/* dopusprobe -- FLAT-D4 rig helper (tools/rig/amarchy_flat_rig.py d4 step).
 *   dopusprobe        print "JT drawbox=<hex> fieldbox=<hex>": the six bytes of the dopus5.library jump table entries
 *                     at LVO -696 / -702 (JMP opcode + target), or "JT absent" when the library is not resident.
 *   dopusprobe ON|OFF set flat_state.enabled in the Amarchy.Flat record (the switch Amarchy writes in the product).
 *                     The state block is found by its first two words (FLAT_VERSION, FLAT_STATE_SIZE) in the record. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include "../../amarchy/flat_core.h"

long __stack = 8192;
const char dopusprobe_stack_cookie[] = "$STACK: 8192";

static void entry(char *out, const unsigned char *lib, long lvo)
{
    const unsigned char *e = lib + lvo;
    sprintf(out, "%02x%02x%02x%02x%02x%02x", e[0], e[1], e[2], e[3], e[4], e[5]);
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = *(struct ExecBase **)4;
    if (argc > 1 && argv[1][0] == 'D') {
        /* DUMP: the first FLAT_FUNC_BYTES of both routines, to see which bytes change between loads (relocations) */
        struct Library *lib;
        int el, i;
        static const long lvo[2] = { FLAT_LVO_DRAWBOX, FLAT_LVO_FIELDBOX };
        Forbid();
        lib = (struct Library *)FindName(&eb->LibList, (STRPTR)"dopus5.library");
        for (el = 0; lib && el < 2; el++) {
            const unsigned char *e = (const unsigned char *)lib + lvo[el];
            const unsigned char *t = (const unsigned char *)(((unsigned long)e[2] << 24) | (e[3] << 16) | (e[4] << 8) | e[5]);
            printf("DUMP%d lib=%08lx ", el, (unsigned long)lib);
            for (i = 0; i < FLAT_FUNC_BYTES; i++)
                printf("%02x", t[i]);
            printf("\n");
        }
        Permit();
        return lib ? 0 : 5;
    }
    if (argc > 1) {
        int on = argv[1][0] == 'O' && argv[1][1] == 'N';
        unsigned char *p;
        int i, done = 0;
        Forbid();
        p = (unsigned char *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
        for (i = 0; p && i < 160 && !done; i += 2) {
            flat_state *s = (flat_state *)(p + i);
            if (s->version == FLAT_VERSION && s->size == FLAT_STATE_SIZE) {
                s->enabled = on;
                done = 1;
            }
        }
        Permit();
        printf("ENABLED %s\n", done ? (on ? "1" : "0") : "no record");
        return done ? 0 : 5;
    }
    {
        struct Library *lib;
        char a[16], b[16];
        Forbid();
        lib = (struct Library *)FindName(&eb->LibList, (STRPTR)"dopus5.library");
        if (!lib) {
            Permit();
            printf("JT absent\n");
            return 0;
        }
        entry(a, (const unsigned char *)lib, FLAT_LVO_DRAWBOX);
        entry(b, (const unsigned char *)lib, FLAT_LVO_FIELDBOX);
        Permit();
        printf("JT drawbox=%s fieldbox=%s\n", a, b);
    }
    return 0;
}
