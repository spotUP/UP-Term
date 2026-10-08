#!/usr/bin/env python3
"""The vtcon test rig: FS-UAE, A1200 (68020, KS 3.1 40.068, 8 MB fast), driven
through amiagent (TCP 7846, tools/rig/ami.py). UPTERM_RIG=2 selects a second,
independent rig: build/rig2, amiagent on 7847 (tools/rig/README.md).

  rig.py setup    copy the system disk once, write the config and boot drawer
  rig.py start    boot it in the background (--060: a 68060 + FPU at the host's speed, 128 MB Z3 (W41);
                  --max: the --060 machine with 1 GB Zorro III RAM;
                  --fast: the CPU at the host's speed; --os32: AmigaOS 3.2 -- the 3.2.3 ROM
                  and the owner's 3.2 install as DH0:, ledger T3;
                  --ro: DH0: read-only, stalls on writes;
                  --kick <file>: another Kickstart ROM for this boot;
                  --serial <path>: the serial port to that path, not serial.log)
  rig.py aga      Workbench on native AGA (PAL hires, 16 colours) from next boot
  rig.py rtg      Workbench back on the graphics card from next boot
  (The screen mode is the Workbench's ScreenMode prefs, not the config: the
  owner's way, 2026-09-29. aga/rtg need the rig up and reboot it.)
  rig.py stop     kill THIS rig only (matched by its config path, at kill time)
  rig.py install  copy build/amiga/vtcon-handler to the VTC: drawer
  rig.py status   is it up (amiagent ping)

Layout under build/rig/ (gitignored): sys.hdf (DH0:), boot/ (BOOTX:, boots
first: assigns, mounts XCON:, starts amiagent), vtc/ (VTC:, the binaries
under test), shots/.
"""
import os, pathlib, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths

ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG = paths.RIG      # build/rig, or build/rig2 with UPTERM_RIG=2
CFG = RIG / "vtcon-rig.fs-uae"
# the owner's system disk (2026-10-03: the rig's copy was taken from it;
# the old source sat in a session scratchpad that is gone). Only read: the
# rig works on its own copy, build/rig/sys.hdf.
SRC_HDF = paths.SYSTEM_HDF        # UPTERM_SYSTEM_HDF
# amiagent itself lives in build/rig/boot (the Up Rough demo system's agent,
# copied there once); the rig refuses to start without it
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
# owner's tree is only read. Same boot drawer, VTC: and amiagent.
OS32 = "--os32" in sys.argv
OS32_SRC = paths.OS32_TREE        # UPTERM_OS32_TREE
OS32_KICK = paths.OS32_ROM        # UPTERM_OS32_ROM
if OS32:
    KICK = OS32_KICK
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
# Classes and MUI) and ncurses. The boot script uses it, and install_rig.py
# puts it back after testing Install/Uninstall against the stock library.
RIG_LIBS = ["Assign >NIL: LIBS: VTC:ixp6",
            "Assign >NIL: LIBS: DH0:Libs ADD",
            "Assign >NIL: LIBS: DH0:Classes ADD",
            "Assign >NIL: LIBS: DH0:MUI/Libs ADD",
            "Assign >NIL: LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD"]

# The rig's lines in the system disk's S:Shell-Startup (go adds them once).
# The bare "Path >NIL: VTC:gg/bin ADD" an earlier go wrote raised "Please
# insert volume VTC" in every Shell that started before VTC: was assigned
# (go assigned it after its Wait 15), or on a boot without the rig; that
# requester kept Intuition from resetting the Workbench screen ("attempting
# to reset the Workbench screen") when Install CONSOLE DEVICE ran, and
# amiagent stopped answering until both were closed (2026-10-07).
# Assign EXISTS never asks for a volume.
SHELL_STARTUP_PATH = ["Assign >NIL: VTC: EXISTS", "If NOT WARN",
                      "  Path >NIL: VTC:gg/bin ADD", "EndIf"]
BARE_SHELL_PATH = "Path >NIL: VTC:gg/bin ADD"

GO = """FailAt 21
Echo >BOOTX:boot.log "go started"
C:Wait 15
C:Mount XCON: FROM BOOTX:Mountlist
C:Assign >NIL: AmiTCP: VTC:amitcp
C:Assign >NIL: ETC: VTC:etc
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
  ; as the kit puts UP-Term:bin there. amiagent gives its commands a
  ; bare path (Current_directory, C:), so a Path here would not reach the
  ; windows: S:Shell-Startup runs in each new Shell. Added once, guarded
  ; (SHELL_STARTUP_PATH): the disk boots without the rig too.
  C:Search >NIL: S:Shell-Startup "VTC:gg/bin" QUIET
  If WARN
%(shellpath)s
  EndIf
EndIf
C:SetEnv TERM vtcon
C:SetEnv TERMINFO /VTC/terminfo
C:SetEnv TERMCAP /etc/termcap
Echo >>BOOTX:boot.log "assigns done"
Run >NIL: SYS:System/RexxMast
Run >NIL: BOOTX:amiagent PORT=%(port)d TOKEN=rigtoken
Echo >>BOOTX:boot.log "amiagent started"
""" % {"port": paths.AGENT_PORT, "libs": "\n".join("  C:" + l for l in RIG_LIBS),
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
MAX = "--max" in sys.argv
M060 = "--060" in sys.argv or MAX
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

def setup():
    (RIG / "boot").mkdir(parents=True, exist_ok=True)
    if paths.RIG_N > 1 and not (RIG / "vtc").exists():
        # rig 2's VTC: starts as a copy of rig 1's (only read there; .uaem
        # files carry the protection bits); `install` and the scripts keep it
        print("copying rig 1's VTC: drawer ...")
        shutil.copytree(paths.VTCON / "build/rig/vtc", RIG / "vtc", symlinks=True)
    (RIG / "vtc").mkdir(exist_ok=True)
    (RIG / "shots").mkdir(exist_ok=True)
    if OS32 and not (RIG / "os32").exists():
        print("copying the 3.2 system (168 MB) ...")
        paths.require_input(OS32_SRC, "UPTERM_OS32_TREE", "the AmigaOS 3.2 tree")
        shutil.copytree(OS32_SRC, RIG / "os32", symlinks=True)
    if OS32:
        os32_hd_layout()
    if not OS32 and not (RIG / "sys.hdf").exists():
        print("copying the system disk (1.5 GB) ...")
        paths.require_input(SRC_HDF, "UPTERM_SYSTEM_HDF", "the system disk image")
        shutil.copyfile(SRC_HDF, RIG / "sys.hdf")
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
    # --stock: the agent gives every command process a 256 KB stack; on the
    # 2 MB machine that took an eighth of memory each and the window under
    # test ran out (2026-10-04, allocwatch). A stock A1200's Shell has 4 KB;
    # 16 KB keeps Avail readings honest.
    # The agent sets its commands' stack itself (NP_StackSize 262144, two
    # places in the binary): --stock runs a copy with 16384 there.
    if STOCK:
        a = bytearray((RIG / "boot/amiagent").read_bytes())
        for off in (0x752, 0x14f20):
            if a[off:off + 4] != bytes.fromhex("00040000"):
                sys.exit("rig: amiagent's stack constant moved; the --stock patch needs new offsets")
            a[off:off + 4] = (16384).to_bytes(4, "big")
        (RIG / "boot/amiagent.stock").write_bytes(bytes(a))
    (RIG / "boot/go").write_text(GO.replace("BOOTX:amiagent PORT", "BOOTX:amiagent.stock PORT")
                                 if STOCK else GO)
    (RIG / "boot/Mountlist").write_text(MOUNTLIST)
    # once, like the disk: the sources were in a session scratchpad, which
    # is gone after that session (the rig failed to start, 2026-09-30)
    if not (RIG / "boot/amiagent").exists() and paths.RIG_N > 1:
        # rig 2 takes its own copy of rig 1's agent (only read there)
        shutil.copyfile(paths.VTCON / "build/rig/boot/amiagent", RIG / "boot/amiagent")
        os.chmod(RIG / "boot/amiagent", 0o755)
    if not (RIG / "boot/amiagent").exists():
        sys.exit("rig: build/rig/boot/amiagent is missing (copy it from the Up Rough demo system)")
    machine = [
        "[fs-uae]", "amiga_model = A1200", "accuracy = 1", "chip_memory = 2048",
        "bsdsocket_library = 1",
    ] if STOCK else [
        "[fs-uae]", "amiga_model = A1200",
        "cpu = %s" % ("68060-NOMMU" if M060 else "68020"), "fpu = %s" % ("68060" if M060 else "68882"),
        "fast_memory = 8192",
        # 64 MB more, as an accelerator's: GNU screen with four panes (tcsh in
        # each) left 663 KB of the 8 MB (owner 2026-09-30: "you can add more ram")
        "zorro_iii_memory = %d" % (1048576 if MAX else 131072 if M060 else 65536),
        "bsdsocket_library = 1", "graphics_card = uaegfx",
        # the RTG card's pointer as a sprite: Picasso96's software pointer is
        # hidden and drawn again around every blit, so it flickered with the
        # blinking cursor (owner 2026-10-03: "the mouse pointer still blinks")
        "uae_gfxcard_hardware_sprite = true",
        "jit_compiler = %d" % (0 if EXACT else 1),
        # without this FS-UAE runs the A1200's 68020 at about its real speed
        # (no JIT on an ARM host): UPDemo BENCH and cellbench measured a
        # stock machine, 2026-10-04
        "uae_cpu_speed = %s" % ("max" if FAST or M060 else "real"),
        "uae_cpu_cycle_exact = %s" % ("true" if EXACT else "false"),
        "uae_cpu_compatible = %s" % ("true" if EXACT else "false"),
    ] + (["uae_cpu_no_unimplemented = false", "uae_fpu_no_unimplemented = false"] if M060 else [])
    paths.require_rom(KICK)
    CFG.write_text("\n".join(machine + [
        "hard_drive_0 = %s" % (RIG / ("os32" if OS32 else "sys.hdf")),
        # Writable: read-only (--ro) put up "Volume System is write
        # protected" requesters that stalled the rig (the owner saw them,
        # 2026-09-29). stop write-protects DH0: through amiagent before its
        # SIGKILL, so no write is left in flight to validate on next boot.
        "hard_drive_0_read_only = %d" % (0 if RW else 1),
        "hard_drive_1 = %s" % (RIG / "vtc"), "hard_drive_1_label = VTCX",
        "hard_drive_2 = %s" % (RIG / "boot"), "hard_drive_2_label = BOOTX",
        "hard_drive_2_priority = 10", "joystick_port_1 = none",
        "kickstart_file = %s" % KICK,
        "uae_sound_output = interrupts", "volume = 0", "initial_input_grab = 0",
        "window_width = 1280", "window_height = 1024",
        # the serial port into a file: kprintf traces (ixemul's DEBUG_VERSION)
        "serial_port = %s" % SERIAL,
        "screenshots_output_dir = %s" % (RIG / "shots"), ""]))
    if "start" not in sys.argv:
        print("rig ready:", CFG)

def setup_config_only():
    """Rewrite just the config, so --rw on or off takes effect."""
    if (RIG / "sys.hdf").exists():
        setup()

def install():
    shutil.copyfile(ROOT / "build/amiga/vtcon-handler", RIG / "vtc/vtcon-handler")
    print("installed", (RIG / "vtc/vtcon-handler").stat().st_size, "bytes")
    # every other build/amiga program the drawer already holds: rig 2's VTC:
    # starts as a copy of old builds, and a script that runs VTC:<name> must
    # run the current one
    for f in sorted((ROOT / "build/amiga").iterdir()):
        dst = RIG / "vtc" / f.name
        if f.is_file() and dst.is_file() and dst.read_bytes() != f.read_bytes():
            shutil.copyfile(f, dst)
            print("refreshed", f.name, dst.stat().st_size, "bytes")
    # the themes beside the handler: the rig has no kit, so no
    # ENVARC:up-term/themes, and the theme requesters and /theme fall back
    # to VTC:themes (prefs_theme_drawer, W30)
    (RIG / "vtc/themes").mkdir(exist_ok=True)
    for f in sorted((ROOT / "themes").glob("*.conf")):
        shutil.copyfile(f, RIG / "vtc/themes" / f.name)

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
    stop()
    start()

def mine():
    out = subprocess.run(["ps", "-eo", "pid,command"], capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if str(CFG) in l and "fs-uae" in l.lower()]

def start(retries=2):
    """Boot; a boot that stops right after "go started" (the emulator froze
    before anything under test loaded: seen 3 times on 2026-09-29, cause not
    found) is killed and retried."""
    # before anything is written: a start while the rig runs (or boots, when
    # status still says down) must not rewrite BOOTX:go, BOOTX:s/startup-sequence
    # and BOOTX:Mountlist, or delete BOOTX:boot.log, under the running system
    if mine():
        print("already running")
        return
    setup_config_only()
    log = RIG / "boot/boot.log"
    if log.exists():
        log.unlink()
    subprocess.run(["open", "-g", "-n", "-a", "/Applications/FS-UAE.app", "--args", str(CFG)], check=True)
    for i in range(90):
        time.sleep(2)
        if status(quiet=True):
            print("up")
            guard_shell_startup()
            if M060:
                retry_wb_reset()
            return
        text = log.read_text() if log.exists() else ""
        if i >= 60 and "go started" in text and "assigns done" not in text:
            break  # frozen early boot
    print("[ERROR] no amiagent; boot.log:",
          log.read_text().strip().replace("\n", " | ") if log.exists() else "(none: go never ran)")
    if retries > 0:
        print("retrying the boot")
        stop()
        start(retries - 1)

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
    amiagent answers, so wait for it (up to two minutes) and click the
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
        # the requester: the gadget labelled Retry; UITREE gives gadgets
        # relative to their window
        win, gads = None, []
        for line in tree:
            f = line.split()
            if line.startswith("W "):
                win = (int(f[2]), int(f[3]))
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
    if mine() and status(quiet=True):
        try:  # write-protect DH0: first, which flushes it (matters with --rw)
            sys.path.insert(0, str(ROOT / "tools/rig"))
            import ami
            ami.req(0x02, (10).to_bytes(2, "big") + b"C:Lock DH0: ON", timeout=15)
        except BaseException:
            pass
    subprocess.run(["pkill", "-9", "-f", str(CFG)])
    print("stopped" if not mine() else "[ERROR] still running")

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
            print("down")
        return False

if __name__ == "__main__":
    {"setup": setup, "start": start, "stop": stop, "install": install,
     "status": status, "aga": lambda: screenmode("aga"), "aga256": lambda: screenmode("aga256"),
     "rtg": lambda: screenmode("rtg")}[sys.argv[1]]()
