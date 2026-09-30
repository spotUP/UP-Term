#!/usr/bin/env python3
"""dosnode_rig.py -- DP3 of the console.device plan on the rig: runs
tests/amiga/dosnode, which dumps the CON:, RAW: and XCON: DosList entries
(and where their seglists live) and prints PRISTINE lines for the table in
device/upconsole.c. No typing. Prints the log and saves it as
build/rig/shots/dosnode-ks<version>.log (one per Kickstart of the DD22
matrix); exits non-zero on a FAIL line. The rig must be up;
`make build/amiga/dosnode` first."""
import os, pathlib, re, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def main():
    shutil.copyfile(ROOT / "build/amiga/dosnode", VTC / "dosnode")
    OUT.mkdir(parents=True, exist_ok=True)
    rc, out = run('VTC:dosnode')
    sys.stdout.write(out)
    m = re.search(r'kickstart (\d+\.\d+)', out)
    (OUT / ('dosnode-ks%s.log' % (m.group(1) if m else 'unknown'))).write_text(out)
    return 0 if rc == 0 and 'FAIL' not in out and 'passed ' in out else 1

if __name__ == '__main__':
    sys.exit(main())
