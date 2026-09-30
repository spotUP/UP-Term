#!/usr/bin/env python3
"""install_rig.py -- the install kit on the rig, the owner's way: unpack
build/dist/vtcon into VTC:distkit, run Install from its drawer, check what
it did (PTY: mounted and working, the patched ixemul in LIBS: with the
original kept), then Uninstall and check the rig is as before. The rig's
own image is left with its original ixemul. The rig must be up; `make
dist` first."""
import os, pathlib, shutil, struct, sys, time
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
    for name in ("ptytest", "iconprobe", "wbrun"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "runinstall").write_text("CD VTC:distkit\nExecute Install\n")
    (VTC / "rununinstall").write_text("CD VTC:distkit\nExecute Uninstall\n")
    # LIBS: as the rig boots it (ixpty_rig.use_ixemul puts VTC:ixp6 first,
    # and Install would then replace and keep the copy there)
    run('Assign LIBS: DH0:Libs')
    run('Assign LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD')
    rc, terminfo_before = run('GetEnv TERMINFO')  # the rig's boot sets /VTC/terminfo
    terminfo_before = terminfo_before.strip()
    run('Execute VTC:rununinstall')  # a run that stopped half-way left things behind
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == terminfo_before and terminfo_before != '/ENV/up-term/terminfo',
          'Uninstall with nothing installed leaves the user\'s TERMINFO', out)
    before = lib_state()
    check(before.get('ixemul.library') == str(ORIG_SIZE) and 'ixemul.library.orig' not in before,
          'before: the original ixemul, no .orig', str(before))
    rc, out = run('Execute VTC:runinstall', 120)
    check(rc == 0, 'Install runs', out)
    rc, out = run('Assign PTY: EXISTS DEVICES')
    check(rc == 0, 'Install mounted PTY:', out)
    rc, out = run('Assign IXPIPE: EXISTS DEVICES')
    check(rc == 0, 'Install mounted IXPIPE:', out)
    rc, out = run('VTC:ptytest', 120)
    check(rc == 0 and 'FAIL' not in out, 'the installed PTY: passes ptytest', out[-300:])
    st = lib_state()
    check(st.get('ixemul.library.orig') == str(ORIG_SIZE), 'the original ixemul kept as .orig', str(st))
    rc, out = run('Search LIBS:ixemul.library UP-Term')
    check('UP-Term' in out, 'the patched ixemul is in LIBS:', out)
    rc, out = run('Execute VTC:runinstall', 120)  # a second Install must not take ours for theirs
    check(rc == 0, 'Install again over it runs', out)
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == '/ENV/up-term/terminfo', 'TERMINFO is set (and can be)', out)
    rc, out = run('List >NIL: ENV:up-term/terminfo/v/vtcon')
    check(rc == 0, 'the vtcon entry is where TERMINFO points', out)
    rc, out = run('Type ENVARC:TERMINFO')  # a file now: the old ENVARC:terminfo drawer was the same name
    check(rc == 0 and out.strip() == '/ENV/up-term/terminfo', 'TERMINFO is kept in ENVARC: (no drawer by that name)', out)
    rc, out = run('VTC:iconprobe SYS:Utilities/UP-Term')
    check('tool C:vsh' in out and 'WINDOW=XCON:' in out, 'the UP-Term icon is in SYS:Utilities', out)
    rc, out = run('VTC:wbrun C:vsh SYS:Utilities/UP-Term', 30)
    time.sleep(3)
    tree = ami.req(0x0D).decode('latin-1')
    check(rc == 0 and any(l.startswith('W ') and 'UP-Term' in l for l in tree.splitlines()),
          'a Workbench start of the icon opens the UP-Term window', out)
    ami.req(0x08, bytes([4]) + b'exit'); time.sleep(0.4); ami.key(0x44); time.sleep(2)
    tree = ami.req(0x0D).decode('latin-1')
    check(not any(l.startswith('W ') and 'UP-Term' in l for l in tree.splitlines()),
          'exit in it closes the window')
    rc, out = run('Execute VTC:rununinstall', 60)
    check(rc == 0, 'Uninstall runs', out)
    st = lib_state()
    check(st.get('ixemul.library') == str(ORIG_SIZE) and 'ixemul.library.orig' not in st,
          'after Uninstall: the original ixemul back, no .orig', str(st))
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == terminfo_before, 'after Uninstall: the TERMINFO from before Install is back', out)
    left = [f for f in ('DEVS:DOSDrivers/PTY', 'DEVS:DOSDrivers/XCON', 'L:pty-handler',
                        'L:vtcon-handler', 'L:ixpipe-handler', 'DEVS:DOSDrivers/IXPIPE', 'C:vsh', 'C:ixkill', 'ENVARC:up-term', 'ENVARC:up-term-orig', 'ENVARC:TERMINFO',
                        'SYS:Utilities/UP-Term', 'SYS:Utilities/UP-Term.info')
            if run('List >NIL: %s' % f)[0] == 0]
    check(not left, 'Uninstall removed the files', ' '.join(left))
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

if __name__ == '__main__':
    sys.exit(main())
