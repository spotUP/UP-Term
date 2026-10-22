#!/usr/bin/env python3
"""upkg_browse_rig.py -- `upkg browse` typed into an UP-Term window on the rig (upkg-browse plan, item D1).

Flow (the product's top-level entry point is the typed command; the sentinel is a file the install makes):
  0  `upkg update` against the served feed must return 0 (D2). A rig without working AmiSSL fails here, with
     a clear message: the run then moves to the A1200 (keys typed into the window with ami.py key).
  1  `upkg browse`; Down to the row up-vim (the row index comes from `upkg list`), Return opens the info box
     (Install, Back), Return on Install runs the install: its output scrolls in the programbox until the
     sentinel UP-Term:pkg/up-vim/Uninstall appears, then Return (OK) goes back to the list.
  2  Return on the same row (the cursor stays on it): the info box now offers Remove, Back; Return on Remove asks
     "Remove up-vim?" (--defaultno starts on No), y answers Yes; the programbox shows the output, Return (OK) goes
     back to the list. The sentinel must be gone.
  3  `q` leaves the browser.
Colour: the row up-vim is green when installed (Z2 in the item text). No live dump of the window's cell attributes exists in the
rig (vtdump replays a byte stream; snipprobe copies text only), so the colour is read from a screenshot of the
list: the number of green pixels in the window area (reusing colour_check.pixels) must rise
after the install and fall back after the remove.
No Shell text: the window text (Select all + Copy) of each screen between the browser's screens must hold no shell
prompt line and none of the old "Press Return to go back to the list." text.
`upkg list-installed` is asserted before the run, after the install and after the remove (up-vim absent,
present, absent). A screenshot follows every step into the out dir (the script never looks at them).

Needs: build/ipk with up-vim (make dist-ipk), the kit of `make dist`, and the kit-stage build of upterm-ports with
the `upkg browse` build: build/sysroot/UP-Term/bin/upkg. The rig is up (`rig.py start --max`; UPTERM_RIG=4
for rig 4). A rig with no UP-Term: gets the base from the kit of `make dist` (every option, CURL among them) and loses
it again at the end (rununinstall). A rig whose UP-Term:VERSIONS first line differs from the kit's gets the kit
installed over it and keeps it. The files are copied into UP-Term:bin and left there. The feed is served over http from build/ipk.
The rig's UP-Term:etc/upkg.conf is replaced by the feed conf for the run (browse's child commands use the
default config) and put back at the end. The script stops the rig at the end.

  upkg_browse_rig.py [--out DIR]      report.md and the screenshots go to DIR (default build/<rig>/upkg-browse)
  upkg_browse_rig.py --help | -h | help | ?
"""
import argparse, os, pathlib, re, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import install_rig as ir
import typewin
import vshwin
import upkg_rig
import colour_check

SYSROOT_BIN = paths.repo("upterm-ports") / "build/sysroot/UP-Term/bin"
UPKG = SYSROOT_BIN / "upkg"
PKG = 'up-vim'
SENTINEL = 'UP-Term:pkg/%s/Uninstall' % PKG
DEFCONF = 'UP-Term:etc/upkg.conf'
SAVED = 'T:upkg.conf.browse-save'
TITLE = 'upkg-browse'
DOWN = 0x4D
UP = 0x4C
LOG = []
texts = {}
GREEN_MIN = 20          # pixels
# what the old browser left in the window between its screens, and a vsh prompt line
SHELL_TEXT = re.compile(r'Press Return to go back|^\s*\S*[>#$%] ?\s*(upkg|echo)\b|^\d+\.[^ ]*> ')


def green_off_row(out, name):
    """upkg-ui draws the selected row in the base attribute (no colour), so the cursor goes one row up for the
    screenshot and back down onto up-vim."""
    typewin.key(UP, wait=0.8)
    shot(out, name)
    typewin.key(DOWN, wait=0.8)
    return green(out / name)


def green(png):
    """Pixels of the window area whose green clearly beats red and blue (the \\Z2 rows; colour_check's PNG reader)."""
    w, h, px = colour_check.pixels(str(png))
    return sum(1 for y in range(14, min(h, 320)) for x in range(0, min(w, 640))
               if px(x, y)[1] > px(x, y)[0] + 40 and px(x, y)[1] > px(x, y)[2] + 40)


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


def base_stale(installed, kit):
    """True when the rig's base is not the kit's: no UP-Term:VERSIONS (None or empty) or another first line.
    The first line names the release, the vtcon commit and the build date, so any rebuilt kit differs."""
    first = lambda t: (t or '').strip().splitlines()[:1]
    return not first(installed) or first(installed) != first(kit)


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


class State:
    """What setup() changed on the rig, for teardown()."""
    saved = based = False


def setup(rel, st, must=must):
    """The rig ready for a browse run: the kit's base (installed when stale), upkg in
    UP-Term:bin, the served feed `rel` and its conf as the default config. teardown() undoes it."""
    ir.put_vsh()
    had_base = ir.run('Assign >NIL: UP-Term: EXISTS')[0] == 0
    kit_versions = ir.KIT / 'Files/VERSIONS'
    if not kit_versions.exists():
        raise SystemExit('upkg_browse_rig: there is no kit at %s (make dist first)' % ir.KIT)
    installed = None
    if had_base:
        rc, installed = ir.run('Type UP-Term:VERSIONS')
        installed = installed if rc == 0 else None
    # an older base on the rig would test the old handler and vtcon, not the kit's
    if base_stale(installed, kit_versions.read_text()):
        ir.prepare(None)
        st.based = not had_base
        # upkg's download backend runs curl by name: only Install's CURL option puts it in UP-Term:bin
        (ir.VTC / 'runinstallbrowse').write_text('Execute VTC:distkit/Files/install.dos VTC:distkit/Files '
                                                 'NOCONSOLE NODEVICE SSH BEBBOGET CURL WASABI CPU060 UNIFONT EMOJI\n')
        ir.base_install(must, 'runinstallbrowse')
        # prepare() assigned LIBS: as Install sees it, without the rig's AmiSSL drawer: curl needs
        # amisslmaster.library even for the http feed
        for line in ir.rig_libs():
            ir.run(line)
        note('the base installed from %s (%s)' % (ir.KIT, 'over an older base' if had_base else 'no base before'))
    for n, src in (('upkg', UPKG),):
        shutil.copy(src, ir.VTC / n)
        rc, o = ir.run('Copy VTC:%s UP-Term:bin/%s CLONE' % (n, n))
        if rc != 0:
            raise SystemExit('[ERROR] Copy of %s into UP-Term:bin returned %d: %s' % (n, rc, o.strip()))
        ir.run('Protect UP-Term:bin/%s +rwed' % n)
    rel.set()
    (ir.VTC / 'browse-rig.conf').write_text(upkg_rig.conf_for((ir.ROOT / 'dist/upkg.conf').read_text(), rel.url))
    ir.run('Delete >NIL: %s QUIET' % SAVED)
    ir.run('Copy %s %s CLONE' % (DEFCONF, SAVED))
    st.saved = True
    ir.run('Copy VTC:browse-rig.conf %s' % DEFCONF)
    ir.run('Delete >NIL: %s ALL QUIET' % upkg_rig.STAGE)



def teardown(rel, st):
    if st.saved:
        ir.run('Copy %s %s CLONE' % (SAVED, DEFCONF))
        ir.run('Delete >NIL: %s QUIET' % SAVED)
    if st.based:
        ir.run_long('rununinstall')
    rel.close()
    ir.show_close()
    time.sleep(6)   # 5 s quiet before stop: a reset mid-write kills the disk
    subprocess.call([sys.executable, str(pathlib.Path(__file__).with_name('rig.py')), 'stop'])


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-browse'))
    a = ap.parse_args(argv)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for need in (UPKG, ir.ROOT / 'build/ipk/Packages', ir.ROOT / ('build/ipk/%s_1.0_m68k-amigaos.ipk' % PKG)):
        if not need.exists():
            raise SystemExit('upkg_browse_rig: no %s (kit-stage build of upterm-ports / make dist-ipk first)' % need)
    ck = {}

    def check(name, ok, detail=''):
        ck[name] = bool(ok)
        note('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail))

    with paths.rig_lock():
        rel = upkg_rig.Release()
        st = State()
        try:
            setup(rel, st, must)
            # step 0 (D2): upkg update must work on this rig
            rc, o = upkg_rig.upkg('browse-update', 'update', conf=False)
            check('step 0: upkg update returns 0', rc == 0, 'rc %d' % rc)
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

            # step 1: Down to the row, Return opens the info box, Return on Install
            for _ in range(row):
                typewin.key(DOWN, wait=0.3)
            shot(out, 'step1-cursor.png')
            g0 = green_off_row(out, 'step1-off-row0.png')
            texts['list before'] = typewin.screen_text('RAM:browse-screen.txt')
            typewin.key(typewin.RET, wait=4)       # the info box: Install, Back (Install is the first button)
            shot(out, 'step1-info.png')
            typewin.key(typewin.RET, wait=1)       # Install
            shot(out, 'step1-installing.png')
            check('install: sentinel %s appears' % SENTINEL, wait_for(SENTINEL, True, 420))
            time.sleep(5)
            shot(out, 'step1-installed.png')
            typewin.key(typewin.RET, wait=6)       # OK in the programbox
            shot(out, 'step1-back.png')
            g1 = green_off_row(out, 'step1-off-row.png')
            texts['list after install'] = typewin.screen_text('RAM:browse-screen.txt')
            check('after install: %s in list-installed' % PKG, PKG in listed('list-installed'))
            check('colour: green pixels rise after install', g1 > g0 + GREEN_MIN, '%d -> %d' % (g0, g1))

            # step 2: Return opens the info box (Remove, Back), Return on Remove, y, OK
            typewin.key(typewin.RET, wait=4)
            shot(out, 'step2-info.png')
            typewin.key(typewin.RET, wait=3)       # Remove
            shot(out, 'step2-question.png')
            typewin.typ('y')                       # y answers Yes at once (--defaultno starts on No)
            time.sleep(3)
            check('remove: sentinel gone', wait_for(SENTINEL, False, 240))
            time.sleep(5)
            shot(out, 'step2-removed.png')
            typewin.key(typewin.RET, wait=6)       # OK in the programbox
            shot(out, 'step2-back.png')
            g2 = green_off_row(out, 'step2-off-row.png')
            texts['list after remove'] = typewin.screen_text('RAM:browse-screen.txt')
            check('after remove: %s not in list-installed' % PKG, PKG not in listed('list-installed'))
            check('colour: green pixels fall after remove', g1 > g2 + GREEN_MIN and g2 <= g0 + GREEN_MIN, '%d -> %d (start %d)' % (g1, g2, g0))
            for k, t in texts.items():
                note('window text "%s":\n%s' % (k, t.rstrip()[-700:]))
                bad_lines = [l for l in t.split('\n') if SHELL_TEXT.search(l)]
                check('no Shell text on the screen "%s"' % k, not bad_lines, repr(bad_lines[:3]))

            # step 3: q
            typewin.typ('q')
            time.sleep(3)
            shot(out, 'step3-quit.png')
            typewin.typ('echo BROWSE-QUIT-DONE\r')
            time.sleep(2)
            note('window text at the end:\n' + typewin.screen_text('RAM:browse-screen.txt')[-1000:])
        finally:
            teardown(rel, st)
    bad = [k for k, v in ck.items() if not v]
    head = 'Verdict: %s (%d of %d checks)' % ('FAIL' if bad or len(ck) < 12 else 'PASS', len(ck) - len(bad), len(ck))
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(LOG) + '\n')
    print(head)
    return 1 if bad or len(ck) < 12 else 0


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] in ('--help', '-h', 'help', '?'):
        print(__doc__)
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
