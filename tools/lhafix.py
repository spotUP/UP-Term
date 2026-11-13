#!/usr/bin/env python3
"""Store again, uncompressed (-lh0-), every member of an lha archive that 7-Zip cannot decode.

usage: tools/lhafix.py ARCHIVE CWD

CWD is the directory the archive was packed from (member paths are relative to it).

Why: LHa for UNIX 1.14i-ac20220213 can emit an -lh5-/-lh6-/-lh7- stream whose back-reference
points before the start of the file (up-term/Files/ixkill in v1.1). lha's own decoder tolerates it;
7-Zip and other strict decoders report a data error. shortcut: stores the failing members, upgrade
once the encoder is fixed or replaced (vtcon/thoughts/spot/todos.md, packer root cause).
"""
import os, re, subprocess, sys


def bad_members(archive):
    r = subprocess.run(['7zz', 't', archive], capture_output=True, text=True)
    bad = re.findall(r'^ERROR: Data Error : (.+)$', r.stdout + r.stderr, re.M)
    if r.returncode != 0 and not bad:
        sys.exit('lhafix: 7zz t %s failed (rc %d) without a member name:\n%s' % (archive, r.returncode, r.stdout[-2000:] + r.stderr))
    return bad


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    archive, cwd = os.path.abspath(argv[0]), argv[1]
    bad = bad_members(archive)
    if not bad:
        return
    print('lhafix: storing uncompressed in %s: %s' % (archive, ' '.join(bad)))
    subprocess.run(['lha', '-aqz', archive] + bad, check=True, cwd=cwd)
    left = bad_members(archive)
    if left:
        sys.exit('lhafix: %s still fails 7zz t: %s' % (archive, ' '.join(left)))


if __name__ == '__main__':
    main(sys.argv[1:])
