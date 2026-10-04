#!/usr/bin/env python3
"""test_mkrom.py -- host checks of tools/mkrom.py (ledger R1): `make test-rom`.

Builds the images into a temporary directory and proves the checks are not
vacuous: a corrupt module byte, a missing RomTag and a module with DATA/BSS
must each be caught. Needs the amitools venv and a Commodore 3.1 ROM (KICK=).
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkrom  # noqa: E402

failed = 0


def check(name, ok):
    global failed
    print("[%s] %s" % ("OK" if ok else "FAIL", name))
    if not ok:
        failed += 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kick", required=True)
    ap.add_argument("--module", required=True)
    a = ap.parse_args()
    blobs = ["build/amiga/up-console.device", "build/amiga/vtcon-handler"]
    with tempfile.TemporaryDirectory(dir="build") as out:
        r = subprocess.run([sys.executable, "tools/mkrom.py", "--kick", a.kick, "--module", a.module,
                            "--out", out], capture_output=True, text=True)
        check("mkrom builds and its own checks pass", r.returncode == 0 and "checks passed" in r.stdout)
        img = open(os.path.join(out, "upterm-1m.rom"), "rb").read()
        ext, kick = img[:512 * 1024], img[512 * 1024:]
        check("1 MB image: ext half first, kick half second (romtool combine's order)",
              len(img) == 1024 * 1024 and kick == open(os.path.join(out, "upterm-kick.rom"), "rb").read())
        module = mkrom.load_module(a.module)
        check("the built image passes check_image", mkrom.check_image(kick, ext, module, blobs) == [])
        # every source module is still in the kick half, unchanged
        src = open(a.kick, "rb").read()
        rs, mods = mkrom.split(a.kick)
        again, _ = mkrom.build_kick(mods)
        check("the split modules rebuild the source ROM byte for byte", again == src)
        # a byte of the handler changed in the ext half
        hb = open(blobs[1], "rb").read()
        at = ext.find(hb) + len(hb) // 2
        bad = ext[:at] + bytes([ext[at] ^ 0xFF]) + ext[at + 1:]
        check("a changed handler byte is caught", any("vtcon-handler" in p for p in
                                                     mkrom.check_image(kick, bad, module, blobs)))
        # the stub RomTag gone from the kick half
        stubs = [o for o in range(0, len(kick) - 26, 2)
                 if (lambda t: t and mkrom.EXT_ADDR <= t[6] < mkrom.EXT_ADDR + len(ext))(
                     mkrom.romtag_at(kick, o, mkrom.KICK_ADDR))]
        check("exactly one RomTag in the kick half points into the ext half", len(stubs) == 1)
        nostub = kick[:stubs[0]] + b"\x00\x00" + kick[stubs[0] + 2:] if stubs else kick
        check("a missing RomTag in the kick half is caught",
              any("RomTags (want 1)" in p for p in mkrom.check_image(nostub, ext, module, blobs)))
        # a module with DATA/BSS (UPConsole is a normal program) is refused
        r = subprocess.run([sys.executable, "tools/mkrom.py", "--kick", a.kick, "--module",
                            "build/amiga/UPConsole", "--out", out], capture_output=True, text=True)
        check("a module with DATA/BSS hunks is refused", r.returncode != 0 and "one CODE hunk" in r.stderr)
    print("%d failed" % failed)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
