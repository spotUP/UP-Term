#!/usr/bin/env python3
"""The vtcon test rig: FS-UAE, A1200 (68020, KS 3.1 40.068, 8 MB fast), driven
through amiagent (TCP 7846, tools/rig/ami.py).

  rig.py setup    copy the system disk once, write the config and boot drawer
  rig.py start    boot it in the background (never takes the owner's focus)
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
SRC_HDF = pathlib.Path("/private/tmp/claude-501/-Users-spot-Code-Up-Rough-Demo-System/"
                       "b2f93683-405c-45fc-b7dc-6a4daa5aa439/scratchpad/repro/sys.hdf")
SRC_AGENT = SRC_HDF.parent / "boot/amiagent"
KICK = pathlib.Path("/Users/spot/Code/Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200")

STARTUP = """DH0:C/Assign >NIL: SYS: DH0:
DH0:C/Assign >NIL: C: DH0:C
DH0:C/Assign >NIL: S: DH0:S
DH0:C/Assign >NIL: L: DH0:L
DH0:C/Assign >NIL: LIBS: DH0:Libs
DH0:C/Assign >NIL: DEVS: DH0:Devs
DH0:C/Assign >NIL: FONTS: DH0:Fonts
C:Run >NIL: C:Execute BOOTX:go
C:Execute DH0:S/Startup-Sequence
"""
GO = """C:Wait 15
C:Assign >NIL: VTC: VTCX:
C:Mount XCON: FROM BOOTX:Mountlist
C:Assign >NIL: AmiTCP: VTC:amitcp
C:Assign >NIL: LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD
C:SetEnv TERM vtcon
C:SetEnv TERMINFO /VTC/terminfo
Run >NIL: SYS:System/RexxMast
Run >NIL: BOOTX:amiagent TOKEN=rigtoken
"""
MOUNTLIST = """XCON:
   Handler   = VTC:vtcon-handler
   Priority  = 5
   StackSize = 16000
   GlobVec   = -1
#
"""

def setup():
    (RIG / "boot").mkdir(parents=True, exist_ok=True)
    (RIG / "vtc").mkdir(exist_ok=True)
    (RIG / "shots").mkdir(exist_ok=True)
    if not (RIG / "sys.hdf").exists():
        print("copying the system disk (1.5 GB) ...")
        shutil.copyfile(SRC_HDF, RIG / "sys.hdf")
    (RIG / "boot/s").mkdir(exist_ok=True)
    (RIG / "boot/s/startup-sequence").write_text(STARTUP)
    (RIG / "boot/go").write_text(GO)
    (RIG / "boot/Mountlist").write_text(MOUNTLIST)
    shutil.copyfile(SRC_AGENT, RIG / "boot/amiagent")
    CFG.write_text("\n".join([
        "[fs-uae]", "amiga_model = A1200", "cpu = 68020", "fpu = 68882", "fast_memory = 8192",
        "bsdsocket_library = 1", "graphics_card = uaegfx", "jit_compiler = 1",
        "hard_drive_0 = %s" % (RIG / "sys.hdf"),
        "hard_drive_1 = %s" % (RIG / "vtc"), "hard_drive_1_label = VTCX",
        "hard_drive_2 = %s" % (RIG / "boot"), "hard_drive_2_label = BOOTX",
        "hard_drive_2_priority = 10", "joystick_port_1 = none",
        "kickstart_file = %s" % KICK,
        "uae_sound_output = interrupts", "volume = 0", "initial_input_grab = 0",
        "window_width = 1280", "window_height = 1024",
        "screenshots_output_dir = %s" % (RIG / "shots"), ""]))
    print("rig ready:", CFG)

def install():
    shutil.copyfile(ROOT / "build/amiga/vtcon-handler", RIG / "vtc/vtcon-handler")
    print("installed", (RIG / "vtc/vtcon-handler").stat().st_size, "bytes")

def mine():
    out = subprocess.run(["ps", "-eo", "pid,command"], capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if str(CFG) in l and "fs-uae" in l.lower()]

def start():
    if mine():
        print("already running")
        return
    subprocess.run(["open", "-g", "-n", "-a", "/Applications/FS-UAE.app", "--args", str(CFG)], check=True)
    for _ in range(90):
        time.sleep(2)
        if status(quiet=True):
            print("up")
            return
    print("[ERROR] no amiagent after 180 s")

def stop():
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
     "status": status}[sys.argv[1]]()
