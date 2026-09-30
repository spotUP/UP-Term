#!/usr/bin/env python3
"""cdprobe_rig.py [--noscroll] -- DP4 of the console.device plan on the
rig: runs tests/amiga/cdprobe (the ROM console.device's answers to commands
0-14 and NSCMD_DEVICEQUERY on units 0/1/3, CONFLAG_NODRAW_ON_NEWSIZE, and
whether a wrapped line re-wraps when its SNIPMAP window widens). No
typing; the probe resizes its own windows. Screenshots the re-wrap window
before and after the resize (build/rig/shots/cdprobe-rewrap-*.png), prints
the log and saves it as build/rig/shots/cdprobe-ks<version>.log; exits
non-zero on a FAIL line. --noscroll skips CD_SETUPSCROLLBACK/POSITION (use
it when a run crashed on them: the log's last "sending" line names the
command). The rig must be up; `make build/amiga/cdprobe` first."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def main():
    shutil.copyfile(ROOT / "build/amiga/cdprobe", VTC / "cdprobe")
    OUT.mkdir(parents=True, exist_ok=True)
    run('Run >NIL: VTC:cdprobe%s' % (' NOSCROLL' if '--noscroll' in sys.argv else ''))
    shots, log = set(), ''
    for _ in range(240):
        time.sleep(1)
        rc, log = run('Type RAM:cdprobe.log')
        for name in re.findall(r'^SHOT (\S+)', log, re.M):
            if name not in shots:
                shots.add(name)
                time.sleep(1)
                ami.main(['shot', str(OUT / ('cdprobe-%s.png' % name))])
        if 'passed ' in log:
            break
    m = re.search(r'kickstart (\d+\.\d+)', log)
    (OUT / ('cdprobe-ks%s.log' % (m.group(1) if m else 'unknown'))).write_text(log)
    sys.stdout.write(log)
    if 'passed ' not in log:
        print('FAIL the probe did not finish (the last "sending" line names the command it was on)')
        return 1
    return 0 if 'FAIL' not in log else 1

if __name__ == '__main__':
    sys.exit(main())
