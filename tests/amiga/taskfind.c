/* taskfind NAME -- is a task or process of that name running (exec's
 * FindTask)? Prints "found" or "none": the rig checks that a worker such
 * as "vtcon watch" ends with its window. */
#include <stdio.h>
#include <proto/exec.h>

int main(int argc, char **argv)
{
    struct Task *t;
    if (argc < 2)
        return 20;
    Forbid();
    t = FindTask((STRPTR)argv[1]);
    Permit();
    printf("%s\n", t ? "found" : "none");
    return t ? 0 : 5;
}
