"""Where the sibling checkouts live: the one reader of UPTERM_ROOT.

UPTERM_ROOT is the workspace directory that holds vtcon, ixemul-vtcon,
tmux-amiga, ... side by side (the meta-repo `upterm` creates it). Default:
the parent of this vtcon checkout. The Makefile's default is the same
(`UPTERM_ROOT ?= $(abspath ..)`).
"""
import os
import pathlib
import sys

VTCON = pathlib.Path(__file__).resolve().parents[2]
UPTERM_ROOT = pathlib.Path(os.environ.get("UPTERM_ROOT") or VTCON.parent)


def repo(name):
    """The checkout of repository `name` in the workspace."""
    return UPTERM_ROOT / name


# The default Kickstart 3.1 (40.068) ROM; UPTERM_ROM names another file.
KICKSTART = pathlib.Path(os.environ.get("UPTERM_ROM") or
    UPTERM_ROOT / "Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200")


def require_rom(path):
    """Exit with one clear message when the Kickstart ROM file is missing."""
    if not pathlib.Path(path).is_file():
        sys.exit("rig: Kickstart ROM not found: %s (set UPTERM_ROM=<file> or "
                 "pass --kick <file>)" % path)
