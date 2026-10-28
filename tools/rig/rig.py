#!/usr/bin/env python3
"""The vtcon test rig: FS-UAE, A1200 (68060 at full speed by default, 68020 with --a1200; KS 3.1 40.068), driven
through wasabid (TCP 1234, tools/rig/ami.py). UPTERM_RIG=2 selects a second,
independent rig: build/rig2, wasabid on 1235 (tools/rig/README.md).

  rig.py setup    copy the system disk once, write the config and boot drawer
  rig.py start    boot it in the background; the default machine is the 68060 + FPU at the host's speed
                  with 1 GB Zorro III RAM (what --max was); --a1200: the stock 68020 at real speed
                  (timing runs only; --stock, --exact and --fast also keep the 020);
                  --060: a 68060 + FPU at the host's speed, 128 MB Z3 (W41);
                  --nofpu: the same machine without an FPU: the default 060 becomes a 68LC060, --a1200 a
                  68020 with no fpu key (what the soft-float ixemul.library is for; --mmu is ignored);
                  --a500: a 68000 A500 at 7 MHz, cycle-exact, 8 MB Zorro II fast (68000 speed, W7);
                  --fast: the CPU at the host's speed; --os32: AmigaOS 3.2 -- the 3.2.3 ROM
                  and the owner's 3.2 install as DH0:, ledger T3;
                  --ro: DH0: read-only, stalls on writes;
                  --kick <file>: another Kickstart ROM for this boot;
                  --serial <path>: the serial port to that path, not serial.log;
                  --no-dopus: Workbench without Directory Opus (no DOPUS.1; the clone gets the stock
                  C/LoadWB and no Libs/workbench.library, the ROM's runs);
                  --idle-minutes <N>: stop the rig after N minutes with no command (default 60, 0 = off, rig 1 never);
                  --purpose "<who: what>": the window title says which session runs the
                  rig and for what, e.g. "Amarchy session: menu terminal check")
                  --overscan: AGA rigs show the whole overscan picture (zoom = full), not the cropped 640x512 area;
                  --os <30|31|314|32|321|322|35|39>: boot the owner's clean installed
                  Workbench of that version (paths.OS_IMAGES; own copy per rig,
                  --fresh copies it anew; the Kickstart goes with it)
  rig.py aga      Workbench on native AGA (PAL hires, 16 colours) from next boot
  rig.py rtg      Workbench back on the graphics card from next boot
  (The screen mode is the Workbench's ScreenMode prefs, not the config: the
  owner's way, 2026-09-29. aga/rtg need the rig up and reboot it.)
  rig.py watchdog the idle watchdog `start` launches (--idle-minutes N, default 60, 0 = off): stops the rig after N
                  minutes with no ami/typewin/wasabi command (paths.LAST_COMMAND); never while a long run holds
                  paths.LONG_RUN (ami.py long, install) or within 5 s of a command; never rig 1
  rig.py stop     kill THIS rig only (matched by its config path, at kill time)
  rig.py reboot   stop and start again with DH0: as the last run left it (a start clones it anew):
                  an install's writes survive. The last start's machine flags, or the ones given
  rig.py install  copy build/amiga/vtcon-handler to the VTC: drawer; the boot then puts
                  these builds over the copies a kit installed in SYS: (KIT_SHADOWS;
                  reboot the rig after install). The kit's own files are kept in
                  VTC:kit-orig and come back with `rig.py kit-restore` + a reboot.
  rig.py status   is it up (wasabid ping)
  rig.py start --mmu   the 68060 with an MMU (slower, no JIT), for MuForce
  rig.py muforce on|off|log   MuForce crash reports on the rig (tools/rig/muforce.py)
  rig.py start --debugger   FS-UAE's console debugger (console_debugger = 1) on a FIFO: stdout/stderr in
                  build/rig<N>/debugger.log, input from build/rig<N>/debugger.in
  rig.py dbg "<line>"   one debugger line to that FIFO, e.g. "w 0 <addr> 4 W" (memwatch on writes), "g"
  rig.py dbgbreak   SIGUSR1 to this rig's fs-uae: enter the console debugger (needs a debug build, UPTERM_FSUAE=<binary>)
  rig.py wasabi on|off|snoop|debug|guru|ps   call trace, debug output, Guru, tasks (tools/rig/wasabi.py)

Layout under build/rig/ (gitignored): sys.hdf (DH0:), boot/ (BOOTX:, boots
first: assigns, mounts XCON:, starts wasabid), vtc/ (VTC:, the binaries
under test), shots/.
"""
import fcntl, hashlib, json, os, pathlib, re, shutil, signal, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a1200_drift
import paths
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import libguard
import kitset
import user_startup
import wasabi as rigwasabi

ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG = paths.RIG      # build/rig, or build/rig2 with UPTERM_RIG=2
CFG = RIG / "vtcon-rig.fs-uae"
DBG_IN = RIG / "debugger.in"
DBG_LOG = RIG / "debugger.log"
# the last start's arguments (the machine: --a1200, --060, --kick ...). A reboot from another command
# (`rig.py aga`/`aga256`/`rtg`) boots with them, so the rig comes back as the same machine: the config is
# rewritten from the arguments on every start, and `rig.py aga` alone once turned an --a1200 rig into the
# default 060 + 1 GB machine, so a 68020 timing run measured the 060 (2026-10-19).
START_ARGS = RIG / "start.args"
# the owner's system disk (2026-10-03: the rig's copy was taken from it;
# the old source sat in a session scratchpad that is gone). Only read: the
# rig works on its own copy, build/rig/sys.hdf.
SRC_HDF = paths.SYSTEM_HDF        # UPTERM_SYSTEM_HDF
# wasabid: the pinned build in build/wasabi (`make wasabi`), copied to boot/wasabid on every start; the port per
# rig and the rig key come from paths.py (wasabi_port, WASABI_KEY)
# The default ROM's header says 40.63 (the A500/A600/A2000 3.1), whatever
# its file name says (measured 2026-09-30). --kick <file> boots another ROM
# (DP6 of the console.device plan); the config is rewritten on every start,
# so the next start without --kick is back on this one.
KICK = paths.KICKSTART
# the serial port: a file for traces (default), or --serial <path> (a host
# pty a test drives: tools/rig/getty_rig.py)
SERIAL = RIG / "serial.log"
if "--serial" in sys.argv:
    SERIAL = sys.argv[sys.argv.index("--serial") + 1]
# --os32: AmigaOS 3.2 (ledger T3; the owner's Replay runs 3.2): the 3.2.3
# Kickstart and the owner's installed 3.2 tree (a host drawer, its .uaem
# files carry the protection bits), copied once to build/rig/os32 -- the
# owner's tree is only read. Same boot drawer, VTC: and wasabid.
OS32 = "--os32" in sys.argv
OS32_SRC = paths.OS32_TREE        # UPTERM_OS32_TREE
OS32_KICK = paths.OS32_ROM        # UPTERM_OS32_ROM
if OS32:
    KICK = OS32_KICK
# --os <name>: boot a clean installed Workbench instead of sys.hdf or the 3.2
# tree (owner 2026-10-10: every rig boots a clean installed OS image). The
# owner's image (paths.OS_IMAGES, only read) is unpacked to the host directory <rig>/os-<name>/ (FS-UAE's hard drive),
# the rig's own copy; --fresh copies it anew (a clean system for this run).
# The system boots as a user's does: the rig's two hooks are lines appended
# to the copy's S:User-Startup (the originals are never written):
# VTC: assigned, BOOTX:go run. BOOTX: and VTCX: are data drives and do not
# boot (the image's priority is the higher). Names: 30 31 314 32 321 322 35 39.
OS_NAMES = {"30": "3.0", "31": "3.1", "314": "3.1.4", "32": "3.2", "321": "3.2.1", "322": "3.2.2",
            "35": "3.5", "39": "3.9"}
OS = sys.argv[sys.argv.index("--os") + 1] if "--os" in sys.argv else None
# --purpose "<who: what>": the session and task the rig serves, shown in the window title (title_line).
PURPOSE = " ".join(sys.argv[sys.argv.index("--purpose") + 1].split()) if "--purpose" in sys.argv[:-1] else None
if OS is not None and OS not in OS_NAMES:
    sys.exit("rig: --os %s: one of %s" % (OS, " ".join(OS_NAMES)))
FRESH = "--fresh" in sys.argv
# --keep-sys (internal, from `rig.py reboot`): this start boots the sys.hdf the last run left, no new clone
KEEP_SYS = "--keep-sys" in sys.argv
if OS in ("32", "321", "322"):
    KICK = OS32_KICK
elif OS == "314":
    KICK = paths.ROM314
if "--kick" in sys.argv:
    KICK = pathlib.Path(sys.argv[sys.argv.index("--kick") + 1]).expanduser()

STARTUP = """DH0:C/Assign >NIL: SYS: DH0:
DH0:C/Assign >NIL: C: DH0:C
DH0:C/Assign >NIL: S: DH0:S
If EXISTS DH0:L
  DH0:C/Assign >NIL: L: DH0:L
EndIf
DH0:C/Assign >NIL: LIBS: DH0:Libs
DH0:C/Assign >NIL: DEVS: DH0:Devs
DH0:C/Assign >NIL: FONTS: DH0:Fonts
DH0:C/Assign >NIL: VTC: VTCX:
C:Run >NIL: C:Execute BOOTX:go
C:Execute DH0:S/Startup-Sequence
"""
# LIBS: as the rig runs: the patched ixemul first, then the system's (with
# Classes and MUI), ncurses and AmiSSL's drawer. The boot script uses it, and
# install_rig.py puts it back after testing Install/Uninstall against the
# stock library. AmiSSL: S:User-Startup adds AmiSSL:Libs before go runs, and
# go's first line here assigns LIBS: anew, so without the last line
# amisslmaster.library was not found and every https request failed
# (amiga-pi: "Connection error.", 2026-10-13; tools/test_rig_libs.py). By its
# volume, not AmiSSL:, which a rig image without AmiSSL would ask for.
# The system disk's AmiSSL drawer holds amisslmaster.library but its AmiSSL/
# only as the kit's per-CPU subdrawers (68020-40/, 68060/), so the master found
# no LIBS:AmiSSL/amissl_v362.library: "Couldn't open AmiSSL!" and curl's rc 20
# (upkg_browse_rig step 0, 2026-10-21). VTC:amissl/AmiSSL holds the one library
# for the CPU this boot emulates, flat, as Install-AmiSSL lays it out
# (stage_amissl). The rig image has no AmiTCP and no usergroup.library: the
# kit's curl is the upterm-ports build on ixemul sockets (owner, 2026-10-27).
RIG_LIBS = ["Assign >NIL: LIBS: VTC:ixp6",
            "Assign >NIL: LIBS: DH0:Libs ADD",
            "Assign >NIL: LIBS: DH0:Classes ADD",
            "Assign >NIL: LIBS: DH0:MUI/Libs ADD",
            "Assign >NIL: LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD",
            "Assign >NIL: LIBS: DH0:AmiSSL/Libs ADD",
            "Assign >NIL: LIBS: VTC:amissl ADD"]

# The rig's lines in the system disk's S:Shell-Startup (go adds them once).
# The bare "Path >NIL: VTC:gg/bin ADD" an earlier go wrote raised "Please
# insert volume VTC" in every Shell that started before VTC: was assigned
# (go assigned it after its Wait 15), or on a boot without the rig; that
# requester kept Intuition from resetting the Workbench screen ("attempting
# to reset the Workbench screen") when Install CONSOLE DEVICE ran, and
# wasabid stopped answering until both were closed (2026-10-07).
# Assign EXISTS never asks for a volume.
SHELL_STARTUP_PATH = ["Assign >NIL: VTC: EXISTS", "If NOT WARN",
                      "  Path >NIL: VTC:gg/bin ADD", "EndIf"]
BARE_SHELL_PATH = "Path >NIL: VTC:gg/bin ADD"

# The kit's install copies the programs this repo builds into SYS: (L:, C:, DEVS:,
# UP-Term:bin), and what the system runs is those copies: XCON: is mounted from
# DEVS:DOSDrivers/XCON, Handler = L:vtcon-handler, and C: comes before the
# shell's path to VTC:. A `rig.py install` that writes only VTC: left a rig with
# a kit running the OLD handler (W-R1, 2026-10-13: three rig checks tested it).
# So the boot, before any of them is first used, puts the VTC: build over every
# copy the system already holds, and the C: commands the kit installs even where no kit put
# one (W-R4: plain `UPDemo` must run the build on every rig), after keeping the
# kit's own once in VTC:kit-orig. `kit-restore` makes the boot put those back.
USERLAND_SRC = ROOT / "build/dist/up-term/Files/userland/bin"
USERLAND_STAGE = "kit-userland"


def userland_names():
    if not USERLAND_SRC.is_dir():
        return []
    return sorted(f.name for f in USERLAND_SRC.iterdir()
                  if f.is_file() and re.fullmatch(r"[A-Za-z0-9._+-]+", f.name))


def kit_entries(nofpu=False, m060=False):
    """kitset.derive for this tree and CPU: every file the kit installs, from install.dos, the userland and the staged
    libraries (the CPU only picks the staged ixemul and AmiSSL builds; the boot's shadow rows do not depend on it)."""
    return kitset.derive(ROOT, USERLAND_SRC, USERLAND_STAGE, paths.VENDOR,
                         paths.IXEMUL_LIB_SOFT if nofpu else paths.IXEMUL_LIB, paths.IXNET_LIB, m060,
                         libguard.require_clean_ixnet)


# (VTC: name, SYS: place, backup name), one row per derived file the boot shadows (kitset): nothing hand-listed,
# so a file install.dos gains (C:Reboot, C:UPTerm, C:uptelnet, UP-Term:bin/hl mdv ps rz sz top sh) is shadowed at once.
# The keymap rows are spelled by path, never ENVARC:/ENV: (kitset.SPELL).
KIT_SHADOWS = [(e.vtc, e.dest, e.bak) for e in kit_entries() if e.shadow]

def kit_shadow_src(name):
    """The host file the VTC: copy of a KIT_SHADOWS name comes from: dist/ for the files the kit
    copies from dist/ (a1200_drift.DIST_SOURCES), build/amiga for the programs, the dist userland for the
    kit-userland/ rows (upkg among them: the dist userland is the ports build the kit packs)."""
    if name.startswith(USERLAND_STAGE + "/"):
        return USERLAND_SRC / name.split("/", 1)[1]
    return (ROOT / "dist" if name in a1200_drift.DIST_SOURCES else build_amiga()) / name


def q(path):
    """path as an AmigaDOS argument: quoted when it holds a space."""
    return '"%s"' % path if " " in path else path


def kit_guard(dest, body):
    """body only if dest exists. UP-Term: is asked with Assign EXISTS first: EXISTS on an
    assign that is not there (a rig with no kit) puts up a "insert volume" requester."""
    pre, post = [], []
    if dest.startswith("UP-Term:"):
        pre, post = ["C:Assign >NIL: UP-Term: EXISTS", "If NOT WARN"], ["EndIf"]
    return pre + ["If EXISTS %s" % q(dest)] + ["  " + l for l in body] + ["EndIf"] + post


def kit_shadow_lines(upterm):
    """The boot lines of KIT_SHADOWS: shadow, or restore when VTC:kit-restore exists. UP-Term:
    is the kit's assign from its User-Startup block, which has run only after go's Wait:
    upterm=True gives the UP-Term: places, False all the others."""
    shadows = [x for x in KIT_SHADOWS if x[1].startswith("UP-Term:") == upterm]
    out = ["If NOT EXISTS VTC:kit-orig", "  C:MakeDir >NIL: VTC:kit-orig", "EndIf",
           "If EXISTS VTC:kit-restore"]
    for _, dest, bak in shadows:
        out += kit_guard(dest, ["If EXISTS VTC:kit-orig/%s" % bak, "  C:Copy >NIL: VTC:kit-orig/%s %s CLONE" % (bak, q(dest)), "EndIf"])
    out.append("Else")
    for src, dest, bak in shadows:
        copy = ["If EXISTS VTC:%s" % src]
        if dest.startswith("C:"):
            # a C: command the kit installs: on a rig with no kit it is put there too (W-R4)
            copy += ["  If EXISTS %s" % q(dest), "    If NOT EXISTS VTC:kit-orig/%s" % bak,
                     "      C:Copy >NIL: %s VTC:kit-orig/%s CLONE" % (q(dest), bak), "    EndIf", "  EndIf"]
            copy += ["  C:Copy >NIL: VTC:%s %s CLONE" % (src, q(dest)), "EndIf"]
            out += copy
            continue
        out += kit_guard(dest, copy[:1] + ["  If NOT EXISTS VTC:kit-orig/%s" % bak,
                                "    C:Copy >NIL: %s VTC:kit-orig/%s CLONE" % (q(dest), bak),
                                "  EndIf",
                                "  C:Copy >NIL: VTC:%s %s CLONE" % (src, q(dest)), "EndIf"])
    out.append("EndIf")
    return "\n".join(out)


# the boot's own UP-Term: assign (a kit's wins); install_rig.py runs it again when it has put the owner's add-ons back
UPTERM_ASSIGN = """C:Assign >NIL: UP-Term: EXISTS
If WARN
  C:Assign >NIL: UP-Term: VTC:%s
EndIf""" % paths.UPTERM_DRAWER

# RIG-NO-THEMES: the kit ships UP-Term:Amarchy/Themes and FONTS:JetBrainsMono.ttf; a rig whose kit is older or
# absent has neither (empty theme list, bitmap menu font). `install` stages both in VTC: (stage_amarchy_assets),
# the boot puts them where the kit would, only where they are missing.
AMARCHY_ASSETS_BOOT = """C:Assign >NIL: UP-Term: EXISTS
If NOT WARN
  If NOT EXISTS UP-Term:Amarchy/Themes
    If EXISTS VTC:amarchy-themes
      C:MakeDir >NIL: UP-Term:Amarchy UP-Term:Amarchy/Themes
      C:Copy >NIL: VTC:amarchy-themes/#? UP-Term:Amarchy/Themes CLONE
    EndIf
  EndIf
EndIf
If NOT EXISTS FONTS:JetBrainsMono.ttf
  If EXISTS VTC:fonts/JetBrainsMono.ttf
    C:Copy >NIL: VTC:fonts/JetBrainsMono.ttf FONTS: CLONE
  EndIf
EndIf"""

GO = """FailAt 21
Echo >BOOTX:boot.log "go started"
%(shadows)s
C:Wait 15
; BOOTX:s/startup-sequence runs go beside DH0:S/Startup-Sequence, which makes
; ENV: (Assign ENV: RAM:ENV) only after SetPatch and its own Wait; go's SetEnv
; lines below would raise "Please insert volume ENV:" when it is late
; (RIG-ENV-RACE). Assign EXISTS never asks for a volume.
Lab envwait
C:Assign >NIL: ENV: EXISTS
If WARN
  C:Wait 1
  Skip envwait BACK
EndIf
%(shadows_up)s
C:Mount XCON: FROM BOOTX:Mountlist
C:Assign >NIL: ETC: VTC:etc
; /tmp: the kit puts "Assign TMP: T:" in S:User-Startup; a rig with no kit has it from here
C:Assign >NIL: TMP: T:
C:Assign >NIL: LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD
; UP-Term as the owner uses it on the rig: the patched ixemul first in
; LIBS: (Classes and MUI stay), PTY: and IXPIPE:, and GG: for /gg/bin/sh
; (vsh), as the kit sets it up. VTC: is assigned in BOOTX:s/startup-sequence,
; before the system's Startup-Sequence runs anything.
If EXISTS VTC:ixp6/ixemul.library
%(libs)s
EndIf
C:Assign >NIL: PTY: EXISTS DEVICES
If WARN
  C:Mount >NIL: PTY: FROM VTC:ptymount
EndIf
C:Assign >NIL: IXPIPE: EXISTS DEVICES
If WARN
  C:Mount >NIL: IXPIPE: FROM VTC:ixpipemount
EndIf
If EXISTS VTC:gg/bin/sh
  C:Assign >NIL: GG: VTC:gg
  ; the Unix commands (ls, dircolors, ...) on every Shell's command path,
  ; as the kit puts UP-Term:bin there. wasabid gives its commands a
  ; bare path (Current_directory, C:), so a Path here would not reach the
  ; windows: S:Shell-Startup runs in each new Shell. Added once, guarded
  ; (SHELL_STARTUP_PATH): the disk boots without the rig too.
  C:Search >NIL: S:Shell-Startup "VTC:gg/bin" QUIET
  If WARN
%(shellpath)s
  EndIf
EndIf
; UP-Term: for the rig with no kit installed (UT1): the glyph pages are read
; through UP-Term:unifont/ and UP-Term:emoji/ (render/unifont.h, emoji.h), and
; `install` puts them in VTC:Apps (a drawer, not the root of VTC:: dd_dir asks UP-Term: for a
; directory and a device root is not one). A kit's assign wins: its block in
; S:User-Startup either ran already (the assign exists) or runs later and
; replaces this one. ENVARC:up-term/Dir is no sign of a kit: an installer_rig
; run can leave it naming a drawer it deleted (rig 1, 2026-10-13).
%(upterm_assign)s
%(amarchy_assets)s
; no global TERM: a kit sets none either (vsh sets it in UP-Term windows, and a
; ROM CON: window with TERM=vtcon would be sent xterm sequences)
C:SetEnv TERMINFO /VTC/terminfo
C:SetEnv TERMCAP /etc/termcap
; the UP-Term icon beside Shell in SYS:System, as install.dos puts it there (its
; tool UP-Term:bin/vsh is in VTC:Apps, rig.py install); a kit's own icon stays.
; Only through an assign that exists: EXISTS on a missing UP-Term: puts up
; "Please insert volume UP-Term" and the boot waits there for ever.
C:Assign >NIL: UP-Term: EXISTS
If NOT WARN
  If EXISTS UP-Term:UP-Term.info
    If NOT EXISTS SYS:System/UP-Term.info
      C:Copy UP-Term:UP-Term UP-Term:UP-Term.info SYS:System CLONE
    EndIf
  EndIf
EndIf
; and the Prefs icon in SYS:Prefs, a plain program icon as install.dos puts it there
C:Assign >NIL: UP-Term: EXISTS
If NOT WARN
  If EXISTS UP-Term:UP-Term-Prefs.info
    If NOT EXISTS SYS:Prefs/UP-Term-Prefs.info
      If NOT EXISTS SYS:Prefs
        C:MakeDir >NIL: SYS:Prefs
      EndIf
      C:Copy UP-Term:UP-Term-Prefs UP-Term:UP-Term-Prefs.info SYS:Prefs CLONE
    EndIf
  EndIf
EndIf
Echo >>BOOTX:boot.log "assigns done"
Run >NIL: SYS:System/RexxMast
C:SetEnv wasabi.key %(key)s
C:Run >NIL: <NIL: BOOTX:wasabid %(port)d name rig%(rign)d
Echo >>BOOTX:boot.log "wasabid started"
""" % {"port": paths.WASABI_PORT, "key": paths.WASABI_KEY, "rign": paths.RIG_N, "upterm_assign": UPTERM_ASSIGN, "amarchy_assets": AMARCHY_ASSETS_BOOT, "shadows": kit_shadow_lines(False), "shadows_up": kit_shadow_lines(True), "libs": "\n".join("  C:" + l for l in RIG_LIBS),
       "shellpath": "\n".join('    Echo >>S:Shell-Startup "%s"' % l for l in SHELL_STARTUP_PATH)}
# boot.log in the host drawer BOOTX: tells from the Mac how far a boot got
# (the rig's screen is not visible from here).
MOUNTLIST = """XCON:
   Handler   = VTC:vtcon-handler
   Priority  = 5
   StackSize = 16000
   GlobVec   = -1
#
"""

RW = "--ro" not in sys.argv
# --exact: 68020 cycle-exact, no JIT. Emulated time then counts cycles,
# not host speed: the only way to benchmark on a loaded host (JIT timings
# swung 10x with the owner's other emulators, 2026-09-29).
FAST = "--fast" in sys.argv   # the CPU as fast as the host runs it (an accelerator's order of speed, not a model of one)
# --stock: creep's conbench machine, his config A1200-Stock-net (ledger S1,
# 2026-10-04): amiga_model A1200 at FS-UAE's default accuracy (the model's
# own CPU timing, not our overrides), 2 MB chip RAM, no fast RAM, no FPU, no
# graphics card, bsdsocket only. Our default rig has 8 MB fast + 64 MB Z3:
# CCON 1.2.7 ran 38.32 s on it in 77x20 and 86.46 s on his.
STOCK = "--stock" in sys.argv
# --060: the maxed-out machine next to the stock one (owner 2026-10-05: "max
# the amiga with 060 too a 020 is not good enough", ledger W41): a 68060 with
# its FPU, the CPU as fast as the host runs it, 128 MB Zorro III fast RAM.
# 68060-NOMMU: plain "68060" turns on MMU emulation, which is slow and rules
# out the JIT (owner 2026-10-05). FS-UAE executes the 060's unimplemented
# integer/FPU instructions itself
# (uae_cpu/fpu_no_unimplemented false), so no 68060.library is needed.
# --max: the --060 machine with as much Zorro III RAM as FS-UAE gives
# (owner 2026-10-07: "max it for the next run").
# The default machine is this one (owner 2026-10-13: every test rig runs as the 060 at full speed; a 68020 at
# real speed made an install take ages). --a1200 asks for the stock 68020 at real speed (timing runs);
# --stock, --exact and --fast are their own machines and keep the 020.
A1200 = any(f in sys.argv for f in ("--a1200", "--stock", "--exact", "--fast"))
# --a500: a 68000 at 7 MHz, cycle-exact, for the 68000 speed (vtcon plan W7, T1 pre-check 2): amiga_model A500
# (OCS, the model's 512 KB chip + 512 KB slow RAM) with 8 MB Zorro II fast RAM as an A590 or a sidecar gives it,
# no FPU, no graphics card. The default ROM is 40.63, the A500/A600/A2000 3.1, so it boots as is.
A500 = "--a500" in sys.argv
# --overscan: AGA rigs show the whole overscan picture (FS-UAE zoom = full) instead of the cropped 640x512 area, so a
# program that fills the Overscan prefs area (Selecta Fill Screen, I566) can be seen edge to edge. Other rigs keep the crop.
OVERSCAN = "--overscan" in sys.argv
MAX = "--max" in sys.argv or not (A1200 or A500)
M060 = "--060" in sys.argv or MAX
# --mmu: the 68060 with its MMU emulated (FS-UAE "68060", no JIT, slower): what MuForce needs
# (`rig.py muforce`). Only on the 060 machines; the next start without it is back on 68060-NOMMU.
# --nofpu: a machine with no FPU (the soft-float ixemul.library, (database "fpu") NOFPU). The 060 machines
# become the 68LC060 (FS-UAE's cpu key; the LC has no FPU and no MMU), the 020 machine drops its fpu key.
NOFPU = "--nofpu" in sys.argv
MMU = "--mmu" in sys.argv and M060 and not NOFPU
DEBUGGER = "--debugger" in sys.argv   # FS-UAE's console debugger on a FIFO (debugger_lines, dbg)
EXACT = "--exact" in sys.argv  # read-only DH0: makes "write protected" requesters that stall the rig


def os32_hd_layout():
    """SYS:L as an installed 3.2 has it. The owner's tree has the Workbench3.2
    floppy's layout (S:Startup-Sequence is Startup-Sequence_LD 47.4): no
    SYS:L, so its "IF NOT EXISTS SYS:L / Assign L: Extras3.2:L DEFER" made
    L: a deferred assign to a disk that is never in a drive, and anything
    that touched L: (Install's Copy to L:, Uninstall's Delete of
    L:vtcon-handler, a DOSDriver's Handler = L:...) put up "insert volume
    Extras3.2" and stalled the rig (2026-10-07). A hard-disk install has
    SYS:L: Install3.2's Install script assigns L: to <target>/L and copies
    the Extras3.2 disk's L drawer there. This does the same, once, from the
    3.2 CD's Extras3.2.adf (amitools' xdftool)."""
    sysl = RIG / "os32/L"
    if sysl.exists():
        return
    adf = RIG / "os32/media/AmigaOS3.2CD/ADF/Extras3.2.adf"
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    if not adf.exists() or not os.path.exists(xdf):
        sys.exit("rig: SYS:L is missing and cannot be made (needs %s and xdftool: pip install amitools)" % adf)
    tmp = RIG / "os32-extras.tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    subprocess.run([xdf, str(adf), "unpack", str(tmp)], check=True, capture_output=True)
    shutil.copytree(tmp / "L", sysl)
    shutil.rmtree(tmp)
    print("made SYS:L from Extras3.2.adf:", ", ".join(sorted(p.name for p in sysl.iterdir())))

OS_HOOKS = "Assign >NIL: VTC: VTCX:\nRun >NIL: C:Execute BOOTX:go\n"

def unpack_volume(image, dst):
    """The whole file tree of an ADF/HDF volume as a host directory FS-UAE
    mounts (its free space is the Mac's): xdftool unpack, then the protection
    bits, dates and comments of its .xdfmeta file written as FS-UAE's .uaem
    sidecar files (flags in hspa rwed order, date, comment). The image is only
    read. Returns the volume's name."""
    tmp = pathlib.Path(str(dst) + ".unpack")
    shutil.rmtree(tmp, ignore_errors=True)
    tmp.mkdir(parents=True)
    r = subprocess.run(["xdftool", str(image), "unpack", str(tmp)], capture_output=True, text=True)
    metas = list(tmp.glob("*.xdfmeta"))
    if r.returncode != 0 or len(metas) != 1:
        sys.exit("rig: xdftool could not unpack %s: %s" % (image, (r.stderr or r.stdout).strip()[-200:]))
    vol = metas[0].name[:-len(".xdfmeta")]
    for line in metas[0].read_text(encoding="utf-8", errors="replace").splitlines():
        path, _, rest = line.partition(":")
        if rest.startswith("DOS"):
            continue    # the volume's own line (file system type, dates)
        flags, date, comment = (rest.split(",", 2) + ["", ""])[:3]
        f = "".join(c if c in flags else "-" for c in "hsparwed")
        m = re.match(r"(\d\d)\.(\d\d)\.(\d{4}) (\S+)", date)
        when = "%s-%s-%s %s" % (m.group(3), m.group(2), m.group(1), m.group(4)) if m else "1978-01-01 00:00:00.00"
        target = tmp / vol / path
        if target.exists():
            pathlib.Path(str(target) + ".uaem").write_text("%s %s %s\n" % (f, when, comment))
    shutil.rmtree(dst, ignore_errors=True)
    shutil.move(str(tmp / vol), str(dst))
    shutil.rmtree(tmp)
    return vol

def os_image_copy():
    """<rig>/os-<name>/ (a host directory, FS-UAE's hard drive) from the owner's
    clean image (once, or anew with --fresh), the two hooks appended to its
    S:User-Startup; with --060/--max also the 68060.library. A directory and
    not an HDF copy: the clean images are 12 MB, a real system disk is far
    larger (owner 2026-10-11)."""
    dst = RIG / ("os-%s" % OS)
    label = RIG / ("os-%s.label" % OS)
    if not dst.exists() or not label.exists() or FRESH:
        src = paths.OS_IMAGES / ("Workbench_%s.hdf" % OS_NAMES[OS])
        paths.require_input(src, "UPTERM_OS_IMAGES", "the clean Workbench %s image" % OS_NAMES[OS])
        label.write_text(unpack_volume(src, dst))
        us = dst / "S/User-Startup"
        prior = us.read_text(errors="replace") if us.exists() else ""
        us.write_text(prior + ("" if prior.endswith("\n") or not prior else "\n") + OS_HOOKS)   # 3.2.2 ends its comment box without a newline: the first hook line was part of it (VTC: never as
        # the Workbench in 256 colours (rig.py aga256's file): the clean images have AGA only and a Workbench of few
        # colours, whose full colour map gives UP-Term's windows no pens for the xterm 256 colours (cube_rig,
        # screen_rig, tmux_rig: every colour the nearest Workbench pen; owner 2026-10-11)
        sm = RIG / "vtc/screenmode.aga256.prefs"
        paths.require_input(sm, "UPTERM_RIG", "the rig's 256-colour ScreenMode prefs (VTC:screenmode.aga256.prefs)")
        (dst / "Prefs/Env-Archive/Sys").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(sm, dst / "Prefs/Env-Archive/Sys/screenmode.prefs")
    if not (dst / "C/Installer").exists():
        # the clean 3.0/3.1 installs have no Installer (it is on the Install disk)
        paths.require_input(paths.INSTALLER, "UPTERM_INSTALLER", "an Installer program")
        shutil.copyfile(paths.INSTALLER, dst / "C/Installer")
    if M060:
        if not NOFPU:   # on the FPU-less 68LC060 SetPatch's Phase5 68060.library Gurus 80000006 before ENV: (rig 4, 2026-10-23)
            for lib, name, var in paths.libs060():
                paths.require_input(lib, var, name)
                shutil.copyfile(lib, dst / "Libs" / name)
        paths.require_input(paths.LIB68040, "UPTERM_LIB68040", "the 68040.library stub")
        shutil.copyfile(paths.LIB68040, dst / "Libs/68040.library")

def fresh_sys_hdf():
    """RIG/sys.hdf as a new APFS clone of the master image: instant, and no disk used until the rig writes into it.
    Every start does this, so what a killed, crashed or hung run left in DH0: (S:User-Startup blocks, ENVARC:, assigns,
    a planted file) never reaches the next boot (owner 2026-10-24, RIG-FRESH-SYS). A full 1.5 GB copy is never the
    fallback: the host disk has no room for one per rig."""
    paths.require_input(SRC_HDF, "UPTERM_SYSTEM_HDF", "the system disk image")
    dst = RIG / "sys.hdf"
    new = RIG / "sys.hdf.new"
    RIG.mkdir(parents=True, exist_ok=True)   # a rig number that never ran has no dir yet (RIG-CLONE-NEW-NUMBER)
    new.unlink(missing_ok=True)
    r = subprocess.run(["cp", "-c", str(SRC_HDF), str(new)], capture_output=True, text=True)
    if r.returncode != 0:
        new.unlink(missing_ok=True)
        sys.exit("rig: cannot clone the system disk %s to %s (cp -c: %s). The rig needs an APFS clone: a full 1.5 GB "
                 "copy would fill the host disk." % (SRC_HDF, dst, r.stderr.strip() or r.returncode))
    os.replace(new, dst)

def sys_hdf_cpu_lib(image):
    """The rig's own sys.hdf copy follows the machine: no Libs/68060.library on the FPU-less 68LC060 (SetPatch loads it
    and it Gurus 80000006 before ENV:, rig 4 2026-10-23), the paths.libs060() set on a 060 with FPU; other CPUs untouched.
    The set is written even where the master image already has a 68060.library: the master's is Phase5's 44.3, whose
    SetPatch run resets OS 3.1 in a loop (paths.LIB68060)."""
    if not M060:
        return
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    have = subprocess.run([xdf, str(image), "list", "Libs"], capture_output=True, text=True).stdout
    names = {l.split()[0] for l in have.splitlines() if l.split()}
    if NOFPU:
        if "68060.library" in names:
            subprocess.run([xdf, str(image), "delete", "Libs/68060.library"], check=True, capture_output=True)
        return
    for lib, name, var in paths.libs060():
        paths.require_input(lib, var, name)
        if name in names:
            subprocess.run([xdf, str(image), "delete", "Libs/" + name], check=True, capture_output=True)
        subprocess.run([xdf, str(image), "write", str(lib), "Libs/" + name], check=True, capture_output=True)

def sys_hdf_kit(image):
    """Every file the kit installs on the system disk equals what the current build would install (RIG-KIT-DERIVE;
    supersedes sys_hdf_ixnet, RIG-SYSHDF-EARLY). The master image holds older copies of files that load before
    BOOTX:go (ixnet.library 7a43f99c opened AmiTCP:libs: an 'insert AmiTCP:' requester on every rig; ixemul.library,
    up-console.device, C:vsh differ; ixnet refuses an ixemul of another revision), and install.dos files it lacks.
    kit_entries() is the set, derived (kitset.py); the clone gets each entry that has a place on the system disk, every
    file is read back and compared by md5, and the start is refused on any mismatch or when a source fails its check
    (ixnet: tools/libguard.py). The master image is never touched. The UP-Term: places live on the partition the
    kit's User-Startup assigns (sys_hdf_kit_upterm writes them there too); the boot still shadows them and the spelled
    keymap places from VTC: (KIT_SHADOWS, refresh_kit_shadows)."""
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    def run(*a, check=True):
        return subprocess.run([xdf, str(image), *a], check=check, capture_output=True, text=True)
    for e in kit_entries(NOFPU, M060):
        if not e.clone:
            continue
        paths.require_input(e.src, "UPTERM_ROOT", e.src.name)
        if e.check:
            e.check(e.src)
        parts = e.clone.split("/")
        for i in range(1, len(parts)):
            d = "/".join(parts[:i])
            if run("list", d, check=False).returncode != 0:
                run("makedir", d)
        parent, leaf = e.clone.rsplit("/", 1)
        if leaf in {l.split()[0] for l in run("list", parent).stdout.splitlines() if l.split()}:
            run("delete", e.clone)
        run("write", str(e.src), e.clone)
        back = RIG / "kit-check.tmp"
        back.unlink(missing_ok=True)
        run("read", e.clone, str(back))
        try:
            if e.check:
                e.check(back)
            if hashlib.md5(back.read_bytes()).hexdigest() != hashlib.md5(e.src.read_bytes()).hexdigest():
                sys.exit("rig: the clone's %s differs from %s after the write (RIG-KIT-DERIVE)" % (e.clone, e.src))
        finally:
            back.unlink(missing_ok=True)
    sys_hdf_kit_upterm(image)

def clone_upterm_place(image):
    """(partition, directory) of UP-Term: in the clone, or None. The kit's ';BEGIN UP-Term assign' block in
    S:User-Startup names a volume path ("Rushhours:Tools/UP-Term"); the volume is the RDB partition with that disk
    name (DH1 on the master image). A clone with no such block or no such partition has no UP-Term: of its own."""
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    us = RIG / "user-startup.tmp"
    us.unlink(missing_ok=True)
    if subprocess.run([xdf, str(image), "read", "S/User-Startup", str(us)], capture_output=True).returncode != 0:
        return None
    text = us.read_text(errors="replace")
    us.unlink()
    m = re.search(r'^\s*Assign\s+UP-Term:\s+"?([^:"\s]+):([^"\s]*)"?', text, re.M | re.I)
    if not m:
        return None
    vol, d = m.group(1).lower(), m.group(2).strip("/")
    rdb = shutil.which("rdbtool") or str(pathlib.Path.home() / ".local/bin/rdbtool")
    info = subprocess.run([rdb, str(image), "info"], capture_output=True, text=True).stdout
    for part in re.findall(r"^Partition: #\d+ '([^']+)'", info, re.M):
        show = subprocess.run([xdf, str(image), "open", "part=" + part, "+", "root", "show"], capture_output=True, text=True).stdout
        n = re.search(r"disk name:\s*(.+)", show)
        if n and n.group(1).strip().lower() == vol:
            return part, d
    return None

def sys_hdf_kit_upterm(image):
    """sys_hdf_kit for the UP-Term: places (RIG-WARM-SRV-RACE). The master image's UP-Term:bin holds older Amarchy,
    amarchy-menu, amarchy-gui and amarchy-index (all four md5s differed from the build, 2026-10-28). On a fresh clone
    the kit's User-Startup line `Run >NIL: UP-Term:bin/Amarchy` started those while BOOTX:go's shadow Copy wrote the
    new ones over the same files, and the warm servers AMARCHY.MENUSRV/GUISRV never came up (3 of 3 fresh starts;
    every `rig.py reboot`, whose clone already held the copies, had them). The clone now gets every UP-Term: file of
    the kit before it boots, read back and compared by md5 like the system disk's; the boot's shadow then writes the
    same bytes."""
    place = clone_upterm_place(image)
    if not place:
        return
    part, base = place
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    def run(*a, check=True):
        return subprocess.run([xdf, str(image), "open", "part=" + part, "+", *a], check=check, capture_output=True, text=True)
    def amiga(dest):
        return "/".join(x for x in (base, dest[len("UP-Term:"):]) if x)
    entries = [e for e in kit_entries(NOFPU, M060) if e.dest.startswith("UP-Term:")]
    if not entries:
        return
    listed = {}
    ops = []
    for e in entries:
        parent, leaf = amiga(e.dest).rsplit("/", 1)
        if parent not in listed:
            parts = parent.split("/")
            for i in range(1, len(parts) + 1):
                d = "/".join(parts[:i])
                if d not in listed:
                    r = run("list", d, check=False)
                    if r.returncode != 0:
                        run("makedir", d)
                    listed[d] = {l.split()[0] for l in r.stdout.splitlines() if l.split()} if r.returncode == 0 else set()
        if leaf in listed[parent]:
            ops += ["+", "delete", amiga(e.dest)]
        ops += ["+", "write", str(e.src), amiga(e.dest)]
    run(*ops[1:])
    back = RIG / "kit-upterm.tmp"
    shutil.rmtree(back, ignore_errors=True)
    back.mkdir()
    try:
        reads = []
        for i, e in enumerate(entries):
            reads += ["+", "read", amiga(e.dest), str(back / str(i))]
        run(*reads[1:])
        for i, e in enumerate(entries):
            if hashlib.md5((back / str(i)).read_bytes()).hexdigest() != hashlib.md5(e.src.read_bytes()).hexdigest():
                sys.exit("rig: the clone's %s differs from %s after the write (RIG-WARM-SRV-RACE)" % (e.dest, e.src))
    finally:
        shutil.rmtree(back, ignore_errors=True)

def strip_user_startup_blocks(image, name):
    """Remove the ';BEGIN <name>' blocks from the image's S:User-Startup (none: nothing happens)."""
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    us = RIG / "user-startup.tmp"
    us.unlink(missing_ok=True)
    if subprocess.run([xdf, str(image), "read", "S/User-Startup", str(us)], capture_output=True).returncode != 0:
        return   # no S:User-Startup: no block
    data = us.read_bytes()
    clean = user_startup.strip_upterm_blocks(data, name)
    if clean != data:
        us.write_bytes(clean)
        subprocess.run([xdf, str(image), "delete", "S/User-Startup"], check=True, capture_output=True)
        subprocess.run([xdf, str(image), "write", str(us), "S/User-Startup"], check=True, capture_output=True)
    us.unlink()

def sys_hdf_no_wasabid(image):
    """The master image's S:User-Startup carries the kit's ';BEGIN UP-Term wasabi' block (Run >NIL: C:wasabid). That
    wasabid starts before BOOTX:go sets wasabi.key, so it serves an empty key on its default port 1234, and FS-UAE's
    bsdsocket shares the Mac's ports: rig 1's BOOTX:wasabid (port 1234) lost the port to it and every request got
    'auth failed: bad key' (2026-10-25). The block goes from the clone; the master is never touched."""
    strip_user_startup_blocks(image, "UP-Term wasabi")

def sys_hdf_no_dopus(image):
    """--no-dopus: a rig without Directory Opus (the not-running notice, plan D5). The master image replaces the
    Workbench with Directory Opus 5 in two files: Libs/workbench.library (45.130) and C/LoadWB, which starts DOpus
    when S:Startup-Sequence runs it; the original LoadWB is kept as C/loadwb_old. In the clone C/LoadWB becomes
    loadwb_old again and Libs/workbench.library goes (the Kickstart 3.1 ROM carries the stock one), so there is no
    DOPUS.1 port. The master image is never touched."""
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    old = RIG / "loadwb_old.tmp"
    old.unlink(missing_ok=True)
    subprocess.run([xdf, str(image), "read", "C/loadwb_old", str(old)], check=True, capture_output=True)
    subprocess.run([xdf, str(image), "delete", "C/LoadWB"], check=True, capture_output=True)
    subprocess.run([xdf, str(image), "write", str(old), "C/LoadWB"], check=True, capture_output=True)
    old.unlink()
    subprocess.run([xdf, str(image), "delete", "Libs/workbench.library"], check=True, capture_output=True)
    # The master also has Directory Opus 5.82 at DH0:Opus5 with the DOpus5: assign and PATH lines in S:User-Startup
    # (block ';BEGIN DirectoryOpus 5'); with them the add-on's install.dos takes its "already installed" branch
    # (plan D9, owner 2026-10-26). The block goes and so do Opus5 and its icon
    # (xdftool has no rename; the clone is disposable, so removing equals hiding), on the clone only.
    strip_user_startup_blocks(image, "DirectoryOpus 5")
    subprocess.run([xdf, str(image), "delete", "Opus5", "all"], check=True, capture_output=True)
    subprocess.run([xdf, str(image), "delete", "Opus5.info"], check=True, capture_output=True)

SCREENMODE_NAME = RIG / "screenmode.name"   # the last `rig.py aga|aga256|rtg`: a start re-applies it to the fresh sys.hdf

def sys_hdf_screenmode(image):
    """`rig.py aga|aga256|rtg` writes ENVARC:Sys/screenmode.prefs and reboots; the reboot clones sys.hdf anew, so the
    chosen mode is put into the fresh image from the host here (it persists as RIG/screenmode.name, not in the disk)."""
    if not SCREENMODE_NAME.exists():
        return
    src = RIG / ("vtc/screenmode.%s.prefs" % {"aga": "aga16"}.get(SCREENMODE_NAME.read_text().strip(), SCREENMODE_NAME.read_text().strip()))
    paths.require_input(src, "UPTERM_RIG", "the ScreenMode prefs of `rig.py %s`" % SCREENMODE_NAME.read_text().strip())
    xdf = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
    dst = "Prefs/Env-Archive/Sys/screenmode.prefs"
    subprocess.run([xdf, str(image), "delete", dst], capture_output=True)   # absent in the master: nothing to delete
    subprocess.run([xdf, str(image), "write", str(src), dst], check=True, capture_output=True)

def cpu_lines():
    """The A1200 machine's cpu and fpu keys: 68060 + 68060 FPU (default), 68020 + 68882 (--a1200); --nofpu: the
    68LC060 and no fpu key, or the 68020 and no fpu key (FS-UAE: cpu accepts 68LC060, fpu 68881/68882/68040/68060)."""
    if M060:
        cpu = "68LC060" if NOFPU else "68060" if MMU else "68060-NOMMU"
    else:
        cpu = "68020"
    return ["cpu = %s" % cpu] + ([] if NOFPU else ["fpu = %s" % ("68060" if M060 else "68882")])

def unimplemented_lines():
    """The 060 runs the instructions it lacks in the emulator (no 68060.library needed), the FPU's too when there is one."""
    return (["uae_cpu_no_unimplemented = false"] + ([] if NOFPU else ["uae_fpu_no_unimplemented = false"])) if M060 else []

# The Amiga picture each screen mode puts in the window, in Amiga pixels: the master sys.hdf's
# ScreenMode prefs is uaegfx mode 0x5004xxxx (800x600, `rig.py rtg`); `rig.py aga|aga256` and the
# --os images are PAL hires (640x256, line-doubled to 640x512 and cropped there by FS-UAE's zoom).
SCREEN_SIZES = {"rtg": (800, 600), "aga": (640, 512), "aga256": (640, 512)}
WINDOW_BUDGET = (1280, 1024)   # the window size the rigs opened at before integer scaling

def window_lines():
    """The window fits the picture: with scale = integer a fixed 1280x1024 window showed the 800x600 RTG
    Workbench at x1 inside wide black bands (owner 2026-10-26: "they open windows much bigger than the
    content"). The window is the picture times the largest whole factor that stays inside WINDOW_BUDGET."""
    name = SCREENMODE_NAME.read_text().strip() if SCREENMODE_NAME.exists() else "aga256" if OS else "rtg"
    w, h = SCREEN_SIZES[name]
    k = max(1, min(WINDOW_BUDGET[0] // w, WINDOW_BUDGET[1] // h))
    zoom = ["zoom = full"] if OVERSCAN else [] if name == "rtg" else ["zoom = 640x512"]   # --overscan: every mode, rtg included
    return ["window_width = %d" % (w * k), "window_height = %d" % (h * k)] + zoom   # AGA: crop the overscan border, or x2 no longer fits

def title_line():
    """The window title names the rig and its machine: with several rigs open the
    owner saw only identical FS-UAE windows (2026-10-24) and could not tell them apart. It also names the
    session and task from --purpose: with rigs of several sessions open the owner could not tell what each
    one did or whose it was (2026-10-25). A start without --purpose says so in the title."""
    machine = ("A1200 stock" if STOCK else "A500" if A500 else
               "060 max" if MAX else "060" if M060 else "68020")
    return "title = vtcon rig %d (%s%s) - %s" % (paths.RIG_N, machine, ", OS %s" % OS if OS else ", OS 3.2" if OS32 else "",
                                                  PURPOSE or "purpose not given (rig.py start --purpose)")

# The Mac keys the rig's fs-uae maps (setup writes them into the config).
HOST_KEYS = [
    # both Cmd keys are Right-Amiga, so Cmd+C/Cmd+V copy and paste from either
    # side as on the Mac. FS-UAE's own modifier was the left Cmd (its log: "Using
    # default modifier key LSUPER") and took those keys; it moves to the right
    # Ctrl. The F12 menu is unchanged.
    "modifier_key = rctrl",
    "keyboard_key_lsuper = action_key_amiga_right",
    "keyboard_key_rsuper = action_key_amiga_right",
    # so no Cmd key is Left-Amiga, which Amarchy's bindings need (Super is
    # "rawkey lcommand"), and macOS keeps Cmd+Space for Spotlight anyway: the right
    # Option key is Left-Amiga (Right Option+Space opens the menu). The left Option
    # key stays the Amiga's Left Alt.
    "keyboard_key_ralt = action_key_amiga_left",
]


# The ScreenMode prefs files VTC: holds (IFF PREF/PRHD/SCRM, 62 bytes): rig.py aga|aga256|rtg copy them to
# ENVARC:Sys/screenmode.prefs and os_image_copy puts aga256 into a clean image. They had no source in the repo;
# a rig other than rig 1 got them only by a hand copy. (name, DisplayID, width, height, depth) as rig 1 holds them.
SCREENMODE_PREFS = [("aga16", 0x00029000, 0x280, 0x100, 4), ("aga256", 0x00029000, 0x280, 0x100, 8),
                    ("rtg", 0x50041303, 0xffff, 0xffff, 24), ("rtg360", 0x50041303, 0x280, 0x168, 24)]


def screenmode_prefs_bytes(display_id, width, height, depth):
    import struct
    scrm = struct.pack(">16xIHHHH", display_id, width, height, depth, 1)
    return (b"FORM" + struct.pack(">I", 4 + 8 + 6 + 8 + len(scrm)) + b"PREF" + b"PRHD" + struct.pack(">I", 6)
            + bytes(6) + b"SCRM" + struct.pack(">I", len(scrm)) + scrm)


def stage_rig_vtc(vtc):
    """What a rig's VTC: drawer needs to start, for build/rig and build/rigN alike (setup calls it): the ScreenMode
    prefs, and the UP-Term drawer the boot's UP-Term: assign points at, with bin/vsh, the icons and the Prefs launcher
    (stage_upterm, as `rig.py install` makes it). Files already there are kept."""
    vtc.mkdir(parents=True, exist_ok=True)
    for name, did, w, h, depth in SCREENMODE_PREFS:
        f = vtc / ("screenmode.%s.prefs" % name)
        if not f.exists():
            f.write_bytes(screenmode_prefs_bytes(did, w, h, depth))
    vsh = ROOT / "build/amiga/vsh"
    if not (vtc / paths.UPTERM_DRAWER / "bin/vsh").is_file():
        paths.require_input(vsh, "UPTERM_ROOT", "build/amiga/vsh (run make for the Amiga programs)")
        stage_upterm(vtc / paths.UPTERM_DRAWER, vsh)


def setup():
    (RIG / "boot").mkdir(parents=True, exist_ok=True)
    if paths.RIG_N > 1 and not (RIG / "vtc").exists():
        # rig 2's VTC: starts as a copy of rig 1's (only read there; .uaem
        # files carry the protection bits); `install` and the scripts keep it
        print("copying rig 1's VTC: drawer ...")
        shutil.copytree(paths.RIG_HOME / "rig/vtc", RIG / "vtc", symlinks=True)
    stage_rig_vtc(RIG / "vtc")
    (RIG / "shots").mkdir(exist_ok=True)
    if OS32 and not (RIG / "os32").exists():
        print("copying the 3.2 system (168 MB) ...")
        paths.require_input(OS32_SRC, "UPTERM_OS32_TREE", "the AmigaOS 3.2 tree")
        shutil.copytree(OS32_SRC, RIG / "os32", symlinks=True)
    if OS32:
        os32_hd_layout()
    if OS:
        # the clean system partition is 12 MB: the kit goes to a second, roomy
        # drive, Work:, as on an Amiga with System and Work partitions
        (RIG / "work").mkdir(exist_ok=True)
        os_image_copy()
    elif not OS32 and not (RIG / "sys.hdf").exists():
        fresh_sys_hdf()
    (RIG / "boot/s").mkdir(exist_ok=True)
    # --060: at host speed the boot shell's window is open (something prints
    # into it) when IPrefs switches the Workbench to the saved RTG mode, and
    # Intuition cannot reset the screen ("Please close all windows"): the
    # Workbench stays PAL on some boots (2026-10-05, twice). The system's
    # Startup-Sequence runs with its output to NIL: there.
    (RIG / "boot/s/startup-sequence").write_text(
        STARTUP.replace("C:Execute DH0:S/Startup-Sequence", "C:Execute DH0:S/Startup-Sequence >NIL:")
        if M060 else STARTUP)
    # --stock: creep's Workbench prints topaz 8, 77 columns in a 640-wide
    # window. The rig boots from BOOTX:, which had no Devs/system-configuration,
    # so Intuition took its built-in 60-column topaz (61 columns measured,
    # 2026-10-04). The 3.2 install's own file (FontHeight 8) goes in for --stock
    # only: the default rig's serial and other settings stay as they were.
    sysconf = RIG / "boot/devs/system-configuration"
    if STOCK:
        sysconf.parent.mkdir(exist_ok=True)
        shutil.copyfile(RIG / "os32/Devs/system-configuration", sysconf)
    elif sysconf.exists():
        sysconf.unlink()
    # the pinned wasabid (`make wasabi`), every start; one daemon, no other agent in the boot
    shutil.copyfile(rigwasabi.build_dir() / "wasabid", RIG / "boot/wasabid")
    stage_amissl(RIG / "vtc", M060)
    stage_ixemul(RIG / "vtc", NOFPU)
    refresh_programs(RIG / "vtc", ROOT / "build/amiga")   # the files build_drift checks, as install copies them
    refresh_kit_shadows(RIG / "vtc")
    stale = build_drift(RIG / "vtc")
    if stale:
        print("[WARN] %d file(s) in VTC: differ from the build: %s; `rig.py install` puts the build there"
              % (len(stale), ", ".join(stale)))
    (RIG / "boot/go").write_text(GO)
    (RIG / "boot/Mountlist").write_text(MOUNTLIST)
    machine = [
        "[fs-uae]", "amiga_model = A1200", "accuracy = 1", "chip_memory = 2048",
        "bsdsocket_library = 1",
    ] if STOCK else [
        "[fs-uae]", "amiga_model = A500", "accuracy = 1", "fast_memory = 8192",
        "bsdsocket_library = 1", "jit_compiler = 0",
        "uae_cpu_speed = real", "uae_cpu_cycle_exact = true", "uae_cpu_compatible = true",
    ] if A500 else [
        "[fs-uae]", "amiga_model = A1200",
    ] + cpu_lines() + [
        "fast_memory = 8192",
        # 64 MB more, as an accelerator's: GNU screen with four panes (tcsh in
        # each) left 663 KB of the 8 MB (owner 2026-09-30: "you can add more ram")
        "zorro_iii_memory = %d" % (1048576 if MAX else 131072 if M060 else 65536),
        "bsdsocket_library = 1", "graphics_card = uaegfx",
        # the RTG card's pointer as a sprite: Picasso96's software pointer is
        # hidden and drawn again around every blit, so it flickered with the
        # blinking cursor (owner 2026-10-03: "the mouse pointer still blinks")
        "uae_gfxcard_hardware_sprite = true",
        "jit_compiler = %d" % (0 if EXACT or MMU else 1),   # the JIT and the emulated MMU exclude each other
        # without this FS-UAE runs the A1200's 68020 at about its real speed
        # (no JIT on an ARM host): UPDemo BENCH and cellbench measured a
        # stock machine, 2026-10-04
        "uae_cpu_speed = %s" % ("max" if FAST or M060 else "real"),
        "uae_cpu_cycle_exact = %s" % ("true" if EXACT else "false"),
        "uae_cpu_compatible = %s" % ("true" if EXACT else "false"),
    ] + unimplemented_lines()
    paths.require_rom(KICK)
    paths.FSUAE_LOG_DIR.mkdir(parents=True, exist_ok=True)
    CFG.write_text("\n".join(machine + [
        # FS-UAE's log in this rig's dir, capped by paths.cap_fsuae_log; not the shared ~/Documents/FS-UAE log
        # that grew to 7.9 GB (2026-10-17). No log_* option is set: the rig needs none of them.
        "logs_dir = %s" % paths.FSUAE_LOG_DIR,
        # the flood source (2026-10-17): with the default flag value 1 every set-protection / filenote /
        # set-date on a host drawer logs "need_metadata_file", "writing mode ----rwed" and "writing comment" (3 lines
        # each, unconditional write_log in fsdb_host.cpp) and writes a .uaem for DEFAULT bits. nhsparwed keeps
        # every note, time and non-default bit in .uaem (all anything reads) and drops only the default-bit writes.
        "uaem_write_flags = nhsparwed",
        "hard_drive_0 = %s" % (RIG / ("os-%s" % OS if OS else "os32" if OS32 else "sys.hdf")),
        ] + (["hard_drive_0_label = %s" % (RIG / ("os-%s.label" % OS)).read_text()] if OS else []) + [
        # Writable: read-only (--ro) put up "Volume System is write
        # protected" requesters that stalled the rig (the owner saw them,
        # 2026-09-29). stop write-protects DH0: through wasabid before its
        # SIGKILL, so no write is left in flight to validate on next boot.
        "hard_drive_0_read_only = %d" % (0 if RW else 1),
    ] + (["hard_drive_0_priority = 10"] if OS else []) + [
        "hard_drive_1 = %s" % (RIG / "vtc"), "hard_drive_1_label = VTCX",
        "hard_drive_2 = %s" % (RIG / "boot"), "hard_drive_2_label = BOOTX",
        "hard_drive_2_priority = %d" % (0 if OS else 10), "joystick_port_1 = none",
        "kickstart_file = %s" % KICK,
    ] + (["hard_drive_3 = %s" % (RIG / "work"), "hard_drive_3_label = Work", "hard_drive_3_priority = -128"] if OS else []) + [
        # audible output on every rig: "interrupts" is emulated with no output and
        # volume 0 muted it, so no rig made sound (owner, 2026-10-26)
        "uae_sound_output = exact", "volume = 100", "initial_input_grab = 0",
        # floppy drive clicks and head steps off; Paula audio stays on (owner, 2026-10-27)
        "floppy_drive_volume = 0", "floppy_drive_volume_empty = 0",
        # the Amiga's clipboard.device unit 0 and the Mac's clipboard are one: a copy
        # in UP-Term pastes on the Mac and back (off without it, 2026-10-12)
        "clipboard_sharing = 1",
    ] + HOST_KEYS + [title_line(),
    ] + window_lines() + [
        # sharp pixels: nearest-neighbour texture filter instead of the default linear blur
        # (the owner found the rigs blurry, 2026-10-26)
        "texture_filter = nearest",
        # whole-number zoom only (the Amiga mode shown at x1, x2, never 1.6x), FS-UAE scale = integer
        # (owner, 2026-10-26)
        "scale = integer",
        # the serial port into a file: kprintf traces (ixemul's DEBUG_VERSION)
        "serial_port = %s" % SERIAL,
        "screenshots_output_dir = %s" % (RIG / "shots"),
        ] + debugger_lines() + [""]))
    if "start" not in sys.argv:
        print("rig ready:", CFG)

def debugger_lines():
    """--debugger: the UAE console debugger reads the FIFO DBG_IN and writes DBG_LOG (see launch_fsuae)."""
    # use_debugger: the debugger prompt comes up at boot, before the Amiga runs; set breaks there and `g`. The
    # F12+D hot key cannot be sent to the SDL window as a synthetic key (tried, no effect).
    return ["console_debugger = 1", "uae_use_debugger = 1"] if DEBUGGER else []

def dbg():
    """rig.py dbg "<line>": one line into the running debugger's FIFO."""
    if not DBG_IN.exists():
        sys.exit("rig: no debugger FIFO %s (start with --debugger)" % DBG_IN)
    fd = os.open(str(DBG_IN), os.O_RDWR | os.O_NONBLOCK)
    try:
        os.write(fd, (" ".join(sys.argv[2:]) + "\n").encode())
    finally:
        os.close(fd)

def dbgbreak():
    """rig.py dbgbreak: SIGUSR1 into this rig's FS-UAE. A build with debuggable() = 1 and a SIGUSR1 handler that
    calls activate_debugger() (Emulators/fs-uae-debug, UPTERM_FSUAE) drops into the console debugger on the FIFO."""
    pids = [int(l.split()[0]) for l in mine()]
    if not pids:
        sys.exit("rig: no FS-UAE running for %s" % CFG)
    for pid in pids:
        os.kill(pid, signal.SIGUSR1)
    print("rig: SIGUSR1 sent to %s" % pids)

def fsuae_stdio():
    """stdin, stdout, stderr for the FS-UAE process: the debugger FIFO and log with --debugger, else null."""
    if not DEBUGGER:
        return subprocess.DEVNULL, subprocess.DEVNULL, subprocess.DEVNULL
    if not DBG_IN.exists():
        os.mkfifo(str(DBG_IN))
    # read-write so the FIFO never reaches end of file between two dbg lines
    return os.open(str(DBG_IN), os.O_RDWR), open(DBG_LOG, "wb"), subprocess.STDOUT

def setup_config_only():
    """Rewrite just the config, so --rw on or off takes effect."""
    # a --os / --os32 rig has no sys.hdf: its first start (a fresh build/rigN) stages the drawer here too
    if (RIG / "sys.hdf").exists() or OS or OS32:
        setup()

def stage_upterm(dest, vsh):
    """The kit's UP-Term: drawer as far as the icon needs it (R9): bin/vsh, the tool
    of UP-Term.info, and the icon and its text file, made as `make dist-base` makes them. Amarchy and
    amarchy-menu go to bin beside vsh when they were built, at paths.UPTERM_BIN (the rig drawer; the
    rig scripts start them from there, as UP-Term: moves with a kit install); before this the rig had no Amarchy."""
    (dest / "bin").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(vsh, dest / "bin/vsh")
    for name in ("Amarchy", "amarchy-menu", "amarchy-gui", "amarchy-index", "AmTile"):
        if (vsh.parent / name).is_file():
            shutil.copyfile(vsh.parent / name, dest / "bin" / name)
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/mkicon.py"), str(dest / "UP-Term.info")], check=True)
    (dest / "UP-Term").write_text("UP-Term: double-click the icon to open a terminal with vsh.\n")
    # the Prefs editor's icon (W-R3), as make dist-base makes it; GO copies it to SYS:Prefs
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/mkicon.py"), str(dest / "UP-Term-Prefs.info"),
                    "--tool", "C:UP-Term-Prefs", "--plain"], check=True)
    (dest / "UP-Term-Prefs").write_text("UP-Term Prefs: edit the profiles in ENVARC:up-term/up-term.\n")
    # the drawer's own icon, beside it (VTC:Apps.info): without it the drawer cannot be opened from the
    # Workbench, and Directory Opus showed only the stale Apps and UP-Term icons (rig 1, 2026-10-14)
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/mkicon.py"),
                    str(dest.with_name(dest.name + ".info")), "--drawer"], check=True)

def stage_amarchy_assets(vtc):
    """RIG-NO-THEMES: dist/amarchy-themes to VTC:amarchy-themes and the kit's JetBrainsMono.ttf (the
    Makefile's JBMONO_TTF, installed as Files/fonts/JetBrainsMono.ttf) to VTC:fonts; the boot
    (AMARCHY_ASSETS_BOOT) puts them at UP-Term:Amarchy/Themes and FONTS:."""
    themes = sorted((ROOT / "dist/amarchy-themes").glob("*.theme"))
    if themes:
        (vtc / "amarchy-themes").mkdir(parents=True, exist_ok=True)
        for f in themes:
            shutil.copyfile(f, vtc / "amarchy-themes" / f.name)
    else:
        print("no themes in %s: the rig's theme list stays empty" % (ROOT / "dist/amarchy-themes"))
    ttf = ROOT / "build/jbmono/JetBrainsMono-Regular.ttf"
    if ttf.is_file():
        (vtc / "fonts").mkdir(exist_ok=True)
        shutil.copyfile(ttf, vtc / "fonts/JetBrainsMono.ttf")
    else:
        print("no %s: run make jbmono, or the menu falls back to a bitmap font" % ttf)

def clean_stale_icons(vtc):
    """Icons at the root of the host drawer that belong to nothing the rig or a kit has: VTC:UP-Term and
    VTC:UP-Term.info (a launcher project copied there by hand on 2026-09-29 while P8 was written; no committed
    rig code makes them, and the kit's launcher is VTC:Apps/UP-Term), and a drawer icon whose drawer is gone
    (VTC:Apps.info after installer_rig deleted Apps). The .uaem files are FS-UAE's metadata of them."""
    gone = []
    old = vtc / "UP-Term"
    if old.is_file():   # the drawer of that name (and its drawer icon) is the rig's own
        gone.append("UP-Term")
    info = vtc / "UP-Term.info"
    if info.is_file() and info.read_bytes()[48:49] == b"\x04":   # a project icon, not the drawer icon
        gone.append("UP-Term.info")
    for icon in sorted(vtc.glob("*.info")):
        data = icon.read_bytes()
        # do_Type 2 = WBDRAWER (a volume's Disk.info and project icons have no drawer to miss)
        if len(data) > 48 and data[48] == 2 and not (vtc / icon.name[:-5]).is_dir():
            gone.append(icon.name)
    for name in gone:
        for f in (vtc / name, vtc / (name + ".uaem")):
            if f.is_file():
                f.unlink()
                print("removed stale", f.name)

def migrate_upterm_drawer(vtc):
    """VTC:upterm -> VTC:Apps, with its icon made again by stage_upterm. Through a temporary name:
    the Mac's filesystem is case-insensitive, and a half-done rename must not lose the drawer."""
    old, new = vtc / paths.OLD_UPTERM_DRAWER, vtc / paths.UPTERM_DRAWER
    tmp = vtc / "Apps.migrating"
    if old.is_dir() and not new.is_dir():
        old.rename(tmp)
        tmp.rename(new)
        print("renamed", old.name, "to", new.name, "in", vtc)
    elif old.is_dir():   # an Apps drawer already there (installer_rig makes one): move in what it lacks
        for f in sorted(old.iterdir()):
            if not (new / f.name).exists():
                f.rename(new / f.name)
        shutil.rmtree(old, ignore_errors=True)
    for name in (paths.OLD_UPTERM_DRAWER + ".info", paths.OLD_UPTERM_DRAWER + ".uaem", paths.OLD_UPTERM_DRAWER + ".info.uaem"):
        (vtc / name).unlink(missing_ok=True)

def stage_terminfo(src, dest):
    """VTC:terminfo (the rig's TERMINFO, GO) as the kit's terminal entries (make build/kit-terminfo/stamp:
    vtcon, vtcon-amiga, screen, in first-letter and hex directories). An older copy of it lacked
    vtcon-amiga, the TERM of an AMIGA-personality vsh window, and ncurses' clear answered
    "'vtcon-amiga': unknown terminal type" and printed nothing (W-T6, 2026-10-13)."""
    for f in sorted(src.glob("*/*")):
        if f.is_file():
            (dest / f.parent.name).mkdir(parents=True, exist_ok=True)
            shutil.copyfile(f, dest / f.parent.name / f.name)

def stage_amissl(vtc, m060):
    """VTC:amissl/AmiSSL/amissl_v362.library (RIG_LIBS): the kit's AmiSSL library for the CPU the rig
    boots as (68060 with --060/--max, else 68020-40), flat, where amisslmaster.library looks for it."""
    src = paths.VENDOR / "amissl-5.27/AmiSSL/Libs/AmigaOS3/AmiSSL" / ("68060" if m060 else "68020-40") / "amissl_v362.library"
    if not src.is_file():
        sys.exit("rig: the kit's AmiSSL 5.27 library not found: %s (vendor/amissl-5.27, the kit's AmiSSL drawer)" % src)
    dst = vtc / "amissl/AmiSSL" / src.name
    dst.parent.mkdir(parents=True, exist_ok=True)
    if not (dst.is_file() and dst.read_bytes() == src.read_bytes()):
        shutil.copyfile(src, dst)

def stage_ixemul(vtc, nofpu):
    """VTC:ixp6, first in LIBS: (RIG_LIBS), as the kit's patched ixemul.library and its ixnet.library, every
    start: the soft-float ixemul with --nofpu, else the FPU one. Before this only ixpty_rig and stackext_rig
    filled it, and a rig kept what they last put there: rig 7's 19 Oct ixemul (199528 bytes; the build's is
    200644) made amarchy-menu stop with "Illegal instruction" and draw no menu (2026-10-25). Those scripts
    still put their own library there after boot, for that boot."""
    ixemul = paths.IXEMUL_LIB_SOFT if nofpu else paths.IXEMUL_LIB
    for src, var in ((ixemul, "IXEMUL"), (paths.IXNET_LIB, "IXNET")):
        paths.require_input(src, var, src.name)
    libguard.require_clean_ixnet(paths.IXNET_LIB)
    (vtc / "ixp6").mkdir(parents=True, exist_ok=True)
    for src, name in ((ixemul, "ixemul.library"), (paths.IXNET_LIB, "ixnet.library")):
        dst = vtc / "ixp6" / name
        if not (dst.is_file() and dst.read_bytes() == src.read_bytes()):
            shutil.copyfile(src, dst)
            print("refreshed ixp6/%s" % name, dst.stat().st_size, "bytes")

def program_pairs(vtc, built):
    """The one list of (build file, VTC: copy) pairs for the build/amiga programs: each file of `built`, in VTC: and in
    the UP-Term drawer's bin. build_drift (the start [WARN]) checks these pairs, and refresh_programs (install, the
    start, the menu rig's stale_guard) copies them; before, install, start and restage each had their own set and
    the start printed 16 stale programs on every boot (RIG-START-VTC-DRIFT, MENU-STALE-RESTAGE-DRIFT)."""
    pairs = []
    for f in sorted(built.iterdir()):
        if f.is_file():
            pairs += [(f, vtc / f.name), (f, vtc / paths.UPTERM_DRAWER / "bin" / f.name)]
    return pairs

def refresh_programs(vtc, built, required=()):
    """Copies every program of program_pairs that differs (or, for the `required` names, is absent from VTC: itself).
    The copy replaces the file whole (temp file, rename), so a running rig never reads half a program."""
    done = []
    for src, dst in program_pairs(vtc, built):
        absent = src.name in required and dst.parent == vtc and not dst.is_file()
        if (absent or dst.is_file()) and (absent or dst.read_bytes() != src.read_bytes()):
            tmp = dst.with_name(dst.name + ".new")
            shutil.copyfile(src, tmp)
            os.replace(tmp, dst)
            print("refreshed", dst.relative_to(vtc), dst.stat().st_size, "bytes")
            done.append(str(dst.relative_to(vtc)))
    return done

def build_drift(vtc):
    """The VTC: files that differ from the host build they come from: build/amiga programs in VTC: and in the
    UP-Term drawer's bin, GG:bin/sh (vsh). install() refreshes them, but a rig not installed since the last build
    runs old ones and every check there tests old code (2026-10-25: rigs 3 and 4 held 16 and 19 stale programs,
    rigs 5 and 6 an old pty-handler, while rig 7's stale ixemul was hunted as a menu bug)."""
    stale = [str(dst.relative_to(vtc)) for src, dst in program_pairs(vtc, ROOT / "build/amiga")
             if dst.is_file() and dst.read_bytes() != src.read_bytes()]
    sh, vsh = vtc / "gg/bin/sh", ROOT / "build/amiga/vsh"
    if sh.is_file() and vsh.is_file() and sh.read_bytes() != vsh.read_bytes():
        stale.append("gg/bin/sh")
    for name in a1200_drift.DIST_SOURCES:
        src, dst = ROOT / "dist" / name, vtc / name
        if dst.is_file() and dst.read_bytes() != src.read_bytes():
            stale.append(name)
    for name in userland_names():   # staged by install for the boot's UP-Term:bin shadows
        dst = vtc / USERLAND_STAGE / name
        if dst.is_file() and dst.read_bytes() != (USERLAND_SRC / name).read_bytes():
            stale.append("%s/%s" % (USERLAND_STAGE, name))
    return stale

def kit_restore():
    """The next boot puts the kit's own files back (KIT_SHADOWS); `install` turns shadowing on again."""
    (RIG / "vtc/kit-restore").write_text("")
    print("kit-restore: reboot the rig (stop, start); the kit's files come back from VTC:kit-orig")

def refresh_kit_shadows(vtc):
    """Every program the boot puts at a kit name (KIT_SHADOWS) is the current build in VTC:, even
    when the drawer never held it (W-R3 Prefs editor, W-R4 UPDemo and the other C: commands).
    A stale VTC: copy is worse than none: each boot puts it over what the kit's install wrote, and
    a kit check after a reboot runs the old program (rig 3, 2026-10-19: Execute install.dos wrote
    the 237268-byte C:vsh, the next boot put VTC:'s 236784-byte one of 09:54 back)."""
    for name in sorted({x[0] for x in KIT_SHADOWS}):
        src, dst = kit_shadow_src(name), vtc / name
        if src.is_file() and not (dst.is_file() and dst.read_bytes() == src.read_bytes()):
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
            print("refreshed", name, dst.stat().st_size, "bytes")
    # GG:bin/sh is vsh too: the boot assigns GG: to VTC:gg when it holds one, and ixemul programs run it
    # as /gg/bin/sh (rig 3, 2026-10-24: an 8 Oct vsh there gave a pipeline's ixemul child a
    # non-interactive stdout, so upkg-ui tty:programbox found no terminal and exited 3)
    src, dst = build_amiga() / "vsh", vtc / "gg/bin/sh"
    if src.is_file() and dst.is_file() and dst.read_bytes() != src.read_bytes():
        shutil.copyfile(src, dst)
        print("refreshed gg/bin/sh", dst.stat().st_size, "bytes")

_MERGED_BUILD = None


def build_amiga():
    """The build/amiga directory `install` reads: this tree's, or after merge_build() one that also holds the main
    checkout's files this tree has not built."""
    return _MERGED_BUILD or ROOT / "build/amiga"


def merge_build():
    """A fresh worktree (`make worktree-ready` not run) has no build/amiga/*: every file it has not built is taken
    from the main checkout's build/amiga, one [WARN] line per file, this tree's own winning. Returns the directory
    install reads (symlinks, in a temporary directory) and sets it as build_amiga()."""
    global _MERGED_BUILD
    mine, main = ROOT / "build/amiga", paths.MAIN_CHECKOUT / "build/amiga"
    own = {f.name for f in mine.iterdir() if f.is_file()} if mine.is_dir() else set()
    theirs = [f for f in sorted(main.iterdir()) if f.is_file() and f.name not in own] \
        if main.is_dir() and main.resolve() != mine.resolve() else []
    if not theirs:
        _MERGED_BUILD = None
        return build_amiga()
    d = pathlib.Path(tempfile.mkdtemp(prefix="rig-build-amiga-"))
    for f in sorted(mine.iterdir()) if mine.is_dir() else []:
        if f.is_file():
            (d / f.name).symlink_to(f.resolve())
    for f in theirs:
        (d / f.name).symlink_to(f.resolve())
        print("[WARN] %s missing: installing the main checkout's %s (make worktree-ready, then make amiga, for this tree's)"
              % (mine / f.name, f))
    _MERGED_BUILD = d
    return d


def built_handler():
    """The handler every rig run needs, from build_amiga() (this tree's, else the main checkout's)."""
    f = build_amiga() / "vtcon-handler"
    if not f.is_file():
        sys.exit("[ERROR] no %s: run make amiga (make worktree-ready seeds build/ from the main checkout)" % f)
    return f


# Programs the rig scripts run as VTC:<name> that a rig's VTC: may not hold yet: install copies them even when
# absent (the loop below refreshes only files already there). UPTerm: typewin.quiet_screen_text runs
# `VTC:UPTerm ACTIVE screen FILE`; a rig whose VTC: predates it answered "Unknown command", and every
# screen_text() fell back to Select all + Copy (the owner saw the text menu fully selected, 2026-10-28).
VTC_REQUIRED = ("UPTerm",)


def install():
    with paths.long_run():
        _install()

def _install():
    merge_build()
    (RIG / "vtc/kit-restore").unlink(missing_ok=True)
    shutil.copyfile(built_handler(), RIG / "vtc/vtcon-handler")
    print("installed", (RIG / "vtc/vtcon-handler").stat().st_size, "bytes")
    # every other build/amiga program the drawer already holds: rig 2's VTC:
    # starts as a copy of old builds, and a script that runs VTC:<name> must
    # run the current one. The UP-Term drawer's bin too, the places build_drift checks: before, install
    # refreshed only VTC:, so the start [WARN] kept listing Apps/bin programs (vtcon-a1 P7, 2026-10-27)
    refresh_programs(RIG / "vtc", build_amiga(), VTC_REQUIRED)
    refresh_kit_shadows(RIG / "vtc")
    stage_amarchy_assets(RIG / "vtc")
    # the themes beside the handler: the rig has no kit, so no
    # ENVARC:up-term/themes, and the theme requesters and /theme fall back
    # to VTC:themes (prefs_theme_drawer, W30)
    # the glyph pages (UT1): UP-Term: is assigned to VTC:Apps on a rig with no
    # kit (GO), and the windows read UP-Term:unifont/ and emoji/
    for drawer in ("unifont", "emoji"):
        src = ROOT / "build" / drawer / "pages"
        if src.is_dir():
            dst = RIG / "vtc" / paths.UPTERM_DRAWER / drawer
            dst.mkdir(parents=True, exist_ok=True)
            for f in sorted(src.iterdir()):
                if f.is_file() and f.name != "stamp" and not (dst / f.name).is_file():
                    shutil.copyfile(f, dst / f.name)
        else:
            print("no %s: run make %s for glyphs above U+00FF on the rig" % (src, drawer))
    for vtc in sorted(paths.RIG_HOME.glob("rig*/vtc")):   # every rig drawer, the ones that are off too
        clean_stale_icons(vtc)
        migrate_upterm_drawer(vtc)
    stage_upterm(RIG / "vtc" / paths.UPTERM_DRAWER, build_amiga() / "vsh")
    if (ROOT / "build/kit-terminfo/stamp").is_file():
        stage_terminfo(ROOT / "build/kit-terminfo", RIG / "vtc/terminfo")
    else:
        print("no %s: run make build/kit-terminfo/stamp, or VTC:terminfo stays as it was" % (ROOT / "build/kit-terminfo"))
    (RIG / "vtc/themes").mkdir(exist_ok=True)
    for f in sorted((ROOT / "themes").glob("*.conf")):
        shutil.copyfile(f, RIG / "vtc/themes" / f.name)
    print("installed in VTC:; a rig with a kit runs the build after the next boot (KIT_SHADOWS copy it over the kit's files)")

def screenmode(name):
    """Install a ScreenMode prefs file (saved from ours) and reboot."""
    sys.path.insert(0, str(ROOT / "tools/rig"))
    import ami
    if name == "aga":
        cmd = b"Copy VTC:screenmode.aga16.prefs ENVARC:Sys/screenmode.prefs"
    elif name == "aga256":  # the aga16 file with depth 8 (SCRM sm_Depth)
        cmd = b"Copy VTC:screenmode.aga256.prefs ENVARC:Sys/screenmode.prefs"
    else:
        cmd = b"Copy VTC:screenmode.rtg.prefs ENVARC:Sys/screenmode.prefs"
    print(ami.req(0x02, (20).to_bytes(2, "big") + cmd)[4:].decode("latin-1"))
    SCREENMODE_NAME.write_text(name)   # the reboot's fresh sys.hdf gets the mode from here (sys_hdf_screenmode)
    args = reboot_args(sys.argv[2:])
    stop()
    # a new process: the machine flags are read from sys.argv when rig.py is loaded
    subprocess.run([sys.executable, str(pathlib.Path(__file__).resolve()), "start"] + args, check=True)

def reboot_args(given):
    """The arguments a reboot starts with: the ones given to this command, else the last start's."""
    if given:
        return list(given)
    try:
        return json.loads(START_ARGS.read_text())
    except (OSError, ValueError):
        return []

def reboot():
    """Stop safely (stop() flushes the volumes) and start again on the same sys.hdf. A start clones DH0: from the master,
    which threw away what an install wrote: installer_rig's reboot check failed on the clone and a master build lost its
    install (2026-10-27, RIG-REBOOT, MASTER-BUILD-NOREBOOT)."""
    args = reboot_args(sys.argv[2:])
    stop()
    # a new process: the machine flags are read from sys.argv when rig.py is loaded
    subprocess.run([sys.executable, str(pathlib.Path(__file__).resolve()), "start", "--keep-sys"] + args, check=True)

def mine():
    out = subprocess.run(["ps", "-eo", "pid,command"], capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if str(CFG) in l and "fs-uae" in l.lower()]

LOGWATCH_EVERY = 30   # seconds between two log caps while a rig runs

def logwatch():
    """Runs beside FS-UAE (start spawns it) and caps the FS-UAE logs and serial.log every LOGWATCH_EVERY s
    until the rig stops. ami.req caps too, but a rig nobody talks to gets no ami.req, and one run's log
    filled the Mac's disk (7.9 GB, 2026-10-17). One watcher per rig: a second one exits on the lock."""
    paths.FSUAE_LOG_DIR.mkdir(parents=True, exist_ok=True)
    with open(paths.FSUAE_LOG_DIR / ".logwatch.lock", "w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            return
        time.sleep(LOGWATCH_EVERY)   # the emulator process shows up during this first wait
        while mine():
            paths.cap_fsuae_log(extra=[SERIAL])
            time.sleep(LOGWATCH_EVERY)

START_RETRIES = 2

def start(retries=START_RETRIES):
    """Boot; a boot that stops right after "go started" (the emulator froze
    before anything under test loaded: seen 3 times on 2026-09-29, cause not
    found) is killed and retried."""
    # before anything is written: a start while the rig runs (or boots, when
    # status still says down) must not rewrite BOOTX:go, BOOTX:s/startup-sequence
    # and BOOTX:Mountlist, or delete BOOTX:boot.log, under the running system
    if mine():
        print("already running")
        return
    # the system disk is new from the master on every start: nothing a dead run wrote into DH0: survives, and a rig
    # that never had one gets it here. Only here: no emulator of this rig runs (mine() above).
    if KEEP_SYS and not OS and not OS32 and (RIG / "sys.hdf").exists():
        # `rig.py reboot`: DH0: stays as the last run left it. The clone-time edits are in it already; only the
        # wasabi block goes again (an install may have written it, and its wasabid takes port 1234, sys_hdf_no_wasabid)
        sys_hdf_no_wasabid(RIG / "sys.hdf")
    elif not OS and not OS32:
        fresh_sys_hdf()
        sys_hdf_screenmode(RIG / "sys.hdf")
        sys_hdf_no_wasabid(RIG / "sys.hdf")
        if "--no-dopus" in sys.argv:
            sys_hdf_no_dopus(RIG / "sys.hdf")
        sys_hdf_kit(RIG / "sys.hdf")
        sys_hdf_cpu_lib(RIG / "sys.hdf")   # the master image is never touched; this and any other write into DH0: runs after the clone
    RIG.mkdir(parents=True, exist_ok=True)
    START_ARGS.write_text(json.dumps([a for a in sys.argv[2:] if a != "--keep-sys"]))
    setup_config_only()
    paths.cap_fsuae_log(limit=0)   # a fresh log per start (the tail of the last run stays in <name>.tail)
    log = RIG / "boot/boot.log"
    if log.exists():
        log.unlink()
    if paths.FSUAE.suffix == ".app":
        subprocess.run(["open", "-g", "-n", "-a", str(paths.FSUAE), "--args", str(CFG)], check=True)
    else:  # a bare binary: detached, so the rig outlives this command; nice 19 so the
        # machine-load gates of other sessions do not block on the rig
        fin, fout, ferr = fsuae_stdio()
        subprocess.Popen(["nice", "-n", "19", str(paths.FSUAE), str(CFG)], stdin=fin,
                         stdout=fout, stderr=ferr, start_new_session=True)
    subprocess.Popen([sys.executable, str(pathlib.Path(__file__).resolve()), "logwatch"], stdin=subprocess.DEVNULL,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    for i in range(90):
        time.sleep(2)
        if status(quiet=True):
            print("up")
            missing = boot_assigns_missing()
            if missing:
                sys.exit("rig: the boot lacks the assigns %s (BOOTX:go or the boot hook did not run)" % " ".join(missing))
            guard_shell_startup()
            start_watchdog()
            if M060:
                retry_wb_reset()
            return
        text = log.read_text() if log.exists() else ""
        if i >= 60 and "go started" in text and "assigns done" not in text:
            break  # frozen early boot
    print("[ERROR] no wasabid answer; boot.log:",
          log.read_text().strip().replace("\n", " | ") if log.exists() else "(none: go never ran)")
    if retries > 0:
        print("retrying the boot")
        stop()
        return start(retries - 1)
    # out of retries: stop the last boot's emulator and fail the command. Falling off the end returned rc 0, so
    # getty_rig (check=True) went on against a rig that never came up and died in wasabi Conn with
    # ConnectionRefusedError, and the third boot's FS-UAE (with its logwatch) kept running unattended (rig 1, 2026-10-25).
    stop()
    sys.exit("rig: the boot failed %d times (no wasabid answer); see the boot.log lines above" % (START_RETRIES + 1))

# the assigns every rig script uses, made by the boot hook and BOOTX:go: a
# boot that lacks one has a requester waiting ("Please insert volume VTC:"
# three times on 2026-10-10: ENV:, UP-Term:, VTC:). Read from Assign LIST,
# which never asks for a volume.
BOOT_ASSIGNS = ["VTC", "ETC", "LIBS", "ENV"]

def boot_assigns_missing():
    """The names of BOOT_ASSIGNS (and GG: when VTC:gg/bin/sh is on the rig) the
    booted system does not have; [] when all are there."""
    import struct
    import ami
    out = ami.req(0x02, struct.pack(">H", 30) + b"Assign LIST", 60)[4:].decode("latin-1")
    have = {l.split()[0].rstrip(":").upper() for l in out.splitlines() if l.strip() and not l[0].isspace()}
    need = BOOT_ASSIGNS + (["GG"] if (RIG / "vtc/gg/bin/sh").exists() else [])
    return [n for n in need if n.upper() not in have]

def guard_shell_startup():
    """An earlier go's bare BARE_SHELL_PATH line in S:Shell-Startup becomes
    the guarded SHELL_STARTUP_PATH block (the system disk keeps it across
    setups, so go's own "added once" never rewrites it)."""
    import ami
    tmp = RIG / "shell-startup.tmp"
    try:
        tmp.write_bytes(ami.req(0x03, b"S:Shell-Startup"))
    except BaseException as e:
        print("[WARN] S:Shell-Startup not read:", e)
        return
    lines = tmp.read_text(encoding="latin-1").split("\n")
    if BARE_SHELL_PATH not in lines:
        tmp.unlink()
        return
    i = lines.index(BARE_SHELL_PATH)
    lines[i:i + 1] = SHELL_STARTUP_PATH
    tmp.write_text("\n".join(lines), encoding="latin-1")
    ami.put(str(tmp), "S:Shell-Startup")
    tmp.unlink()
    print("S:Shell-Startup: the VTC:gg/bin path line is guarded now")

def retry_wb_reset():
    """--060: the boot comes up on a PAL Workbench with Intuition's "attempting
    to reset the Workbench screen ... close all windows" requester (IPrefs'
    switch to the saved RTG mode raced a window; ledger W43). Retry then
    succeeds (the owner, 2026-10-05). The requester shows a while after
    wasabid answers, so wait for it (up to two minutes) and click the
    gadget labelled Retry.
    What it saw goes to build/rig/retry.log."""
    sys.path.insert(0, str(ROOT / "tools/rig"))
    import ami
    log = open(RIG / "retry.log", "a")
    t0 = time.time()
    while time.time() - t0 < 120:
        b = ami.req(0x0A)
        width = int.from_bytes(b[2:4], "big")
        if width != 640:
            if time.time() - t0 > 20:   # RTG, and nothing came for 20 s
                log.write("%.0fs RTG %d\n" % (time.time() - t0, width))
                return
            time.sleep(2)
            continue
        tree = ami.req(0x0D).decode("latin-1").splitlines()
        log.write("%.0fs PAL\n%s\n" % (time.time() - t0, "\n".join(tree[:20])))
        # the requester: the gadget labelled Retry; ami.py's UITREE gives gadgets
        # in absolute screen pixels
        win, gads = None, []
        for line in tree:
            f = line.split()
            if line.startswith("W "):
                win = (0, 0)
            elif line.startswith("G ") and win and line.rstrip().endswith('"Retry"'):
                w, h = map(int, f[4].split("x"))
                gads.append((win[0] + int(f[2]), win[1] + int(f[3]), w, h))
        if gads:
            x, y, w, h = min(gads)
            kx, ky = ami.pointer_scale()
            cx, cy = x + w // 2, y + h // 2
            log.write("click %d,%d\n" % (cx, cy))
            ami.script(("move", int(cx * kx), int(cy * ky)), ("wait", 2), ("button", 0, 1),
                       ("wait", 2), ("button", 0, 0), ("wait", 5))
            time.sleep(6)
        else:
            time.sleep(2)
    print("[WARN] the Workbench stayed PAL (W43)")

def stop():
    """Safe stop: killing FS-UAE while the Amiga writes to its disk corrupts the filesystem (rig 2, 2026-10-13).
    Wait until wasabid takes a command (nothing else is running), lock the volumes (the OS flushes them), stay
    quiet for 8 s, ask FS-UAE to quit (TERM), and kill it only if it ignores that."""
    if mine() and status(quiet=True):
        try:
            sys.path.insert(0, str(ROOT / "tools/rig"))
            import ami
            for _ in range(60):   # a running command answers "one at a time"/"still running": wait it out
                try:
                    ami.req(0x02, (10).to_bytes(2, "big") + b"Echo >NIL: idle", timeout=15)
                    break
                except BaseException:
                    time.sleep(5)
            for vol in ("DH0:", "DH1:"):
                try:
                    ami.req(0x02, (10).to_bytes(2, "big") + b"C:Lock " + vol.encode() + b" ON", timeout=15)
                except BaseException:
                    pass
            time.sleep(8)
        except BaseException:
            pass
    subprocess.run(["pkill", "-TERM", "-f", str(CFG)])
    for _ in range(90):   # FS-UAE closes its disks before it exits
        if not mine():
            break
        time.sleep(1)
    if mine():
        subprocess.run(["pkill", "-9", "-f", str(CFG)])
    print("stopped" if not mine() else "[ERROR] still running")

WATCHDOG_EVERY = 60   # seconds between two idle checks
WATCHDOG_QUIET = 5    # the last command must be at least this old: a stop never lands on a write in flight
WATCHDOG_IDLE_MINUTES = 60

def idle_minutes():
    """--idle-minutes N of this command line (default 60; 0 = no watchdog)."""
    if "--idle-minutes" in sys.argv[:-1]:
        return int(sys.argv[sys.argv.index("--idle-minutes") + 1])
    return WATCHDOG_IDLE_MINUTES

def watchdog_step(now, rig_n, idle_min, last_cmd, long_active, stop_fn):
    """One watchdog check; calls stop_fn() and returns True when the rig has been idle long enough.
    Never rig 1, never with idle_min 0, never while a long run holds its flag, never within WATCHDOG_QUIET s of
    a command; a rig nothing ever talked to counts as idle from last_cmd None only when idle_min is exceeded
    by the caller's start stamp (start touches the stamp)."""
    if rig_n == 1 or idle_min <= 0 or long_active or last_cmd is None:
        return False
    quiet = now - last_cmd
    if quiet < WATCHDOG_QUIET or quiet < idle_min * 60:
        return False
    stop_fn()
    return True

def start_watchdog():
    """Launch the detached watchdog for this rig (not rig 1, not with --idle-minutes 0)."""
    n = idle_minutes()
    if paths.RIG_N == 1 or n <= 0:
        return
    paths.touch_command()   # the idle clock starts at boot
    subprocess.Popen([sys.executable, str(pathlib.Path(__file__).resolve()), "watchdog", "--idle-minutes", str(n)],
                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    print("watchdog: stops rig %d after %d idle minutes (--idle-minutes 0 = off)" % (paths.RIG_N, n))

def watchdog():
    """The loop `start` detaches: every WATCHDOG_EVERY s one watchdog_step; leaves when the rig is gone (stopped by
    other means) or after it stopped it, through stop(), the safe stop. A newer watchdog replaces this one (pid file)."""
    pidf = RIG / "watchdog.pid"
    pidf.write_text(str(os.getpid()))
    log = RIG / "watchdog.log"
    n = idle_minutes()
    while pidf.exists() and pidf.read_text() == str(os.getpid()):
        if not mine():
            break
        def do_stop():
            with log.open("a") as f:
                f.write("%s idle %d min: stopping rig %d\n" % (time.strftime("%F %T"), n, paths.RIG_N))
            stop()
        if watchdog_step(time.time(), paths.RIG_N, n, paths.last_command(), paths.long_run_active(), do_stop):
            break
        time.sleep(WATCHDOG_EVERY)
    if pidf.exists() and pidf.read_text() == str(os.getpid()):
        pidf.unlink()

def status(quiet=False):
    sys.path.insert(0, str(ROOT / "tools/rig"))
    import ami
    try:
        r = ami.req(0x01, timeout=3)
        if not quiet:
            print(r.decode("latin-1"))
        return True
    except BaseException:
        if not quiet:
            crash = paths.fsuae_crash() if mine() else None
            if crash:
                print("crashed: %s, %d CPU reset(s) since (fs-uae log %s)" % (crash[0], crash[1], paths.FSUAE_LOG_DIR))
            else:
                print("down")
        return False

def muforce():
    import muforce as mf
    mf.main(sys.argv[2:])

def wasabi():
    import wasabi as wb
    wb.main(sys.argv[2:])

def main(argv):
    """Run the command argv[1]. --help, -h, help and ? (alone or after a command) print this file's usage (rc 0), the command not run; no command or an
    unknown one prints it to stderr (rc 2)."""
    cmds = {"setup": setup, "start": start, "stop": stop, "reboot": reboot, "install": install, "kit-restore": kit_restore,
            "status": status, "logwatch": logwatch, "muforce": muforce, "dbg": dbg, "dbgbreak": dbgbreak, "watchdog": watchdog, "wasabi": wasabi, "aga": lambda: screenmode("aga"),
            "aga256": lambda: screenmode("aga256"), "rtg": lambda: screenmode("rtg")}
    helps = ("--help", "-h", "help", "?")
    if len(argv) > 1 and (argv[1] in helps or (argv[1] in cmds and any(a in helps for a in argv[2:]))):
        print(__doc__)   # also `rig.py stop --help`: usage, never the command's side effect
        return 0
    if len(argv) < 2 or argv[1] not in cmds:
        print(__doc__, file=sys.stderr)
        print("rig.py: %s" % ("no command" if len(argv) < 2 else "unknown command %r" % argv[1]), file=sys.stderr)
        return 2
    cmds[argv[1]]()
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
