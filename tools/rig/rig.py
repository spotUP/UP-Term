#!/usr/bin/env python3
"""The vtcon test rig: FS-UAE, A1200 (68020, KS 3.1 40.068, 8 MB fast), driven
through amiagent (TCP 7846, tools/rig/ami.py).

  rig.py setup    copy the system disk once, write the config and boot drawer
  rig.py start    boot it in the background (--fast: the CPU at the host's speed; --os32: AmigaOS 3.2 -- the 3.2.3 ROM
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

ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG = ROOT / "build/rig"
CFG = RIG / "vtcon-rig.fs-uae"
# the owner's system disk (2026-10-03: the rig's copy was taken from it;
# the old source sat in a session scratchpad that is gone). Only read: the
# rig works on its own copy, build/rig/sys.hdf.
SRC_HDF = pathlib.Path.home() / "Downloads/nyhd2.hdf"
# amiagent itself lives in build/rig/boot (the Up Rough demo system's agent,
# copied there once); the rig refuses to start without it
# The default ROM's header says 40.63 (the A500/A600/A2000 3.1), whatever
# its file name says (measured 2026-09-30). --kick <file> boots another ROM
# (DP6 of the console.device plan); the config is rewritten on every start,
# so the next start without --kick is back on this one.
KICK = pathlib.Path("/Users/spot/Code/Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200")
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
OS32_SRC = pathlib.Path.home() / "Downloads/AmigaOS-3.2-full (1)"
OS32_KICK = pathlib.Path.home() / "Desktop/KICK_323.rom"
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

GO = """FailAt 21
Echo >BOOTX:boot.log "go started"
C:Wait 15
C:Assign >NIL: VTC: VTCX:
C:Mount XCON: FROM BOOTX:Mountlist
C:Assign >NIL: AmiTCP: VTC:amitcp
C:Assign >NIL: ETC: VTC:etc
C:Assign >NIL: LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD
; UP-Term as the owner uses it on the rig: the patched ixemul first in
; LIBS: (Classes and MUI stay), PTY: and IXPIPE:, and GG: for /gg/bin/sh
; (vsh), as the kit sets it up. VTC: exists only from here on: a
; User-Startup line cannot do this (it runs before this script's Wait).
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
EndIf
C:SetEnv TERM vtcon
C:SetEnv TERMINFO /VTC/terminfo
C:SetEnv TERMCAP /etc/termcap
Echo >>BOOTX:boot.log "assigns done"
Run >NIL: SYS:System/RexxMast
Run >NIL: BOOTX:amiagent TOKEN=rigtoken
Echo >>BOOTX:boot.log "amiagent started"
""" % {"libs": "\n".join("  C:" + l for l in RIG_LIBS)}
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
EXACT = "--exact" in sys.argv  # read-only DH0: makes "write protected" requesters that stall the rig


def setup():
    (RIG / "boot").mkdir(parents=True, exist_ok=True)
    (RIG / "vtc").mkdir(exist_ok=True)
    (RIG / "shots").mkdir(exist_ok=True)
    if OS32 and not (RIG / "os32").exists():
        print("copying the 3.2 system (168 MB) ...")
        shutil.copytree(OS32_SRC, RIG / "os32", symlinks=True)
    if not OS32 and not (RIG / "sys.hdf").exists():
        print("copying the system disk (1.5 GB) ...")
        shutil.copyfile(SRC_HDF, RIG / "sys.hdf")
    (RIG / "boot/s").mkdir(exist_ok=True)
    (RIG / "boot/s/startup-sequence").write_text(STARTUP)
    (RIG / "boot/go").write_text(GO)
    (RIG / "boot/Mountlist").write_text(MOUNTLIST)
    # once, like the disk: the sources were in a session scratchpad, which
    # is gone after that session (the rig failed to start, 2026-09-30)
    if not (RIG / "boot/amiagent").exists():
        sys.exit("rig: build/rig/boot/amiagent is missing (copy it from the Up Rough demo system)")
    CFG.write_text("\n".join([
        "[fs-uae]", "amiga_model = A1200", "cpu = 68020", "fpu = 68882", "fast_memory = 8192",
        # 64 MB more, as an accelerator's: GNU screen with four panes (tcsh in
        # each) left 663 KB of the 8 MB (owner 2026-09-30: "you can add more ram")
        "zorro_iii_memory = 65536",
        "bsdsocket_library = 1", "graphics_card = uaegfx",
        # the RTG card's pointer as a sprite: Picasso96's software pointer is
        # hidden and drawn again around every blit, so it flickered with the
        # blinking cursor (owner 2026-10-03: "the mouse pointer still blinks")
        "uae_gfxcard_hardware_sprite = true",
        "jit_compiler = %d" % (0 if EXACT else 1),
        # without this FS-UAE runs the A1200's 68020 at about its real speed
        # (no JIT on an ARM host): UPDemo BENCH and cellbench measured a
        # stock machine, 2026-10-04
        "uae_cpu_speed = %s" % ("max" if FAST else "real"),
        "uae_cpu_cycle_exact = %s" % ("true" if EXACT else "false"),
        "uae_cpu_compatible = %s" % ("true" if EXACT else "false"),
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
    setup_config_only()
    log = RIG / "boot/boot.log"
    if log.exists():
        log.unlink()
    if mine():
        print("already running")
        return
    subprocess.run(["open", "-g", "-n", "-a", "/Applications/FS-UAE.app", "--args", str(CFG)], check=True)
    for i in range(90):
        time.sleep(2)
        if status(quiet=True):
            print("up")
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
