"""Where the sibling checkouts live: the one reader of UPTERM_ROOT.

Also the one reader of the rig's input paths (UPTERM_ROM, UPTERM_SYSTEM_HDF,
UPTERM_OS32_TREE, UPTERM_OS32_ROM).

UPTERM_ROOT is the workspace directory that holds vtcon, ixemul-vtcon,
tmux-amiga, ... side by side (the meta-repo `upterm` creates it). Default:
the parent of the main vtcon checkout (a git worktree's parent is not it). The Makefile's default is the same
(`UPTERM_ROOT ?= $(abspath $(CURDIR)/..)`).
"""
import contextlib
import fcntl
import os
import pathlib
import subprocess
import sys

VTCON = pathlib.Path(__file__).resolve().parents[2]


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


MAIN_CHECKOUT = _main_checkout(VTCON)
UPTERM_ROOT = pathlib.Path(os.environ.get("UPTERM_ROOT") or MAIN_CHECKOUT.parent)
# third-party SDKs and archives (gitignored, so a worktree has none): this checkout's vendor/ if it has one, else the
# main checkout's. The Makefile asks (`paths.py vendor`).
VENDOR = VTCON / "vendor" if (VTCON / "vendor").is_dir() else MAIN_CHECKOUT / "vendor"
# the rig's own UP-Term: drawer in the host drawer VTC: (the Workbench labels a drawer icon with the file name,
# so the owner sees Apps); rig.py, the rig scripts and their tests all take it from here
UPTERM_DRAWER = "Apps"
# where stage_upterm puts the built binaries and where the rig scripts start them from: the rig drawer, not
# UP-Term:, which a kit install moves (rig 4: UP-Term: was Rushhours:Tools/UP-Term, the binaries in VTC:Apps/bin)
UPTERM_BIN = "VTC:%s/bin" % UPTERM_DRAWER
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


# The rigs are machine resources (an FS-UAE each, a fixed wasabid port, a
# 1.5 GB system disk), so they live in the main checkout's build/ for every
# worktree. A worktree has no build/rig2: FS-UAE started from one booted with
# no bootable device (2026-10-12).
RIG_HOME = MAIN_CHECKOUT / "build"

# Which rig: UPTERM_RIG unset or 1 is the first (build/rig, wasabid port 1234);
# UPTERM_RIG=2 is the second (build/rig2, port 1235), a separate FS-UAE with
# its own system disk copy. Every rig script reads the instance from here.
RIG_N = int(os.environ.get("UPTERM_RIG") or 1)
RIG_NAME = "rig" if RIG_N == 1 else "rig%d" % RIG_N
RIG = RIG_HOME / RIG_NAME

# Verdicts are results of THIS checkout's files (their fingerprints name its sources), not a machine
# resource: they live in the build/ of the checkout the script runs from, so a worktree's run never writes
# into the main checkout (2026-10-19: tutorial_rig wrote into a peer's build/rig4).
RESULTS = VTCON / "build" / RIG_NAME

# FS-UAE's own log (option logs_dir) goes inside the rig's dir, never into the shared
# ~/Documents/FS-UAE/Cache/Logs/fs-uae.log.txt: all rigs wrote that one file and it reached 7.9 GB and
# filled the Mac's disk (2026-10-17). The size cap is enforced by cap_fsuae_log(), called at rig start and
# from every ami.req().
FSUAE_LOG_DIR = RIG / "logs"
FSUAE_LOG_MAX = 50 * 1024 * 1024    # bytes on disk a log may hold before it is cut
FSUAE_LOG_TAIL = 1024 * 1024        # the end of a cut log kept, as <name>.tail, for diagnosis


def fsuae_crash(log_dir=None):
    """(exception line, reset count) when the rig's FS-UAE log shows a CPU exception followed by emulated
    CPU resets (a Guru turned into a reset loop: the rig then looks plain "down"), else None. Reads the
    live log and its .tail. The exception is the first "Illegal instruction" / "Exception N" line of the run before the
    "CPU reset" lines that follow it."""
    d = pathlib.Path(log_dir or FSUAE_LOG_DIR)
    text = ""
    for n in ("fs-uae.log.txt.tail", "fs-uae.log.txt"):
        try:
            text += (d / n).read_text(encoding="latin-1")
        except OSError:
            pass
    exc, resets = None, 0
    for line in text.splitlines():
        if line.startswith(("Illegal instruction", "Exception ")):
            if exc is None or resets:
                exc, resets = line.strip(), 0
        elif exc and line.startswith("CPU reset"):
            resets += 1
    return (exc, resets) if exc and resets else None


def cap_fsuae_log(log_dir=None, limit=None, tail=None, extra=()):
    """Cut every file in the rig's FS-UAE log dir that holds more than `limit` bytes on disk: its last `tail`
    bytes go to <name>.tail, the file is truncated in place (FS-UAE keeps it open). Sized by allocated blocks:
    if FS-UAE does not append, a truncated file keeps writing at its old offset and stays large but sparse.
    `extra` names more files to cap the same way (the rig's serial.log). Returns the names cut."""
    d = pathlib.Path(log_dir or FSUAE_LOG_DIR)
    limit = FSUAE_LOG_MAX if limit is None else limit
    tail = FSUAE_LOG_TAIL if tail is None else tail
    cut = []
    try:
        files = [f for f in d.iterdir() if f.is_file() and f.suffix != ".tail"]
    except OSError:
        files = []
    files += [pathlib.Path(f) for f in extra if pathlib.Path(f).is_file()]
    for f in files:
        try:
            st = f.stat()
            if st.st_blocks * 512 <= limit:
                continue
            with open(f, "rb") as r:
                r.seek(max(0, st.st_size - tail))
                data = r.read().lstrip(b"\0")
            f.with_name(f.name + ".tail").write_bytes(data)
            os.truncate(f, 0)
            cut.append(f.name)
        except OSError:
            pass
    return cut


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


# The default Kickstart 3.1 (40.68, A1200) ROM; UPTERM_ROM names another file. It lives in the
# workspace, outside every repository (the repositories are public; a ROM is not ours to
# publish): a copy of the owner's Dropbox ROM, header 40.68,
# sha1 e21545723fe8374e91342617604f1b3d703094f1. The UpRough file named kick40068.A1200 is
# really 40.63 (the A500/A600/A2000 ROM) and is no longer used.
KICKSTART = pathlib.Path(os.environ.get("UPTERM_ROM") or UPTERM_ROOT / "roms/kick40068-a1200.rom")


# The rest of the rig's inputs, each an environment variable with the owner's
# path as the default (the README says where each comes from):
# UPTERM_SYSTEM_HDF  the Workbench 3.x hard-disk image every rig start clones to build/rigN/sys.hdf; outside
#                    ~/Downloads, which macOS keeps from the CLI ("Operation not permitted", 2026-10-24)
# UPTERM_OS32_TREE   an installed AmigaOS 3.2 tree (a host drawer), for --os32
# UPTERM_OS32_ROM    the Kickstart 3.2.3 ROM, for --os32
SYSTEM_HDF = pathlib.Path(os.environ.get("UPTERM_SYSTEM_HDF") or
    UPTERM_ROOT / "images/nyhd2.hdf")
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
# (Software Failure 8000 888B, rig 3, 2026-10-10); UPTERM_LIB68060 names the file. Default: Thor's MMULib 68060.library
# 47.2 (Aminet util/libs/MMULib.lha), which needs its mmu.library and 680x0.library beside it (UPTERM_LIBMMU,
# UPTERM_LIB680X0). Phase5's 68060.library 44.3 (the old rig system disk's) made OS 3.1 on the emulated 060 reset
# in a loop once SetPatch had run: 333 resets on a clean 3.1, none with SetPatch commented out, none on MMULib
# (rig 8, 2026-10-26, thoughts/spot/term-findings/reset-loop-cause-rig8-2026-10-26.md). GG-Lite's did not help.
LIB68060 = pathlib.Path(os.environ.get("UPTERM_LIB68060") or
    RIG_HOME / "rig-inputs/mmulib/68060.library")
LIBMMU = pathlib.Path(os.environ.get("UPTERM_LIBMMU") or RIG_HOME / "rig-inputs/mmulib/mmu.library")
LIB680X0 = pathlib.Path(os.environ.get("UPTERM_LIB680X0") or RIG_HOME / "rig-inputs/mmulib/680x0.library")
def libs060():
    """Every Libs: file a 060 with FPU gets, as (source, Libs: name, variable) for require_input; read at call time,
    so a test that points LIB68060 elsewhere is followed."""
    return ((LIB68060, "68060.library", "UPTERM_LIB68060"),
            (LIBMMU, "mmu.library", "UPTERM_LIBMMU"),
            (LIB680X0, "680x0.library", "UPTERM_LIB680X0"))


# ... and the 3.1 68040.library, which SetPatch also loads on a 68060, crashes
# there; the old rig system disk has a 692-byte stub in its place (UPTERM_LIB68040).
LIB68040 = pathlib.Path(os.environ.get("UPTERM_LIB68040") or RIG_HOME / "rig-inputs/68040.library")


# the clean 3.0/3.1 installs have no Installer; Installer 43.3 (30.4.96), the 3.1 Install disk's (UPTERM_INSTALLER)
INSTALLER = pathlib.Path(os.environ.get("UPTERM_INSTALLER") or RIG_HOME / "rig-inputs/Installer")


# the patched ixemul.library the kit ships (the Makefile's IXEMUL_LIB, IXEMUL_LIB_SOFT, IXNET_LIB): the FPU build,
# the soft-float one for a machine with no FPU, and ixnet.library from the same tree (ixnet_open refuses an
# ixemul of another revision, so the two always travel together). rig.py start puts them in VTC:ixp6.
IXEMUL_LIB = pathlib.Path(os.environ.get("IXEMUL") or
    repo("ixemul-vtcon") / "build295/library/68020/68881/amigaos/ixemul.library")
IXEMUL_LIB_SOFT = IXEMUL_LIB.parents[2] / "soft-float/amigaos/ixemul.library"
IXNET_LIB = pathlib.Path(os.environ.get("IXNET") or IXEMUL_LIB.parents[4] / "ixnet/68020/amigaos/ixnet.library")


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
elif __name__ == "__main__" and sys.argv[1:] == ["main-checkout"]:
    print(MAIN_CHECKOUT)
elif __name__ == "__main__" and sys.argv[1:] == ["vendor"]:
    print(VENDOR)
