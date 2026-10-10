#!/usr/bin/env python3
"""kitmodes.py -- one rule for the mode of every file `make dist` stages.

  python3 tools/kitmodes.py <kit drawer>

lha keeps a file's Unix mode and the Amiga reads it as protection bits: a file
without u+w arrives without w and d, so the next Install or an Update cannot
copy over it and Uninstall cannot delete it (less's LICENSE and COPYING came
from its upstream source as 0444, 2026-10-09). So every file is made
rw-r--r--, plus x (the Amiga's e) for a program only: an Amiga hunk
executable (it starts with 0x000003F3) or a script starting with #!. A text
file with x (coreutils' COPYING was 0755) loses it. Drawers become rwxr-xr-x.
Exits 1, naming them, if a file is still without u+w afterwards."""
import os, stat, sys

HUNK = b'\x00\x00\x03\xf3'


def wants_x(path):
    with open(path, 'rb') as f:
        head = f.read(4)
    return head == HUNK or head[:2] == b'#!'


def normalise(kit):
    """Set every mode under kit by the rule above; returns [(path, old, new)] of those changed."""
    changed = []
    for d, dirs, files in os.walk(kit):
        for name in dirs:
            p = os.path.join(d, name)
            if os.path.islink(p):
                continue
            old = stat.S_IMODE(os.lstat(p).st_mode)
            if old != 0o755:
                os.chmod(p, 0o755)
                changed.append((p, old, 0o755))
        for name in files:
            p = os.path.join(d, name)
            if os.path.islink(p):
                continue
            old = stat.S_IMODE(os.lstat(p).st_mode)
            if not old & stat.S_IRUSR:
                os.chmod(p, old | stat.S_IRUSR)   # to read its head
            new = 0o755 if wants_x(p) else 0o644
            if old != new:
                os.chmod(p, new)
                changed.append((p, old, new))
    return changed


def unwritable(kit):
    return [os.path.join(d, n) for d, _, fs in os.walk(kit) for n in fs
            if not os.path.islink(os.path.join(d, n)) and not os.lstat(os.path.join(d, n)).st_mode & stat.S_IWUSR]


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    kit = sys.argv[1]
    for p, old, new in normalise(kit):
        print('kitmodes: %s %o -> %o' % (os.path.relpath(p, kit), old, new))
    bad = unwritable(kit)
    if bad:
        print('kitmodes: still without u+w (the Amiga could not replace them):', *bad, sep='\n  ')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
