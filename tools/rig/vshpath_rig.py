#!/usr/bin/env python3
"""vshpath_rig.py -- ledger V2 on the rig: vsh takes Unix absolute names
("/vol/rest" = vol:rest) where the AmigaDOS meaning of a leading "/" (the
parent directory) names nothing, and keeps the AmigaDOS meaning where it does.

  1. A command by Unix name: /C/Version (from SYS:, which has no parent).
  2. A redirection by Unix name: > /RAM/v2.out.
  3. cd /RAM goes to RAM:.
  4. "/" alone is still the parent: in RAM:v2/sub, "cd /" is RAM:v2.
  5. The Amiga meaning wins where it names something: in RAM:v2/sub, a
     drawer RAM:v2/RAM exists, and "cd /RAM" goes there, not to RAM:.

Run with the rig up: python3 tools/rig/vshpath_rig.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import pathlib, shutil, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def vsh(cmd):
    return c.run('VTC:vsh -c "%s"' % cmd.replace('"', '*"'))


def main():
    shutil.copyfile(ROOT / 'build/amiga/vsh', paths.RIG / 'vtc/vsh')
    c.run('Delete RAM:v2 RAM:v2.out ALL QUIET')
    c.run('MakeDir RAM:v2 RAM:v2/sub RAM:v2/RAM')
    try:
        # C:Version is a file (Echo is the Shell's own on 3.1)
        rc, out = vsh('cd SYS: ; /C/Version exec.library')
        check('exec' in out.lower() and rc == 0, 'a command by Unix name (/C/Version from SYS:)', (rc, out))
        vsh('cd SYS: ; echo redirected > /RAM/v2.out')
        got = c.run('Type RAM:v2.out')[1]
        check('redirected' in got, 'a redirection by Unix name (> /RAM/v2.out)', got)
        rc, out = vsh('cd SYS: ; cd /RAM ; pwd')
        check(out.strip().lower().startswith('ram'), 'cd /RAM from SYS: goes to RAM:', out)
        rc, out = vsh('cd RAM:v2/sub ; cd / ; pwd')
        check(out.strip().lower().endswith('v2'), '"cd /" is still the parent directory', out)
        rc, out = vsh('cd RAM:v2/sub ; cd /RAM ; pwd')
        check(out.strip().lower().endswith('v2/ram'), 'the Amiga meaning wins where it names something', out)
    finally:
        c.run('Delete RAM:v2 RAM:v2.out ALL QUIET')
    print('vshpath_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
