/* ixmalloc -- how fast ixemul's malloc, free and realloc are (tmux's
 * start-up spent 39 s parsing ~30 KB of key bindings: its lexer grows a
 * token one byte at a time with realloc). Times in ms per 10000 calls. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static long ms(struct timeval *a)
{
    struct timeval b;
    gettimeofday(&b, NULL);
    return (b.tv_sec - a->tv_sec) * 1000 + (b.tv_usec - a->tv_usec) / 1000;
}

#include <unistd.h>

int main(void)
{
    struct timeval t;
    char *p;
    int i, k;

    gettimeofday(&t, NULL);
    for (i = 0; i < 10000; i++)
        (void)getpid();
    printf("getpid (a library call and nothing else), 10000x: %ld ms\n", ms(&t));
    gettimeofday(&t, NULL);
    for (i = 0; i < 10000; i++) {
        void *volatile q = malloc(32);
        (void)q;
    }
    printf("malloc 32 bytes alone, 10000x: %ld ms\n", ms(&t));
    {
        int live;
        for (live = 0; live <= 20000; live = live ? live * 4 : 1250) {
            /* a heap with live blocks, as tmux's has after start-up */
            static void *volatile keep2[20000];
            for (i = 0; i < live; i++)
                keep2[i] = malloc(16 + (i % 7) * 24);
            gettimeofday(&t, NULL);
            for (i = 0; i < 10000; i++) {
                void *volatile q = malloc(32);
                free(q);
            }
            printf("malloc+free 32 bytes with %d live blocks, 10000x: %ld ms\n", live, ms(&t));
            for (i = 0; i < live; i++)
                free(keep2[i]);
        }
    }

    gettimeofday(&t, NULL);
    for (k = 0; k < 20; k++) {
        p = NULL;
        for (i = 1; i <= 500; i++) {    /* a 500-byte token, one byte at a time */
            p = realloc(p, i + 1);
            p[i - 1] = 'x';
        }
        free(p);
    }
    printf("realloc +1 byte to 500, 20 tokens (10000 calls): %ld ms\n", ms(&t));
    return 0;
}
