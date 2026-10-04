#!/usr/bin/env python3
"""mkrom.py -- Kickstart images with UP-Term in them (ledger R1, research
thoughts/shared/research/2026-10-04_upterm-in-rom.md). Run it with the
amitools venv: `make rom` (build/romvenv, `pip install amitools`).

  mkrom.py --kick <3.1 ROM> --module build/amiga/uprom --out build/rom

Writes into --out (build/rom, gitignored; ROMs never go into the repo):
  upterm-1m.rom   1 MB: the ext half ($E00000) holds the UP-Term module,
                  the kick half ($F80000) is the source ROM's modules
                  unchanged plus one 28-byte RomTag that points into the ext
                  half (exec 40.10 scans only $F00000-$1000000, so the ext
                  half itself is never scanned).
  upterm-kick.rom the kick half alone, upterm-ext.rom the ext half alone.
  report.txt      sizes, what a 512 KB image would have to drop.

The source ROM is split with romtool's database (amitools' splitdata knows
the Commodore 3.1 ROMs). Before building, the split modules are rebuilt
unchanged and must reproduce the source ROM byte for byte: that proves the
builder (and works around romtool 0.8's kickety-split filler, b"\\x0ff",
two bytes per byte). Every image is checked after it is built
(check_image): checksum, our RomTag, the module bytes. Refuses a module
with DATA or BSS hunks or more than one hunk: it runs from ROM.
"""
import argparse
import os
import struct
import sys

from amitools.binfmt.BinFmt import BinFmt
from amitools.binfmt.Relocate import Relocate
from amitools.binfmt.BinImage import SEGMENT_TYPE_CODE
import amitools.rom as rom
from amitools.rom.rombuilder import RomEntryRomHdr

KICK_ADDR = 0xF80000
EXT_ADDR = 0xE00000
RTF_AFTERDOS = 0x04
TAG_NAME = "UP-Term ROM"
# what a 512 KB 3.1 image could give up on a disk-booting A1200 / A500
# (they come back from LIBS:/DEVS: of a 3.1.4+ install, or are not needed);
# everything else boots the machine or runs the module (intuition, graphics,
# layers, gadtools, dos, the Shell, scsi.device, trackdisk, keymap, ...)
DROPPABLE = ["workbench.library", "wbtask", "icon.library", "audio.device",
             "mathffp.library", "mathieeesingbas", "ramdrive", "carddisk.device",
             "card.resource", "bootmenu", "con-handler"]


def fixed_romhdr_data(self, addr):
    """romtool 0.8's RomEntryRomHdr writes b"\\x0ff" (2 bytes) per filler
    byte; the original ROMs have 0xff"""
    return b"\xff" * self.skip + struct.pack(">II", 0x11114EF9, self.jmp_addr)


RomEntryRomHdr.get_data = fixed_romhdr_data


def split(kick_path):
    rs = rom.RomSplitter()
    if rs.find_rom(kick_path) is None:
        sys.exit("mkrom: %s is not in amitools' split database (a Commodore 3.1 ROM is)" % kick_path)
    mods = []
    for e in rs.get_all_entries():
        mods.append((e.name, rs.extract_bin_img(e)))
    return rs, mods


def build_kick(mods, extra=None):
    """the kick ROM from split modules (+ an extra raw entry: name, fn(addr))"""
    rb = rom.KickRomBuilder(512, base_addr=KICK_ADDR, fill_byte=0xFF, kickety_split=True)
    for name, img in mods:
        size = img.get_size()
        pad = (4 - size % 4) % 4
        if rb.cross_kickety_split(size + pad):
            rb.add_kickety_split()
        if rb.add_bin_img(name, img) is None:
            sys.exit("mkrom: %s" % rb.get_error())
        if pad:
            rb.add_padding(pad)
    if extra is not None:
        if rb.add_module(extra[0], extra[1]) is None:
            sys.exit("mkrom: %s" % rb.get_error())
    left = rb.get_bytes_left()
    data = rb.build_rom()
    if data is None:
        sys.exit("mkrom: %s" % rb.get_error())
    return bytes(data), left


class RomTagStub:
    """a RomTag in the kick half that exec finds; its init, name and id
    string are the module's own in the ext half"""

    def __init__(self, ext_tag):
        self.name = "uprom-stub"
        self.tag = ext_tag  # (flags, version, type, pri, name, id, init)

    def get_size(self):
        return 28

    def get_data(self, addr):
        fl, ver, ty, pri, nm, ids, ini = self.tag
        return struct.pack(">HIIBBBbIIIH", 0x4AFC, addr, addr + 26, fl, ver, ty, pri, nm, ids, ini, 0)


def load_module(path):
    img = BinFmt().load_image(path)
    segs = img.get_segments()
    if len(segs) != 1 or segs[0].seg_type != SEGMENT_TYPE_CODE:
        sys.exit("mkrom: %s must be one CODE hunk (it runs from ROM; no DATA, no BSS)" % path)
    seg = segs[0]
    for to in seg.get_reloc_to_segs():
        if to is not seg:
            sys.exit("mkrom: %s relocates into another hunk" % path)
    return img


def romtag_at(data, off, base):
    mw, mt, es, fl, ver, ty, pri, nm, ids, ini = struct.unpack_from(">HIIBBBbIII", data, off)
    if mw != 0x4AFC or mt != base + off:
        return None
    return fl, ver, ty, pri, nm, ids, ini


def cstr(data, addr, base):
    o = addr - base
    return data[o:data.index(b"\0", o)].decode("latin-1")


def check_image(kick, ext, module_img, blobs):
    """everything the boot will rely on, from the bytes of the image"""
    problems = []
    for name, img, base in (("kick", kick, KICK_ADDR), ("ext", ext, EXT_ADDR)):
        ka = rom.KickRomAccess(bytearray(img))
        if name == "kick" and not (ka.is_kick_rom() and ka.verify_check_sum()):
            problems.append("kick half: not a valid Kick ROM / checksum")
        if name == "ext" and not ka.check_header():
            problems.append("ext half: no ROM header")
    # our RomTag in the kick half, pointing into the ext module
    # (romtool's ResidentScan reads names inside one image only: ours is in
    # the ext half, so scan by hand, as exec does: every word, MatchTag)
    ours = []
    for off in range(0, len(kick) - 26, 2):
        t = romtag_at(kick, off, KICK_ADDR)
        if t and EXT_ADDR <= t[4] < EXT_ADDR + len(ext) and cstr(ext, t[4], EXT_ADDR) == TAG_NAME:
            ours.append(t)
    if len(ours) != 1:
        problems.append("kick half: %d %r RomTags (want 1)" % (len(ours), TAG_NAME))
    elif ours[0][0] != RTF_AFTERDOS or ours[0][3] != -101:
        problems.append("kick half: RomTag flags %02x pri %d" % (ours[0][0], ours[0][3]))
    mod_off = rom.KickRomAccess.EXT_HEADER_SIZE
    want = Relocate(module_img).relocate_one_block(EXT_ADDR + mod_off)
    if ext[mod_off:mod_off + len(want)] != bytes(want):
        problems.append("ext half: the module is not the relocated build/amiga/uprom")
    t = romtag_at(ext, mod_off, EXT_ADDR)
    if t is None:
        problems.append("ext half: no RomTag at the module's start")
    elif not (EXT_ADDR <= t[6] < EXT_ADDR + len(ext)):
        problems.append("ext half: rt_Init %08x outside the ext half" % t[6])
    for path in blobs:
        b = open(path, "rb").read()
        if ext.find(b) < 0:
            problems.append("ext half: %s is not in it byte for byte" % os.path.basename(path))
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kick", required=True)
    ap.add_argument("--module", required=True)
    ap.add_argument("--out", default="build/rom")
    ap.add_argument("--blobs", nargs="*", default=["build/amiga/up-console.device", "build/amiga/vtcon-handler"])
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    src = open(a.kick, "rb").read()
    rs, mods = split(a.kick)
    again, free_kick = build_kick(mods)
    if again != src:
        sys.exit("mkrom: the split modules do not rebuild %s byte for byte; refusing" % a.kick)
    module_img = load_module(a.module)
    msize = module_img.get_size()
    # ext half: the module at $E00010
    eb = rom.ExtRomBuilder(512, base_addr=EXT_ADDR, fill_byte=0xFF, add_footer=False,
                           rom_ver=None, kick_addr=KICK_ADDR)
    if eb.add_bin_img("uprom", module_img) is None:
        sys.exit("mkrom: ext half: %s" % eb.get_error())
    ext_free = eb.get_bytes_left()
    ext = bytes(eb.build_rom())
    tag = romtag_at(ext, rom.KickRomAccess.EXT_HEADER_SIZE, EXT_ADDR)
    if tag is None:
        sys.exit("mkrom: build/amiga/uprom does not start with its RomTag")
    # kick half: the source modules + the stub
    stub = RomTagStub(tag)
    rb = rom.KickRomBuilder(512, base_addr=KICK_ADDR, fill_byte=0xFF, kickety_split=True)
    for name, img in mods:
        size = img.get_size()
        pad = (4 - size % 4) % 4
        if rb.cross_kickety_split(size + pad):
            rb.add_kickety_split()
        rb.add_bin_img(name, img)
        if pad:
            rb.add_padding(pad)
    if rb._add_entry(stub) is None:
        sys.exit("mkrom: kick half: no room for the 28-byte RomTag: %s" % rb.get_error())
    kick = rb.build_rom()
    if kick is None:
        sys.exit("mkrom: kick half: %s" % rb.get_error())
    kick = bytes(kick)
    problems = check_image(kick, ext, module_img, a.blobs)
    if problems:
        sys.exit("mkrom: image checks failed:\n  " + "\n  ".join(problems))
    with open(os.path.join(a.out, "upterm-ext.rom"), "wb") as f:
        f.write(ext)
    with open(os.path.join(a.out, "upterm-kick.rom"), "wb") as f:
        f.write(kick)
    with open(os.path.join(a.out, "upterm-1m.rom"), "wb") as f:
        f.write(ext + kick)  # romtool combine's order: ext ($E0) first
    # 512 KB: what it would take
    sizes = {name: img.get_size() for name, img in mods}
    drop = sum(s for n, s in sizes.items() if any(n.startswith(d) for d in DROPPABLE))
    lines = [
        "source %s (%s), %d bytes free in it" % (a.kick, rs.remus_rom.name, free_kick),
        "UP-Term module %d bytes (%s)" % (msize, ", ".join(
            "%s %d" % (os.path.basename(p), os.path.getsize(p)) for p in a.blobs)),
        "1 MB: ext half %d bytes free after the module" % ext_free,
        "512 KB: droppable modules (%s) total %d bytes; with the free space %d of %d needed: %s"
        % (", ".join(DROPPABLE), drop, drop + free_kick, msize,
           "fits" if drop + free_kick >= msize else "does NOT fit, short by %d" % (msize - drop - free_kick)),
    ]
    for n in sorted(sizes, key=lambda n: -sizes[n]):
        lines.append("  %7d  %s" % (sizes[n], n))
    rep = "\n".join(lines) + "\n"
    open(os.path.join(a.out, "report.txt"), "w").write(rep)
    sys.stdout.write(rep)
    print("checks passed; wrote %s/upterm-1m.rom (+ -kick, -ext)" % a.out)


if __name__ == "__main__":
    main()
