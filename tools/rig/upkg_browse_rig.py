#!/usr/bin/env python3
"""upkg_browse_rig.py -- `upkg browse` typed into an UP-Term window on the rig (upkg-browse plan, item D1).

Flow (the product's top-level entry point is the typed command; the sentinel is a file the install makes):
  0  `upkg update` against the served feed must return 0 (D2). A rig without working AmiSSL fails here, with
     a clear message: the run then moves to the A1200 (keys typed into the window with ami.py key).
  1  `upkg browse`; Down to the row up-vim (the row index comes from `upkg list`), `i` installs it. After the
     install the script waits for the sentinel UP-Term:pkg/up-vim/Uninstall, then Return goes back to the list.
  2  `r` on the same row (the cursor stays on it), the yes/no question "Remove up-vim?" is answered Yes
     (the y key: --defaultno starts on No), then Return at "Press Return to go back to the list."
     The sentinel must be gone.
  3  `q` leaves the browser.
`upkg list-installed` is asserted before the run, after the install and after the remove (up-vim absent,
present, absent). A screenshot follows every step into the out dir (the script never looks at them).

Needs: build/ipk with up-vim (make dist-ipk), dist/upkg-browse, and the kit-stage build of upterm-ports with
the `browse` row: build/sysroot/UP-Term/bin/upkg and upkg-ui. The rig is up (`rig.py start --max`; UPTERM_RIG=3
for rig 3). A rig with no UP-Term: gets the base from the kit of `make dist` (install_rig's runinstall) and loses
it again at the end (rununinstall). The files are copied into UP-Term:bin and left there. The feed is served over http from build/ipk.
The rig's UP-Term:etc/upkg.conf is replaced by the feed conf for the run (browse's child commands use the
default config) and put back at the end. The script stops the rig at the end.

  upkg_browse_rig.py [--out DIR]      report.md and the screenshots go to DIR (default build/<rig>/upkg-browse)
  upkg_browse_rig.py --help | -h | help | ?
"""
import argparse, os, pathlib, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import install_rig as ir
import typewin
import vshwin
import upkg_rig

SYSROOT_BIN = paths.repo("upterm-ports") / "build/sysroot/UP-Term/bin"
UPKG = SYSROOT_BIN / "upkg"
UPKG_UI = SYSROOT_BIN / "upkg-ui"
PKG = 'up-vim'
SENTINEL = 'UP-Term:pkg/%s/Uninstall' % PKG
DEFCONF = 'UP-Term:etc/upkg.conf'
SAVED = 'T:upkg.conf.browse-save'
TITLE = 'upkg-browse'
DOWN = 0x4D
LOG = []


def note(s):
    print(s)
    LOG.append(s)


def shot(out, name):
    ami.main(['shot', str(out / name)])


def wait_for(path, present, secs):
    end = time.time() + secs
    while time.time() < end:
        if upkg_rig.exists(path) == present:
            return True
        time.sleep(3)
    return upkg_rig.exists(path) == present


def must(ok, what, seen=''):
    """install_rig's check() shape for a step the run cannot go on without."""
    if not ok:
        raise SystemExit('[ERROR] %s failed:\n%s' % (what, seen[-500:]))


def listed(cmd):
    """Names printed by `upkg <cmd>` (default config), in order."""
    rc, out = upkg_rig.upkg('browse-' + cmd.replace(' ', '-'), cmd, conf=False)
    if rc != 0:
        raise SystemExit('[ERROR] upkg %s returned %d:\n%s' % (cmd, rc, out[-400:]))
    return [l.split(' - ')[0].strip() for l in out.split('\n') if ' - ' in l]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-browse'))
    a = ap.parse_args(argv)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for need in (UPKG, UPKG_UI, ir.ROOT / 'dist/upkg-browse', ir.ROOT / 'build/ipk/Packages', ir.ROOT / ('build/ipk/%s_1.0_m68k-amigaos.ipk' % PKG)):
        if not need.exists():
            raise SystemExit('upkg_browse_rig: no %s (kit-stage build of upterm-ports / make dist-ipk first)' % need)
    ck = {}

    def check(name, ok, detail=''):
        ck[name] = bool(ok)
        note('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail))

    with paths.rig_lock():
        rel = upkg_rig.Release()
        saved = based = False
        try:
            ir.put_vsh()
            if ir.run('Assign >NIL: UP-Term: EXISTS')[0] != 0:
                if not (ir.KIT / 'Files/install.dos').exists():
                    raise SystemExit('upkg_browse_rig: the rig has no UP-Term: and there is no kit at %s (make dist first)' % ir.KIT)
                ir.prepare(None)
                based = True
                ir.base_install(must)
                note('the base installed from %s' % ir.KIT)
            for n, src in (('upkg-browse', ir.ROOT / 'dist/upkg-browse'), ('upkg', UPKG), ('upkg-ui', UPKG_UI)):
                shutil.copy(src, ir.VTC / n)
                rc, o = ir.run('Copy VTC:%s UP-Term:bin/%s CLONE' % (n, n))
                if rc != 0:
                    raise SystemExit('[ERROR] Copy of %s into UP-Term:bin returned %d: %s' % (n, rc, o.strip()))
                ir.run('Protect UP-Term:bin/%s +rwed' % n)
            rel.set()
            (ir.VTC / 'browse-rig.conf').write_text(upkg_rig.conf_for((ir.ROOT / 'dist/upkg.conf').read_text(), rel.url))
            ir.run('Delete >NIL: %s QUIET' % SAVED)
            ir.run('Copy %s %s CLONE' % (DEFCONF, SAVED))
            saved = True
            ir.run('Copy VTC:browse-rig.conf %s' % DEFCONF)
            ir.run('Delete >NIL: %s ALL QUIET' % upkg_rig.STAGE)

            # step 0 (D2): upkg update must work on this rig
            rc, o = upkg_rig.upkg('browse-update', 'update', conf=False)
            if rc != 0:
                raise SystemExit('[ERROR] step 0: `upkg update` returned %d on this rig (no working AmiSSL or feed?); '
                                 'run this check on the A1200 instead (keys with ami.py key, Return = 44).\n%s' % (rc, o[-500:]))
            names = listed('list')
            if PKG not in names:
                raise SystemExit('[ERROR] %s is not in `upkg list` (feed has: %s)' % (PKG, ' '.join(names)))
            row = names.index(PKG)
            note('rows: %s; %s is row %d' % (' '.join(names), PKG, row))
            if PKG in listed('list-installed'):
                upkg_rig.upkg('browse-prerm', 'remove %s' % PKG, conf=False)
            check('before: %s not installed' % PKG, PKG not in listed('list-installed'))
            check('before: no sentinel', not upkg_rig.exists(SENTINEL))

            vshwin.open_vsh(TITLE, box='0/20/640/300')
            typewin._last_title = TITLE
            typewin.typ('upkg browse\r')
            time.sleep(8)
            shot(out, 'step1-list.png')

            # step 1: Down to the row, i installs
            for _ in range(row):
                typewin.key(DOWN, wait=0.3)
            shot(out, 'step1-cursor.png')
            typewin.typ('i')
            shot(out, 'step1-installing.png')
            check('install: sentinel %s appears' % SENTINEL, wait_for(SENTINEL, True, 240))
            time.sleep(3)
            shot(out, 'step1-installed.png')
            typewin.key(typewin.RET, wait=6)       # "Press Return to go back to the list."
            shot(out, 'step1-back.png')
            check('after install: %s in list-installed' % PKG, PKG in listed('list-installed'))

            # step 2: r, Yes, Return
            typewin.typ('r')
            time.sleep(3)
            shot(out, 'step2-question.png')
            typewin.typ('y')                       # y answers Yes at once (--defaultno starts on No)
            time.sleep(3)
            check('remove: sentinel gone', wait_for(SENTINEL, False, 240))
            time.sleep(3)
            shot(out, 'step2-removed.png')
            typewin.key(typewin.RET, wait=6)
            shot(out, 'step2-back.png')
            check('after remove: %s not in list-installed' % PKG, PKG not in listed('list-installed'))

            # step 3: q
            typewin.typ('q')
            time.sleep(3)
            shot(out, 'step3-quit.png')
            typewin.typ('echo BROWSE-QUIT-DONE\r')
            time.sleep(2)
            note('window text at the end:\n' + typewin.screen_text('RAM:browse-screen.txt')[-1000:])
        finally:
            if saved:
                ir.run('Copy %s %s CLONE' % (SAVED, DEFCONF))
                ir.run('Delete >NIL: %s QUIET' % SAVED)
            if based:
                ir.run_long('rununinstall')
            rel.close()
            ir.show_close()
            time.sleep(6)   # 5 s quiet before stop: a reset mid-write kills the disk
            subprocess.call([sys.executable, str(pathlib.Path(__file__).with_name('rig.py')), 'stop'])
    bad = [k for k, v in ck.items() if not v]
    head = 'Verdict: %s (%d of %d checks)' % ('FAIL' if bad or len(ck) < 7 else 'PASS', len(ck) - len(bad), len(ck))
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(LOG) + '\n')
    print(head)
    return 1 if bad or len(ck) < 7 else 0


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] in ('--help', '-h', 'help', '?'):
        print(__doc__)
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
