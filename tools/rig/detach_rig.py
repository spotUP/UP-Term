#!/usr/bin/env python3
"""detach_rig.py -- vsh leaves a detaching command's code alone (CygnusEd's ced crashed it).

An AmigaDOS Shell puts the command's seglist in Cli()->cli_Module and frees only what is still
there afterwards. A cback.o-style program (CygnusEd, HiP) takes its code by setting cli_Module to 0
and spawns a child that keeps running in it. vsh never set cli_Module and always called UnLoadSeg,
so the child's code was freed under it (Guru from vsh, fine from AmigaShell). 2026-10-26.

VTC:detach is the stand-in (tests/amiga/detach.c):
  1. cli_Module holds the program's own seglist while vsh runs it ("holds my code=1").
  2. Three seconds later the detached child asks whether its code memory is still allocated:
     "still allocated" (RAM:child.txt); the unfixed vsh frees it, and the child or the machine dies.

Run with the rig up (UPTERM_RIG=7): python3 tools/rig/detach_rig.py
Verdict log: DETACH_LOG or build/rig/detach.log.
"""
import sys, os, time, shutil, pathlib, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
LOG = pathlib.Path(os.environ.get('DETACH_LOG') or ROOT / 'build/detach.log')
passed = total = 0
lines = []


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    s = '%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else '')
    print(s, flush=True)
    lines.append(s)


def main():
    subprocess.run(['nice', '-n', '19', 'make', 'build/amiga/vsh', 'build/amiga/detach'], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    shutil.copyfile(ROOT / 'build/amiga/vsh', paths.RIG / 'vtc/vsh')
    shutil.copyfile(ROOT / 'build/amiga/detach', paths.RIG / 'vtc/detach')
    c.run('Delete RAM:child.txt QUIET')
    rc, out = c.run('VTC:vsh -c "VTC:detach"')
    print(repr(out), flush=True)
    check('holds my code=1' in out, 'cli_Module holds the command seglist while vsh runs it', out)
    time.sleep(6)
    try:
        rc, got = c.run('Type RAM:child.txt')
    except OSError as e:
        # the unfixed vsh: the freed code is reused, the machine Gurus and the rig stops answering
        got = 'rig stopped answering: %s; %s' % (e, paths.fsuae_crash() or 'no crash line')
    print(repr(got), flush=True)
    check('still allocated' in got, 'the detached child still has its code after vsh returned', got)
    try:
        c.run('Delete RAM:child.txt QUIET')
    except OSError:
        pass
    print('%d/%d' % (passed, total))
    LOG.write_text('\n'.join(lines) + '\n%d/%d\n' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
