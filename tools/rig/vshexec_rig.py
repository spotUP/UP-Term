#!/usr/bin/env python3
"""vshexec_rig.py -- AmigaDOS Execute typed in vsh runs its script, as in a Shell.

C:Execute is a command file, not the Shell's own: vsh loaded it and ran it with
RunCommand in its runner process, and Execute only queues the script as the
Shell's next input. No Shell read that input, so the script never ran (rc 0, no
output) and stayed open: `upkg remove` ran no Uninstall and could not delete its
copy ("object is in use"). 2026-10-18, rig 2.

  1. Execute RAM:vx/a.dos writes RAM:vx/a.out (the script ran).
  2. Afterwards the script deletes (Execute did not leave it open).
  3. A .KEY script takes its argument: Execute RAM:vx/k.dos hello.
  4. The script's return code comes back: Quit 7 gives $? = 7.
  5. A program whose main returns -1 (opkg/upkg does on error) is not "not found": sh_os.run uses a negative
     status for that, so vsh masks the runner's negative rc to 0..255. VTC:rcneg gives $? = 255, no
     "not found" (needs `make build/amiga/rcneg`).
  6. A command's CLI standard input is the shell's own input, as the Shell keeps it: C:Type 47.4 (OS 3.2)
     takes any other as redirected and fails `Type file` with "wrong number of arguments" (A1200,
     2026-10-20; vshrc's banner). VTC:clistdin makes Type's test and returns 5 on the redirected path
     (needs `make build/amiga/clistdin`).

Run with the rig up: python3 tools/rig/vshexec_rig.py
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import pathlib, shutil
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


def script(name, text):
    (paths.RIG / 'vtc' / name).write_text(text)
    c.run('Copy VTC:%s RAM:vx/%s QUIET' % (name, name))


def main():
    shutil.copyfile(ROOT / 'build/amiga/vsh', paths.RIG / 'vtc/vsh')
    shutil.copyfile(ROOT / 'build/amiga/rcneg', paths.RIG / 'vtc/rcneg')
    shutil.copyfile(ROOT / 'build/amiga/clistdin', paths.RIG / 'vtc/clistdin')
    c.run('Delete RAM:vx ALL QUIET')
    c.run('MakeDir RAM:vx')
    try:
        script('vxa.dos', 'Echo ran >RAM:vx/a.out\n')
        rc, out = vsh('Execute RAM:vx/vxa.dos')
        got = c.run('Type RAM:vx/a.out')[1]
        check('ran' in got, 'Execute runs its script (RAM:vx/a.out written)', (rc, out, got))
        rc, out = c.run('Delete RAM:vx/vxa.dos')
        check(rc == 0, 'the script deletes afterwards (not left open)', (rc, out))
        script('vxk.dos', '.KEY WORD/A\nEcho "<WORD>" >RAM:vx/k.out\n')
        vsh('Execute RAM:vx/vxk.dos hello')
        got = c.run('Type RAM:vx/k.out')[1]
        check('hello' in got, 'a .KEY script takes its argument', got)
        script('vxq.dos', 'Quit 7\n')
        rc, out = vsh('Execute RAM:vx/vxq.dos; echo rc=$?')
        check('rc=7' in out, "the script's return code comes back (Quit 7)", out)
        rc, out = vsh('VTC:rcneg; echo rc=$?')
        check('not found' not in out and 'rc=255' in out, 'main returning -1 gives $? = 255, not "not found"', out)
        rc, out = vsh('VTC:clistdin; echo rc=$?')
        check('redirected=0' in out and 'rc=0' in out, "the command's CLI standard input is the shell's input", out)
    finally:
        c.run('Delete RAM:vx ALL QUIET')
        for n in ('vxa.dos', 'vxk.dos', 'vxq.dos'):
            (paths.RIG / 'vtc' / n).unlink(missing_ok=True)
    print('%d/%d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
