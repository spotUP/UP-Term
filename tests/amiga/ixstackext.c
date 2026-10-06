/* ixstackext -- the signal mask across ixemul's stack extension.
 * Built with -mstackextend (ixemul's gcc 2.95.3): a function whose frame
 * would pass the stack limit calls ixemul's __stkext glue, which saves the
 * signal mask in a slot (atomic_on), switches to a new stack frame, and
 * restores the mask from the slot afterwards (atomic_off). gcc 16 -O2 dropped
 * the save in atomic_on() and the mask came back as stack garbage. main()
 * blocks SIGUSR1 only and recurses 600000 bytes deep (6000-byte local arrays;
 * the rig agent gives a command 262144 bytes of stack) so that ixemul has to
 * extend it. Prints [OK] only when it did and the mask is still {SIGUSR1}
 * everywhere in the recursion. */
#include <stdio.h>
#include <signal.h>

unsigned long __stack = 20000;

static unsigned long hops, wrong, firstwrong;
static char *lowest, *highest;

static int dive(int depth, int fill)
{
    char pad[6000];
    int i, sum = 0;

    if (!lowest || pad < lowest)
        lowest = pad;
    if (!highest || pad > highest)
        highest = pad;
    /* fill: every byte of the stack the recursion will use gets a pattern that
     * is not the mask, so a missing save in the glue's slot cannot restore the
     * right value by leftover accident (sigprocmask() leaves it nearby) */
    for (i = 0; i < (int)sizeof pad; i++)
        pad[i] = fill ? (char)0xA5 : (char)(depth + i);
    hops++;
    if (!fill) {
        sigset_t cur;
        sigprocmask(SIG_BLOCK, NULL, &cur);
        if (cur != (1UL << (SIGUSR1 - 1)) && !wrong++)
            firstwrong = (unsigned long)cur;
    }
    if (depth > 0)
        sum = dive(depth - 1, fill);
    for (i = 0; i < (int)sizeof pad; i += 512)
        sum += pad[i];
    return sum;
}

int main(void)
{
    sigset_t m, usr1;
    int bad = 0;

    dive(100, 1);
    hops = 0;
    lowest = highest = NULL;
    sigemptyset(&usr1);
    sigaddset(&usr1, SIGUSR1);
    sigprocmask(SIG_SETMASK, &usr1, NULL);

    dive(100, 0);

    sigprocmask(SIG_BLOCK, NULL, &m);
    printf("masks wrong inside the recursion: %lu (first 0x%lx)\n", wrong, firstwrong);
    printf("recursed %lu frames over %ld bytes, mask 0x%lx (want 0x%lx)\n", hops,
           (long)(highest - lowest), (unsigned long)m, (unsigned long)usr1);
    if (highest - lowest < 400000) {
        printf("[FAIL] the recursion did not leave the process stack (the rig agent gives its commands 262144 bytes): no extension was made\n");
        bad = 1;
    } else if (m != usr1) {
        printf("[FAIL] the stack extension changed the signal mask\n");
        bad = 1;
    }
    if (!bad)
        printf("[OK] signal mask intact after the stack extension\n");
    return bad;
}
