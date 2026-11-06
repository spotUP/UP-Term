#!/usr/bin/env python3
"""test_nobreak.py -- a Break during file work must not end Amarchy behind its cleanup.

  python3 tests/test_nobreak.py build/amiga/Amarchy build/amiga/amarchy-gui build/amiga/amarchy-flat

The vbcc library's _chkabort (called from every buffered stdio read/write) clears a pending Ctrl-C and calls
exit(20), skipping the program's own exit path (patches, semaphores, ports, broker stay armed; the next start
crashes). Programs that handle Ctrl-C themselves and hold system state link amarchy/nobreak.c (an empty
_chkabort). This check fails when a given binary still contains the library unit's code.
The list of binaries is CHKABORT_PROGRAMS in the Makefile (make chkabort-check). Exit 1 on a hit.
See thoughts/shared/research/s61-evidence/restart-crash-chkabort.md."""
import glob, os, re, subprocess, sys

# Code of vc.lib's _chkabort unit; None = relocated long (masked).
PATTERN = ['2f0e', '2c79', None, None, '7000', '223c', '0000', '1000', '4eae', 'fece', 'c0bc', '0000', '1000',
           '670c', '4878', '0014', '4eb9', None, None]


def library_pattern():
    """Path of the installed vc.lib, or None. Used to confirm PATTERN still matches the library unit."""
    try:
        prefix = subprocess.run(['brew', '--prefix', 'vbcc'], capture_output=True, text=True, timeout=30).stdout.strip()
    except Exception:
        return None
    libs = glob.glob(os.path.join(prefix, 'targets', 'm68k-amigaos', 'lib', 'vc.lib'))
    return libs[0] if libs else None


def regex():
    out = b''
    for w in PATTERN:
        out += b'..' if w is None else re.escape(bytes.fromhex(w))
    return re.compile(out, re.S)


def main(paths):
    lib = library_pattern()
    rx = regex()
    if lib:
        with open(lib, 'rb') as f:
            if not rx.search(f.read()):
                print('[FAIL] the installed vc.lib no longer holds the known _chkabort code; update PATTERN in', __file__)
                return 1
    bad = 0
    for p in paths:
        with open(p, 'rb') as f:
            m = rx.search(f.read())
        if m:
            print('[FAIL] %s holds the library _chkabort (offset 0x%X): a Break during file work ends it with exit(20) '
                  'behind its cleanup; link amarchy/nobreak.c' % (p, m.start()))
            bad = 1
        else:
            print('[OK] %s has no library _chkabort' % p)
    return bad


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
