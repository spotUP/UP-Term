/* libpatch.h - the one finish step after a hand-written jump-table change (Amiga build only).
 *
 * A vector changed by writing the JMP operand directly (flat_swap, wbreset_patch) leaves the library's checksum
 * stale: exec's periodic check then raises RECOVERABLE ALERT #01000003 (library checksum failure) in whatever task
 * runs it. SetFunction avoids this with the same three steps: LIBF_CHANGED, flush the caches, SumLibrary. Call this
 * under Forbid(), after the write. Every hand patch of a library uses it; no second copy. */
#ifndef AMARCHY_LIBPATCH_H
#define AMARCHY_LIBPATCH_H

#include <exec/libraries.h>
#include <proto/exec.h>

static void libpatch_done(struct Library *lib)
{
    lib->lib_Flags |= LIBF_CHANGED;
    CacheClearU();
    SumLibrary(lib);
    CacheClearU();
}

#endif
