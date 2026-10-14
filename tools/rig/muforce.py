"""MuForce on a rig: `rig.py muforce on|off|log`. Crash feedback for the rigs (inventory Need 4).

MuForce (Thomas Richter's Enforcer for the MMU, NDK 3.2R4 MuTools) reports every access to address 0 or to
non-existent memory, and writes to ROM, with the registers, the PC and the task's name. Its FILE option
(MuForce.guide, "Option: FILE") sends the report to a file; here the file is VTC:muforce/hits.log, which is
the host drawer <rig>/vtc/muforce/hits.log, so the Mac and an agent read it without amiagent. (The guide
warns that a file on a disk can be damaged by the crash that follows a hit; VTC: is a host drawer.)

  rig.py muforce on           install MuForce + mmu.library + disassembler.library into VTC:muforce, start MuForce
  rig.py muforce off          MuForce QUIT, take VTC:muforce out of LIBS:
  rig.py muforce log [--raw]  one line per hit (access, address, PC, task); --raw is the file itself

Needs an MMU: the rig must be started with `rig.py start --mmu` (FS-UAE "68060" instead of 68060-NOMMU;
slower, no JIT). mmu.library 42.8 is not in the NDK (its MuManual is the Developer Distribution, the library
is the MMULib archive): UPTERM_MMULIB names it. UPTERM_MUTOOLS names the MuTools drawer.
Licence: THOR-Software Licence v3 -- free to distribute, no payment, no modified redistribution; nothing of it
is committed or put into the kit, the rig copies it from the Mac.
"""
import os
import pathlib
import re
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths

DRAWER = "muforce"                     # VTC:muforce, <rig>/vtc/muforce
LOG = "hits.log"
FILES = (("MuForce", "MuForce"), ("disassembler.library", "disassembler.library"))
USAGE = "usage: rig.py muforce on | off | log [--raw]"


def parse_args(argv):
    """(command, options) of `rig.py muforce ...` (argv after the word muforce); SystemExit with the usage
    line for anything else."""
    if not argv or argv[0] not in ("on", "off", "log"):
        raise SystemExit(USAGE)
    cmd, rest = argv[0], argv[1:]
    allowed = {"on": (), "off": (), "log": ("--raw",)}[cmd]
    bad = [a for a in rest if a not in allowed]
    if bad:
        raise SystemExit("rig.py muforce %s: unknown argument %s\n%s" % (cmd, bad[0], USAGE))
    return cmd, {a.lstrip("-") for a in rest if a in allowed}


def has_mmu(config_text):
    """True when the rig's FS-UAE config gives the CPU an MMU: a 68030/68040/68060 without the -NOMMU suffix."""
    m = re.search(r"^cpu\s*=\s*(\S+)", config_text, re.M)
    return bool(m) and m.group(1).upper() in ("68030", "68040", "68060")


_ACCESS = re.compile(r"^\s*(BYTE|WORD|LONG)\s+(READ|WRITE)\s+(?:from|to)\s+([0-9A-Fa-f]{8})", re.M)
_PC = re.compile(r"\bPC:\s*([0-9A-Fa-f]{8})")
_NAME = re.compile(r'^\s*Name:\s*"([^"]*)"(?:\s+CLI:\s*"([^"]*)")?', re.M)


def parse_hits(text):
    """The hits in a MuForce report: a list of dicts {kind: "LONG READ", size, rw, address, pc, task, cli}
    (hex strings upper-cased, 8 digits). A hit starts at its "<SIZE> <READ|WRITE> from|to <address>" line and
    runs to the next one; the PC line is on the same line or the next, "Name:" is its last line. Alert hits
    and anything else are ignored."""
    hits = []
    marks = list(_ACCESS.finditer(text))
    for i, m in enumerate(marks):
        block = text[m.start(): marks[i + 1].start() if i + 1 < len(marks) else len(text)]
        pc = _PC.search(block)
        nm = _NAME.search(block)
        hits.append({"kind": "%s %s" % (m.group(1), m.group(2)), "size": m.group(1), "rw": m.group(2),
                     "address": m.group(3).upper(), "pc": pc.group(1).upper() if pc else "",
                     "task": nm.group(1) if nm else "", "cli": (nm.group(2) or "") if nm else ""})
    return hits


def summary(hits):
    """One line per hit; 'no hits' when there are none."""
    if not hits:
        return "no hits"
    return "\n".join("hit %d: %s address %s PC %s task '%s'%s" % (
        i + 1, h["kind"], h["address"], h["pc"], h["task"], " cli '%s'" % h["cli"] if h["cli"] else "")
        for i, h in enumerate(hits))


def drawer():
    return paths.RIG / "vtc" / DRAWER


def run(cmd, timeout=30):
    import ami
    b = ami.req(0x02, struct.pack(">H", timeout) + cmd.encode("latin-1"))
    return struct.unpack(">I", b[:4])[0], b[4:].decode("latin-1")


def stage():
    """Copy MuForce, disassembler.library, mmu.library and build/amiga/muhit (when built) to <rig>/vtc/muforce."""
    d = drawer()
    d.mkdir(parents=True, exist_ok=True)
    srcs = [(paths.MUTOOLS / a, d / b) for a, b in FILES] + [(paths.MMULIB, d / "mmu.library")]
    missing = [str(s) for s, _ in srcs if not s.is_file()]
    if missing:
        raise SystemExit("rig muforce: not found: %s (UPTERM_MUTOOLS = NDK3.2R4/MuTools, UPTERM_MMULIB = mmu.library)"
                         % ", ".join(missing))
    for s, t in srcs:
        shutil.copyfile(s, t)
    probe = paths.VTCON / "build/amiga/muhit"
    if probe.is_file():
        shutil.copyfile(probe, paths.RIG / "vtc/muhit")
    return [t for _, t in srcs]


def running():
    rc, out = run("Status", 15)
    return "MuForce" in out


def on():
    cfg = paths.RIG / "vtcon-rig.fs-uae"
    if not cfg.is_file() or not has_mmu(cfg.read_text()):
        raise SystemExit("rig muforce: %s has no MMU (cpu = 68060-NOMMU). Restart the rig with `rig.py start --mmu`." % paths.RIG.name)
    with paths.rig_lock():
        if running():
            print("MuForce is already running")
            return
        stage()
        run("C:Delete >NIL: VTC:%s/%s" % (DRAWER, LOG))
        run("C:Assign >NIL: LIBS: VTC:%s ADD" % DRAWER)
        run("C:Run >NIL: <NIL: VTC:%s/MuForce FILE=VTC:%s/%s DATESTAMP" % (DRAWER, DRAWER, LOG))
        import time
        for _ in range(20):
            time.sleep(1)
            if running():
                print("MuForce on; hits go to %s" % (drawer() / LOG))
                return
        raise SystemExit("rig muforce: MuForce did not start (no mmu.library 40+ or no MMU in this rig?)")


def off():
    with paths.rig_lock():
        run("VTC:%s/MuForce QUIT" % DRAWER)
        run("C:Assign >NIL: LIBS: VTC:%s REMOVE" % DRAWER)
        print("MuForce off" if not running() else "[ERROR] MuForce still running")


def log(opts):
    f = drawer() / LOG
    if not f.is_file():
        raise SystemExit("rig muforce: no %s (muforce on first)" % f)
    text = f.read_text(encoding="latin-1")
    print(text if "raw" in opts else summary(parse_hits(text)))


def main(argv):
    cmd, opts = parse_args(argv)
    {"on": on, "off": off, "log": lambda: log(opts)}[cmd]()
