"""The Amiberry backend of the rig: a WinUAE-style .uae config from the machine rig.py describes, and the launch
command. rig.py keeps the machine decisions (CPU, RAM, drives, title text); this module only spells them in
Amiberry's keys, so the fs-uae and Amiberry configs come from one description (thoughts/shared/research/
s29-amiberry-port.md maps each fs-uae key to its line here)."""
import pathlib, os

APP = pathlib.Path(os.environ.get("UPTERM_AMIBERRY") or "/Applications/Amiberry.app")
BINARY = APP / "Contents/MacOS/Amiberry" if APP.suffix == ".app" else APP


def drive_line(n, d):
    """One mounted host directory (d["hdf"] false) or hard-disk image as an Amiberry uaehf line. A directory:
    uaehf<n>=dir,rw|ro,<device>:<volume>:<path>,<boot priority>. An image with an RDB: the geometry fields are 0."""
    rw = "ro" if d.get("ro") else "rw"
    pri = d.get("pri", 0)
    if d.get("hdf"):
        return "uaehf%d=hdf,%s,DH%d:%s,0,0,0,512,%d,,uae0" % (n, rw, n, d["path"], pri)
    return "uaehf%d=dir,%s,DH%d:%s:%s,%d" % (n, rw, n, d["label"], d["path"], pri)


def config_lines(m):
    """m: the machine as a dict (see rig.amiberry_machine). Returns the .uae lines."""
    z3 = m.get("z3_mb", 0)
    l = [
        "config_description=%s" % m["title"],
        "config_version=5.0.0",
        "use_gui=no",
        "kickstart_rom_file=%s" % m["rom"],
        "chipset=%s" % ("ocs" if m["model"] == "A500" else "aga"),
        "chipset_compatible=%s" % m["model"],
        "chipmem_size=%d" % (m["chip_kb"] // 512 if m["chip_kb"] >= 512 else 1),
        "cpu_model=%s" % m["cpu"],
        "cpu_speed=%s" % m["speed"],
        "cpu_compatible=%s" % ("true" if m["compatible"] else "false"),
        "cpu_cycle_exact=%s" % ("true" if m["cycle_exact"] else "false"),
        "cpu_24bit_addressing=%s" % ("true" if m["cpu"] in ("68000", "68010") else "false"),
        "cachesize=%d" % (8192 if m["jit"] else 0),
    ]
    if m.get("fpu"):
        l.append("fpu_model=%s" % m["fpu"])
    if m.get("mmu"):
        l.append("mmu_model=%s" % m["mmu"])
    if m.get("no_unimplemented"):
        l += ["cpu_no_unimplemented=false"] + (["fpu_no_unimplemented=false"] if m.get("fpu") else [])
    if m["fast_mb"]:
        l.append("fastmem_size=%d" % m["fast_mb"])
    if m["model"] == "A500":
        l.append("bogomem_size=2")   # the A500's 512 KB slow RAM: 256 KB units
    if z3:
        l.append("z3mem_size=%d" % z3)
    if m.get("rtg"):
        l += ["gfxcard_type=ZorroIII", "gfxcard_size=16", "gfxcard_hardware_sprite=true"]
    if m.get("bsdsocket"):
        l.append("bsdsocket_emu=true")
    for n, d in enumerate(m["drives"]):
        l.append(drive_line(n, d))
    l += [
        "joyport1=none",
        "sound_output=exact" if m["volume"] else "sound_output=none",
        "sound_volume=%d" % (100 - m["volume"]),   # Amiberry counts attenuation: 0 loud, 100 mute
        "floppy0sound=0",
        "clipboard_sharing=%s" % ("true" if m["clipboard"] else "false"),
        "gfx_width_windowed=%d" % m["window"][0],
        "gfx_height_windowed=%d" % m["window"][1],
        "gfx_fullscreen_amiga=false",
        "gfx_fullscreen_picasso=false",
    ]
    if m.get("shots_dir"):
        # Amiberry has no default screenshot hotkey (FS-UAE has its own); with this one the emulator writes a PNG
        # into the screenshot_dir that command() passes
        l.append("amiberry.screenshot=LCtrl+LShift+F10")
    if m.get("serial_tcp"):
        # Amiberry's serial_port is a device or TCP://host:port, never a file ("Error finding serial port <file>");
        # it listens, rig.py's serialwatch connects and writes the stream to serial.log
        l.append("serial_port=TCP://127.0.0.1:%d" % m["serial_tcp"])
    return l + m.get("extra", [])


def command(cfg, shots=None):
    """The argv that starts one rig: no GUI, the config, console log on stdout. shots: this rig's screenshot directory.
    Amiberry's .uae config has no such key ("unknown config entry"); the directory is an amiberry.conf setting
    (screenshot_dir, default ~/Documents/Amiberry/Screenshots) that `-o screenshot_dir=<dir>` overrides for this run
    only, so the shared amiberry.conf stays untouched."""
    argv = [str(BINARY), "-G", "--log", "-f", str(cfg)]
    if shots:
        argv += ["-o", "screenshot_dir=%s" % shots]
    return argv


def launch_path(rig_dir, cfg, title):
    """Amiberry titles its window "Amiberry - [<config file name>]" and has no title option, so the rig starts it on a
    symlink to the config named with the title text (rig, machine, --purpose). Every such link lives in <rig>/win/,
    which is also what rig.py matches a running rig by."""
    win = pathlib.Path(rig_dir) / "win"
    win.mkdir(exist_ok=True)
    for old in win.glob("*.uae"):
        old.unlink()
    link = win / (title.replace("/", "-") + ".uae")
    link.symlink_to(cfg)
    return link
