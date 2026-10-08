#!/usr/bin/env python3
"""autoprobe_rig.py [CON|XCON] -- H5.2 on the rig: drives
tests/amiga/autoprobe (an AUTO/CLOSE window: close gadget, reopen on
write, ACTION_DISK_INFO / ACTION_UNDISK_INFO) and says after each step
whether the window is on the screen (amiagent's window list). Prints one
"STEP phase: window yes|no" line per step, then the probe's log; saves both
as build/rig/shots/autoprobe-<dev>-<dos version>.log. The rig must be up;
`make build/amiga/autoprobe` first. The XCON reopen hang it caught (fixed
2026-09-30, vr_free) struck about one run in three: when touching window
open/close, run it several times with `rig.py stop; rig.py start` between."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
OUT = paths.RIG / "shots"
CLICK_AFTER = ("wrote", "diskinfo", "undisk")

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def main():
    dev = sys.argv[1] if len(sys.argv) > 1 else 'CON'
    shutil.copyfile(ROOT / "build/amiga/autoprobe", VTC / "autoprobe")
    run('Delete RAM:autoprobe.log RAM:autoprobe.go QUIET')
    run('Run >NIL: VTC:autoprobe ' + dev)
    kx, ky = ami.pointer_scale()
    seen, out, log = set(), [], ''
    for _ in range(240):
        time.sleep(0.5)
        log = run('Type RAM:autoprobe.log')[1]
        if 'passed ' in log:
            break
        for ph in re.findall(r'^PHASE (\S+)', log, re.M):
            if ph in seen:
                continue
            seen.add(ph)
            time.sleep(1.5)
            w = ami.window('autoprobe')
            out.append('STEP %s: window %s' % (ph, 'yes' if w else 'no'))
            print(out[-1])
            if ph in CLICK_AFTER and w:
                x, y, ww, hh = w['sys:close']
                cx, cy = x + ww // 2, y + hh // 2
                ami.script(('move', int(cx * kx), int(cy * ky)), ('wait', 2), ('button', 0, 1),
                           ('wait', 2), ('button', 0, 0), ('wait', 5))
                time.sleep(1.5)
            # the probe polls the file: a redirection that meets its read
            # fails ("object in use"), so write until it reads back
            for _ in range(10):
                run('Echo >RAM:autoprobe.go "%s"' % ph)
                if run('Type RAM:autoprobe.go')[1].strip() == ph:
                    break
                time.sleep(0.3)
    ver = run('Version dos.library')[1].split()
    tag = ver[1] if len(ver) > 1 else 'unknown'
    (OUT / ('autoprobe-%s-dos%s.log' % (dev.lower(), tag))).write_text('\n'.join(out) + '\n' + log)
    sys.stdout.write(log)
    if 'passed ' not in log:
        print('FAIL the probe did not finish')
        return 1
    return 0 if 'FAIL' not in log else 1

if __name__ == '__main__':
    sys.exit(main())
