#!/usr/bin/env python3
"""install_rig.py -- the install kit on the rig, the owner's way: unpack
build/dist/vtcon into VTC:distkit, run Install from its drawer, check what
it did (PTY: mounted and working, the patched ixemul in LIBS: with the
original kept), then Uninstall and check the rig is as before. The rig's
own image is left with its original ixemul. The rig must be up; `make
dist` first."""
import os, pathlib, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
ORIG_SIZE = 166972  # the rig's ixemul.library 48.2 as released

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

passed = total = 0
def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + seen.strip()) if seen and not ok else ''))

def lib_state():
    rc, out = run('List LIBS:ixemul.library#? LFORMAT "%N %L"')
    return dict(l.split() for l in out.splitlines() if l.strip())

def main():
    shutil.rmtree(VTC / "distkit", ignore_errors=True)
    shutil.copytree(ROOT / "build/dist/vtcon", VTC / "distkit")
    shutil.copyfile(ROOT / "build/amiga/ptytest", VTC / "ptytest")
    (VTC / "runinstall").write_text("CD VTC:distkit\nExecute Install\n")
    (VTC / "rununinstall").write_text("CD VTC:distkit\nExecute Uninstall\n")
    run('Execute VTC:rununinstall')  # a run that stopped half-way left things behind
    before = lib_state()
    check(before.get('ixemul.library') == str(ORIG_SIZE) and 'ixemul.library.orig' not in before,
          'before: the original ixemul, no .orig', str(before))
    rc, out = run('Execute VTC:runinstall', 120)
    check(rc == 0, 'Install runs', out)
    rc, out = run('Assign PTY: EXISTS DEVICES')
    check(rc == 0, 'Install mounted PTY:', out)
    rc, out = run('VTC:ptytest', 120)
    check(rc == 0 and 'FAIL' not in out, 'the installed PTY: passes ptytest', out[-300:])
    st = lib_state()
    check(st.get('ixemul.library.orig') == str(ORIG_SIZE), 'the original ixemul kept as .orig', str(st))
    rc, out = run('Search LIBS:ixemul.library UP-Term')
    check('UP-Term' in out, 'the patched ixemul is in LIBS:', out)
    rc, out = run('Execute VTC:rununinstall', 60)
    check(rc == 0, 'Uninstall runs', out)
    st = lib_state()
    check(st.get('ixemul.library') == str(ORIG_SIZE) and 'ixemul.library.orig' not in st,
          'after Uninstall: the original ixemul back, no .orig', str(st))
    left = [f for f in ('DEVS:DOSDrivers/PTY', 'DEVS:DOSDrivers/XCON', 'L:pty-handler',
                        'L:vtcon-handler', 'C:vsh') if run('List >NIL: %s' % f)[0] == 0]
    check(not left, 'Uninstall removed the files', ' '.join(left))
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

if __name__ == '__main__':
    sys.exit(main())
