#!/usr/bin/env python3
"""install_rig.py -- the install kit on the rig, the owner's way: unpack
build/dist/UP-Term into VTC:distkit, run its Files/install.dos (what the kit's Installer script runs), check what
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
    shutil.copytree(ROOT / "build/dist/UP-Term", VTC / "distkit")
    for name in ("ptytest", "iconprobe", "wbrun", "conwho", "UPConsole"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    (VTC / "runinstall").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE\n")
    (VTC / "runinstallcon").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files CONSOLE DEVICE\n")
    (VTC / "rununinstall").write_text("CD VTC:distkit\nExecute Uninstall\n")
    # LIBS: as the rig boots it (ixpty_rig.use_ixemul puts VTC:ixp6 first,
    # and Install would then replace and keep the copy there)
    run('Assign LIBS: DH0:Libs')
    run('Assign LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD')
    rc, terminfo_before = run('GetEnv TERMINFO')  # the rig's boot sets /VTC/terminfo
    terminfo_before = terminfo_before.strip()
    run('Execute VTC:rununinstall')  # a run that stopped half-way left things behind
    startup_before = run('Type S:User-Startup')[1]
    run('Delete >NIL: ENVARC:UP-Term.prefs QUIET')  # an earlier run's kept preferences
    ls_before = run('List >NIL: C:ls')[0] == 0  # the user's own ls: Install must leave it
    FU = ('cp', 'mv', 'rm', 'mkdir', 'touch')
    fu_before = [t for t in FU if run('List >NIL: C:%s' % t)[0] == 0]  # likewise
    # the rig's boot assigns GG: (VTC:gg); take it away so Install's own
    # GG: set-up is what gets tested, and put it back at the end
    rig_gg = run('Assign >NIL: GG: EXISTS')[0] == 0
    if rig_gg:
        run('Assign GG:')
    gg_before = False
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
    rc, out = run('C:UPConsole STATUS')
    check('CON: ROM' in out, 'Install NOCONSOLE leaves CON: the ROM\'s', out)
    check(run('Search >NIL: S:User-Startup ";BEGIN UP-Term console"')[0] != 0,
          'Install NOCONSOLE writes no console block', '')
    check('console.device: ROM' in out, 'Install NODEVICE leaves console.device the ROM\'s', out)
    check(run('Search >NIL: S:User-Startup ";BEGIN UP-Term device"')[0] != 0,
          'Install NODEVICE writes no device block', '')
    # a second Install must not take ours for theirs; this one says yes to CON:
    rc, out = run('Execute VTC:runinstallcon', 120)
    check(rc == 0, 'Install CONSOLE again over it runs', out)
    rc, out = run('C:UPConsole STATUS')
    check('CON: UP-Term' in out and 'RAW: UP-Term' in out, 'Install CONSOLE: CON: and RAW: are UP-Term now', out)
    check('console.device: UP-Term' in out, 'Install DEVICE: console.device is UP-Term\'s now', out)
    rc, out = run('Search S:User-Startup "C:UPConsole >NIL: DEVICE ON"')
    check('DEVICE ON' in out, 'S:User-Startup switches console.device at boot', out)
    run('C:UPConsole DEVICE OFF')
    run('C:UPConsole >NIL: DEVICE ON')  # the block's own line, as a boot runs it
    rc, out = run('C:UPConsole STATUS')
    check('console.device: UP-Term' in out, 'the device block\'s line switches it again', out)
    import concon_rig
    who = concon_rig.shell_conwho('kit', 'RAM:who-kit.txt')
    check(who.startswith('UP-Term '), 'a NewShell CON: window runs the kit\'s handler (%s)' % who, who)
    rc, out = run('Search S:User-Startup "C:UPConsole >NIL: CON ON"')
    check('CON ON' in out, 'S:User-Startup switches CON: at boot', out)
    run('C:UPConsole CON OFF')
    run('C:UPConsole >NIL: CON ON')  # the block's own line, as a boot runs it
    rc, out = run('C:UPConsole STATUS')
    check('CON: UP-Term' in out, 'the block\'s line switches CON: again', out)
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == '/ENV/up-term/terminfo', 'TERMINFO is set (and can be)', out)
    rc, out = run('List >NIL: ENV:up-term/terminfo/v/vtcon')
    check(rc == 0, 'the vtcon entry is where TERMINFO points', out)
    rc, out = run('Type ENVARC:TERMINFO')  # a file now: the old ENVARC:terminfo drawer was the same name
    check(rc == 0 and out.strip() == '/ENV/up-term/terminfo', 'TERMINFO is kept in ENVARC: (no drawer by that name)', out)
    if not ls_before:
        rc, out = run('C:ls --version')
        check(rc == 0 and 'fileutils' in out and '3.15' in out, 'C:ls is GNU fileutils 3.15', out)
        check(run('List >NIL: ENVARC:up-term/ls')[0] == 0, 'Install marked C:ls as its own', '')
    if not fu_before:
        # the shims are gone: these are fileutils' own, and .. is the parent
        # (on RAM:, a real volume -- VTC:'s root is its own parent, an FS-UAE
        # quirk). rm -r runs from outside: AmigaDOS keeps the current
        # directory locked, so a shell can never delete the drawer it is in
        rc, out = run('C:vsh -c "mkdir -p RAM:fu/a/b && touch RAM:fu/a/b/t && cd RAM:fu/a/b && '
                      'cp t ../u && mv ../u ../../v && cd RAM: && rm -r fu/a && C:ls RAM:fu"', 30)
        check(rc == 0 and out.split() == ['v'], 'cp, mv, rm, mkdir and touch are fileutils\' and .. is the parent', out)
        rc, out = run('C:rm --version')
        check(rc == 0 and 'fileutils' in out and '3.15' in out, 'C:rm is GNU fileutils 3.15', out)
        check(all(run('List >NIL: ENVARC:up-term/%s' % t)[0] == 0 for t in FU),
              'Install marked cp, mv, rm, mkdir and touch as its own', '')
    run('Delete >NIL: RAM:fu ALL QUIET')
    check(run('List >NIL: ENVARC:up-term/up-term')[0] == 0, 'the window preferences file is in place', '')
    rc, out = run('C:tmux -V')
    check(rc == 0 and 'tmux 3.6a' in out, 'C:tmux runs', out)
    if not gg_before:
        rc, out = run('List >NIL: GG:bin/sh')
        check(rc == 0, 'no GG: before: vsh is GG:bin/sh (/gg/bin/sh for ixemul programs)', out)
        rc, out = run('Search S:User-Startup ";BEGIN UP-Term"')
        check(';BEGIN UP-Term' in out, 'S:User-Startup assigns GG: at boot', out)
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
    rc, out = run('VTC:UPConsole STATUS')
    check('CON: ROM' in out and 'RAW: ROM' in out, 'after Uninstall: CON: and RAW: are the ROM\'s', out)
    check('console.device: ROM' in out, 'after Uninstall: console.device is the ROM\'s', out)
    st = lib_state()
    check(st.get('ixemul.library') == str(ORIG_SIZE) and 'ixemul.library.orig' not in st,
          'after Uninstall: the original ixemul back, no .orig', str(st))
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == terminfo_before, 'after Uninstall: the TERMINFO from before Install is back', out)
    rc, out = run('Type S:User-Startup')
    check(out == startup_before, 'after Uninstall: S:User-Startup as it was, byte for byte',
          'differs: %r vs %r' % (out[-80:], startup_before[-80:]))
    if not gg_before:
        check(run('Assign >NIL: GG: EXISTS')[0] != 0, 'after Uninstall: no GG: (Install made it)')
    left = [f for f in ('DEVS:DOSDrivers/PTY', 'DEVS:DOSDrivers/XCON', 'L:pty-handler',
                        'L:vtcon-handler', 'L:ixpipe-handler', 'DEVS:DOSDrivers/IXPIPE', 'C:vsh', 'C:tmux', 'SYS:UP-Term', 'ENVARC:tmux.conf', 'C:ixkill', 'ENVARC:up-term', 'ENVARC:up-term-orig', 'ENVARC:TERMINFO',
                        'SYS:Utilities/UP-Term', 'SYS:Utilities/UP-Term.info', 'C:UPConsole', 'DEVS:up-console.device',
                        '"C:UP-Term Prefs"', 'SYS:Utilities/UP-Term-Prefs', 'SYS:Utilities/UP-Term-Prefs.info')
            if run('List >NIL: %s' % f)[0] == 0]
    if not ls_before:
        left += [f for f in ('C:ls', 'C:dircolors') if run('List >NIL: %s' % f)[0] == 0]
    left += ['C:' + t for t in FU if t not in fu_before and run('List >NIL: C:%s' % t)[0] == 0]
    check(not left, 'Uninstall removed the files', ' '.join(left))
    check(run('List >NIL: ENVARC:UP-Term.prefs')[0] == 0, 'Uninstall kept the user\'s preferences', '')
    run('Delete >NIL: ENVARC:UP-Term.prefs QUIET')
    if rig_gg:
        run('Assign GG: VTC:gg')
    # LIBS: as the rig runs (the patched ixemul first): this test set it to
    # the stock library, and whatever runs next on the rig needs ours
    import rig
    run('Avail >NIL: FLUSH')
    for line in rig.RIG_LIBS:
        run(line)
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

if __name__ == '__main__':
    sys.exit(main())
