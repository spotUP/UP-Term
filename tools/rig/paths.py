"""Where the sibling checkouts live: the one reader of UPTERM_ROOT.

Also the one reader of the rig's input paths (UPTERM_ROM, UPTERM_SYSTEM_HDF,
UPTERM_OS32_TREE, UPTERM_OS32_ROM).

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


# Which rig: UPTERM_RIG unset or 1 is the first (build/rig, amiagent port 7846);
# UPTERM_RIG=2 is the second (build/rig2, port 7847), a separate FS-UAE with
# its own system disk copy. Every rig script reads the instance from here.
RIG_N = int(os.environ.get("UPTERM_RIG") or 1)
RIG_NAME = "rig" if RIG_N == 1 else "rig%d" % RIG_N
RIG = VTCON / "build" / RIG_NAME
AGENT_PORT = 7846 + RIG_N - 1
os.environ.setdefault("AMI_PORT", str(AGENT_PORT))   # ami.py reads it


def repo(name):
    """The checkout of repository `name` in the workspace."""
    return UPTERM_ROOT / name


# The default Kickstart 3.1 (40.068) ROM; UPTERM_ROM names another file.
KICKSTART = pathlib.Path(os.environ.get("UPTERM_ROM") or
    UPTERM_ROOT / "Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200")


# The rest of the rig's inputs, each an environment variable with the owner's
# path as the default (the README says where each comes from):
# UPTERM_SYSTEM_HDF  the Workbench 3.x hard-disk image copied once to build/rig/sys.hdf
# UPTERM_OS32_TREE   an installed AmigaOS 3.2 tree (a host drawer), for --os32
# UPTERM_OS32_ROM    the Kickstart 3.2.3 ROM, for --os32
SYSTEM_HDF = pathlib.Path(os.environ.get("UPTERM_SYSTEM_HDF") or
    pathlib.Path.home() / "Downloads/nyhd2.hdf")
OS32_TREE = pathlib.Path(os.environ.get("UPTERM_OS32_TREE") or
    pathlib.Path.home() / "Downloads/AmigaOS-3.2-full (1)")
OS32_ROM = pathlib.Path(os.environ.get("UPTERM_OS32_ROM") or
    pathlib.Path.home() / "Desktop/KICK_323.rom")


def require_input(path, var, what):
    """Exit with one clear message when a rig input is missing."""
    if not pathlib.Path(path).exists():
        sys.exit("rig: %s not found: %s (set %s=<path>; tools/rig/README.md says "
                 "where it comes from)" % (what, path, var))


def require_rom(path):
    """Exit with one clear message when the Kickstart ROM file is missing."""
    if not pathlib.Path(path).is_file():
        sys.exit("rig: Kickstart ROM not found: %s (set UPTERM_ROM=<file> or "
                 "pass --kick <file>)" % path)
