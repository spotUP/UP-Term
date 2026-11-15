#!/usr/bin/env python3
"""Patch the Directory Opus 5.104 binary's IPrefs task-name literal in place (plan thoughts/shared/research/2026-11-15-dopus-theme-reopen.md,
"Cause found"): upstream's source lost the Latin-1 guillemets (EF BF BD = U+FFFD), so check_closescreen never recognises
IPrefs' reset requester and the Workbench reset deadlocks. Writes AB 20 'IPrefs' 20 BB 00, rest of the 14 bytes 00; size and
offsets unchanged. Exactly one bad literal or already patched (one good, none bad), else [ERROR] and exit 1.
The "<<sysihack>>" FindTask literal has the same damage and is left alone (no effect on the reset).
usage: dopus_iprefs_patch.py <DirectoryOpus> ..."""
import sys

BAD = b"\xef\xbf\xbd IPrefs \xef\xbf\xbd\x00"
GOOD = b"\xab IPrefs \xbb\x00"
NEW = GOOD + b"\x00" * (len(BAD) - len(GOOD))


def patch(data: bytes) -> bytes:
    nb, ng = data.count(BAD), data.count(GOOD)
    if nb == 0 and ng == 1:
        return data
    if nb != 1 or ng != 0:
        raise ValueError("expected exactly one IPrefs literal, found %d damaged and %d good" % (nb, ng))
    return data.replace(BAD, NEW)


if __name__ == "__main__":
    for p in sys.argv[1:]:
        try:
            old = open(p, "rb").read()
            new = patch(old)
            if new != old:
                open(p, "wb").write(new)
        except (OSError, ValueError) as e:
            print("[ERROR] %s: %s" % (p, e)); sys.exit(1)
        print("[OK] %s: IPrefs literal Latin-1" % p)
