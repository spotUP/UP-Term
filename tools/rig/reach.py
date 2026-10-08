#!/usr/bin/env python3
"""The reachability test (ledger V3): on a running rig, install the fresh
handler build and the probe, run the probe through DOS, and require PASS.

  reach.py            (the rig must be up: tools/rig/rig.py start)

The handler is reloaded only by a reboot (DOS keeps its seglist), so this
reboots the rig first when the installed handler differs from the build.
"""
import filecmp, pathlib, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG = paths.RIG
sys.path.insert(0, str(ROOT / "tools/rig"))
import paths
import ami  # noqa: E402

def main():
    built = ROOT / "build/amiga/vtcon-handler"
    installed = RIG / "vtc/vtcon-handler"
    rig = [sys.executable, str(ROOT / "tools/rig/rig.py")]
    if not installed.exists() or not filecmp.cmp(built, installed, shallow=False):
        subprocess.run(rig + ["stop"], check=True)
        subprocess.run(rig + ["install"], check=True)
        subprocess.run(rig + ["start"], check=True)
    shutil.copyfile(ROOT / "build/amiga/reach", RIG / "vtc/reach")
    out = ami.req(0x02, (30).to_bytes(2, "big") + b"VTC:reach")
    rc = int.from_bytes(out[:4], "big")
    text = out[4:].decode("latin-1")
    print(text.rstrip())
    print("rc", rc)
    return 0 if rc == 0 and "PASS" in text else 1

if __name__ == "__main__":
    sys.exit(main())
