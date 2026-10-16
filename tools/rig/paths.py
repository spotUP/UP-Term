"""Where the sibling checkouts live: the one reader of UPTERM_ROOT.

Also the one reader of the rig's input paths (UPTERM_ROM, UPTERM_SYSTEM_HDF,
UPTERM_OS32_TREE, UPTERM_OS32_ROM).

UPTERM_ROOT is the workspace directory that holds vtcon, ixemul-vtcon,
tmux-amiga, ... side by side (the meta-repo `upterm` creates it). Default:
the parent of this vtcon checkout. The Makefile's default is the same
(`UPTERM_ROOT ?= $(abspath $(CURDIR)/..)`).
"""
import contextlib
import fcntl
import os
import pathlib
import subprocess
import sys

VTCON = pathlib.Path(__file__).resolve().parents[2]
UPTERM_ROOT = pathlib.Path(os.environ.get("UPTERM_ROOT") or VTCON.parent)
# the rig's own UP-Term: drawer in the host drawer VTC: (the Workbench labels a drawer icon with the file name,
# so the owner sees Apps); rig.py, the rig scripts and their tests all take it from here
UPTERM_DRAWER = "Apps"
OLD_UPTERM_DRAWER = "upterm"   # its name until 2026-10-14; rig.py install renames it


def code_root(upterm_root=None):
    """The directory that holds the projects which are not UP-Term repos
    (DCTelnet/, AmiExpress/, UpRough/). CODE_ROOT if set, else the nearest
    ancestor of UPTERM_ROOT (itself included) that contains DCTelnet/, else
    UPTERM_ROOT. Right in both layouts: UPTERM_ROOT = ~/Code (flat) and
    UPTERM_ROOT = ~/Code/UP-Term (umbrella). The Makefile asks this function
    (`python3 -I tools/rig/paths.py code-root`), so the rule lives here once."""
    if os.environ.get("CODE_ROOT"):
        return pathlib.Path(os.environ["CODE_ROOT"])
    root = pathlib.Path(upterm_root or UPTERM_ROOT).resolve()
    for d in (root, *root.parents):
        if (d / "DCTelnet").is_dir():
            return d
    return root


CODE_ROOT = code_root()


def _main_checkout(checkout):
    """The main checkout of the repository `checkout` belongs to: `checkout`
    itself, or for a git worktree the checkout that owns its .git."""
    try:
        common = subprocess.run(
            ["git", "-C", str(checkout), "rev-parse", "--path-format=absolute", "--git-common-dir"],
            capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return checkout
    return pathlib.Path(common).resolve().parent


# The rigs are machine resources (an FS-UAE each, a fixed wasabid port, a
# 1.5 GB system disk), so they live in the main checkout's build/ for every
# worktree. A worktree has no build/rig2: FS-UAE started from one booted with
# no bootable device (2026-10-12).
RIG_HOME = _main_checkout(VTCON) / "build"

# Which rig: UPTERM_RIG unset or 1 is the first (build/rig, wasabid port 1234);
# UPTERM_RIG=2 is the second (build/rig2, port 1235), a separate FS-UAE with
# its own system disk copy. Every rig script reads the instance from here.
RIG_N = int(os.environ.get("UPTERM_RIG") or 1)
RIG_NAME = "rig" if RIG_N == 1 else "rig%d" % RIG_N
RIG = RIG_HOME / RIG_NAME


def wasabi_port(rig_n=None):
    """The TCP/UDP port wasabid listens on in rig `rig_n` (this one by default): wasabid's own default 1234 for
    rig 1, +1 per rig. FS-UAE's bsdsocket passes the Amiga's sockets to the Mac's, so two rigs on one port clash.
    The one place the rule lives: rig.py (boot), wasabi.py, ami.py and the MCP wrapper read it from here."""
    return 1234 + (RIG_N if rig_n is None else rig_n) - 1


WASABI_PORT = wasabi_port()
WASABI_KEY = "rigwasabi"   # the rig key (wasabid's ENV:wasabi.key in the rig). Not a secret.
os.environ.setdefault("AMI_PORT", str(WASABI_PORT))   # ami.py reads it


def repo(name):
    """The checkout of repository `name` in the workspace."""
    return UPTERM_ROOT / name


# The default Kickstart 3.1 (40.068) ROM; UPTERM_ROM names another file.
UPROUGH_ROOT = pathlib.Path(os.environ.get("UPROUGH_ROOT") or CODE_ROOT / "UpRough")
KICKSTART = pathlib.Path(os.environ.get("UPTERM_ROM") or
    UPROUGH_ROOT / "Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200")


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

# UPTERM_FSUAE  the emulator the rig boots: an FS-UAE.app bundle (opened with
# `open -a`) or a bare fs-uae binary (run directly), e.g. a local build that
# fixes the arm64 FPU Inf bugs (fmove.x Inf, fcmp Inf,Inf)
# Default: that local build when present (CODE_ROOT/Emulators/fs-uae, branch
# fix/arm64-fpu-inf; tools/rig/fpu_inf_rig.py proves it), else the stock app.
_FSUAE_LOCAL = CODE_ROOT / "Emulators/fs-uae/fs-uae"
FSUAE = pathlib.Path(os.environ.get("UPTERM_FSUAE") or
    (_FSUAE_LOCAL if _FSUAE_LOCAL.exists() else "/Applications/FS-UAE.app"))


# UPTERM_MUTOOLS    the NDK 3.2R4 MuTools drawer (MuForce, disassembler.library), for `rig.py muforce`
# UPTERM_MMULIB     Thomas Richter's mmu.library (not in the NDK: its Developer Distribution only documents it)
# Licence (THOR-Software Licence v3, MuForce.guide): free to distribute, not for payment, no modified
# redistribution. So the rig installs them from these paths and no repository or kit archive holds them.
MUTOOLS = pathlib.Path(os.environ.get("UPTERM_MUTOOLS") or
    pathlib.Path.home() / "Downloads/NDK3.2R4/MuTools")
MMULIB = pathlib.Path(os.environ.get("UPTERM_MMULIB") or
    pathlib.Path.home() / "Library/CloudStorage/Dropbox/WB/Libs/mmu.library")


# --os <name>: a clean installed Workbench image per OS (the owner's, read
# only: the rig boots its own copy) and the Kickstart that goes with it.
# UPTERM_OS_IMAGES  the drawer of Workbench_<version>.hdf files
# UPTERM_ROM314     the A1200 Kickstart 3.1.4 (46.143), for --os 314
OS_IMAGES = pathlib.Path(os.environ.get("UPTERM_OS_IMAGES") or
    pathlib.Path.home() / "Downloads/clean-wb/Clean Workbench Installs")
ROM314 = pathlib.Path(os.environ.get("UPTERM_ROM314") or
    pathlib.Path.home() / "Downloads/kickstart 3.1.4/kick.a1200.46.143")


# --os with --060/--max: the clean system has no 68060.library and its
# Startup-Sequence (SetPatch, CPU CHECKINSTALL) fails on a 68060 without it
# (Software Failure 8000 888B, rig 3, 2026-10-10); UPTERM_LIB68060 names the file (default: Phase5's 68060.library 44.3,
# taken from the old rig system disk's Libs:; GG-Lite's 68060.library did not help).
LIB68060 = pathlib.Path(os.environ.get("UPTERM_LIB68060") or
    RIG_HOME / "rig-inputs/68060.library")


# ... and the 3.1 68040.library, which SetPatch also loads on a 68060, crashes
# there; the old rig system disk has a 692-byte stub in its place (UPTERM_LIB68040).
LIB68040 = pathlib.Path(os.environ.get("UPTERM_LIB68040") or RIG_HOME / "rig-inputs/68040.library")


# the clean 3.0/3.1 installs have no Installer; Installer 43.3 (30.4.96), the 3.1 Install disk's (UPTERM_INSTALLER)
INSTALLER = pathlib.Path(os.environ.get("UPTERM_INSTALLER") or RIG_HOME / "rig-inputs/Installer")


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


@contextlib.contextmanager
def rig_lock(rig_dir=None):
    """Hold the rig for this run: an exclusive flock on <rig dir>/rig.lock, released when
    the block ends or the process dies (the kernel drops it). A rig is one FS-UAE with one
    amiagent port, so two scripts on it corrupt each other's state; the second stops with
    the holder's pid instead (one agent per rig). `rig_dir` defaults to this instance's."""
    d = pathlib.Path(rig_dir) if rig_dir else RIG
    if os.environ.get("UPTERM_RIG_LOCK_HELD") == str(d):
        yield   # a script run by the holder (assign_rig runs install_rig): the holder has it
        return
    d.mkdir(parents=True, exist_ok=True)
    f = open(d / "rig.lock", "a+")
    try:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            f.seek(0)
            holder = f.read().strip()
            raise SystemExit("rig: %s is in use by pid %s (%s); one script per rig. "
                             "Wait for it, or use the other rig (UPTERM_RIG=2)."
                             % (d.name, holder.split(None, 1)[0] if holder else "unknown", holder[:120] or "no details"))
        f.seek(0)
        f.truncate()
        f.write("%d %s\n" % (os.getpid(), " ".join(sys.argv)[:200]))
        f.flush()
        os.environ["UPTERM_RIG_LOCK_HELD"] = str(d)
        yield
    finally:
        if os.environ.get("UPTERM_RIG_LOCK_HELD") == str(d):
            del os.environ["UPTERM_RIG_LOCK_HELD"]
        f.close()


if __name__ == "__main__" and sys.argv[1:] == ["code-root"]:
    print(CODE_ROOT)
