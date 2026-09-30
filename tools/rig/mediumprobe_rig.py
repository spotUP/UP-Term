#!/usr/bin/env python3
"""mediumprobe_rig.py [nowait] [reply] -- DP5 of the console.device plan on the rig: runs
tests/amiga/mediumprobe (a CON: window in V47 medium mode, SetMode 2) and
types "ab", TAB, Shift+TAB, Up, Down, "hello", RETURN, then "q", RETURN
into it. Prints the probe's log (one READ line per Read(), hex and text)
and saves it as build/rig/shots/mediumprobe-dos<version>.log; exits non-zero
on a FAIL line. Needs a 3.2 Kickstart (rig.py start --kick
~/Desktop/KICK_323.rom); `make build/amiga/mediumprobe` first."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"
TAB, UP, DOWN, RETURN, SHIFT = 0x42, 0x4C, 0x4D, 0x44, 0x0001

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def text(s):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.6)

def key(code, qual=0):
    ami.key(code, qual)
    time.sleep(0.6)

def main():
    shutil.copyfile(ROOT / "build/amiga/mediumprobe", VTC / "mediumprobe")
    OUT.mkdir(parents=True, exist_ok=True)
    run('Delete RAM:mediumprobe.log QUIET')
    args = ' '.join(a.upper() for a in sys.argv[1:])
    run('Run >NIL: VTC:mediumprobe ' + args)
    log = ''
    for _ in range(30):
        time.sleep(1)
        log = run('Type RAM:mediumprobe.log')[1]
        if 'READY' in log or 'FAIL' in log:
            break
    if 'READY' in log:
        time.sleep(1)
        text('ab')
        key(TAB)
        key(TAB, SHIFT)
        key(UP)
        key(DOWN)
        text('hello')
        key(RETURN)
        text('q')
        key(RETURN)
        for _ in range(20):
            time.sleep(1)
            log = run('Type RAM:mediumprobe.log')[1]
            if 'passed ' in log:
                break
    m = re.search(r'dos.library (\d+\.\d+)', log)
    tag = ''.join('-' + a.lower() for a in sys.argv[1:])
    (OUT / ('mediumprobe-dos%s%s.log' % (m.group(1) if m else 'unknown', tag))).write_text(log)
    sys.stdout.write(log)
    if 'passed ' not in log:
        print('FAIL the probe did not finish')
        return 1
    return 0 if 'FAIL' not in log else 1

if __name__ == '__main__':
    sys.exit(main())
