#!/usr/bin/env python3
"""libguard.py -- the one check for an ixnet.library that must not ship or be staged.

Before ixemul-vtcon PR 5 (d1fa642) ixnet opened "AmiTCP:libs/usergroup.library" at every open. With no
AmiTCP: assign every ixemul program stops on ramlib's "Please insert volume AmiTCP:" requester (2026-10-27:
all rigs; 2026-10-28: a kit install copied md5 7a43f99c... into SYS:Libs). Callers: tools/rig/rig.py
(stage_ixemul), tools/mkkit.py (every archive that carries Files/libs/ixnet.library), the Makefile's
dist-base and dist-python before they copy it.

  python3 tools/libguard.py <ixnet.library>...     exit 1 and say why when one is bad
"""
import pathlib, sys


def ixnet_problems(path):
    """Why an ixnet.library must not reach a rig or a kit (empty list: it is clean)."""
    return ["opens AmiTCP:libs/usergroup.library"] if b"AmiTCP:libs" in pathlib.Path(path).read_bytes() else []


def require_clean_ixnet(path):
    """Exit with an [ERROR] naming the file and the fix when `path` opens AmiTCP:libs."""
    problems = ixnet_problems(path)
    if problems:
        sys.exit("[ERROR] %s %s: build ixemul-vtcon at or after PR 5 (d1fa642), or set IXEMUL/IXNET to a clean build"
                 % (path, problems[0]))


if __name__ == '__main__':
    for p in sys.argv[1:]:
        require_clean_ixnet(p)
