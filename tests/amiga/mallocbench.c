/* mallocbench -- what one malloc/free costs with vbcc's C library on the
 * rig (vsh's interpreter makes ~66 allocator calls per loop iteration). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/dos.h>

int main(void)
{
    struct DateStamp a, b;
    long i, ticks;
    char *p[16];
    DateStamp(&a);
    for (i = 0; i < 2000; i++) {
        int k;
        for (k = 0; k < 16; k++)
            p[k] = malloc(16 + k * 8);
        for (k = 0; k < 16; k++)
            p[k] = realloc(p[k], 64 + k * 8);
        for (k = 0; k < 16; k++)
            free(p[k]);
    }
    DateStamp(&b);
    ticks = (b.ds_Minute - a.ds_Minute) * 3000 + b.ds_Tick - a.ds_Tick;
    printf("96000 calls: %ld ticks (%ld us each)\n", ticks, ticks * 20000 / 96000);
    return 0;
}
