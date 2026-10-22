#!/usr/bin/env python3
"""upkg_browse_extra_rig.py -- two `upkg browse` checks the plain upkg_browse_rig.py does not make (upkg browse-dialogs plan):

  B3  an interactive package (up-claude: Files/questions, control field UP-Term-Interactive: yes) is installed from the
      browser in the PLAIN window, not the programbox: its question ("Claude Code on your server") is on the window text,
      the answer typed there reaches ENVARC:Claude/remote, "Press Return to go back to the list." comes after it.
  B4  pipe timing under opkg patch 0024: a slow package (up-slow, built here into a temp feed drawer, never into build/ipk:
      its postinst prints SLOW-FIRST, sleeps 40 s, prints SLOW-LAST) installed from the browser; the window text, read
      every few seconds, must show SLOW-FIRST while SLOW-LAST is not there yet, and SLOW-LAST at least SLOW_GAP s later.

Setup and teardown are upkg_browse_rig's (base from the kit when stale, upkg copied, feed over http,
conf swapped, rig stopped at the end). Needs the same files as upkg_browse_rig plus build/ipk/up-claude_*.ipk.

  upkg_browse_extra_rig.py [--out DIR]       report.md and screenshots go to DIR (default build/<rig>/upkg-browse-extra)
"""
import argparse, os, pathlib, sys, tempfile, shutil, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import install_rig as ir
import typewin
import vshwin
import upkg_rig
import upkg_browse_rig as B

sys.path.insert(0, str(paths.repo('upterm-ports') / 'pkgs/opkg/check'))
import ipk as ipklib   # the host checks' .ipk writer

ASK = 'up-claude'
SLOW = 'up-slow'
QUESTION = 'Claude Code on your server'
ANSWER = 'rigC.lan 2525'
REMOTE = 'ENVARC:Claude/remote'
SLOW_GAP = 20           # seconds between the two lines on the window; the postinst sleeps 40
TITLE = B.TITLE


def slow_feed():
    """A temp feed drawer: build/ipk's .ipk files plus up-slow; (dir, Packages text)."""
    tmp = pathlib.Path(tempfile.mkdtemp(prefix='upkgslow'))
    for f in (B.ir.ROOT / 'build/ipk').glob('*.ipk'):
        (tmp / f.name).symlink_to(f.resolve())
    ctl = (b'Package: %s\nVersion: 1.0\nArchitecture: m68k-amigaos\nMaintainer: rig\nDescription: slow postinst check\n' % SLOW.encode())
    post = b'#!/bin/sh\necho SLOW-FIRST\nsleep 40\necho SLOW-LAST\n'
    data = ipklib.ipk([('./control', ctl, 0o644), ('./postinst', post, 0o755)], [('./slow.txt', b'x\n', 0o644)])
    fn = '%s_1.0_m68k-amigaos.ipk' % SLOW
    (tmp / fn).write_bytes(data)
    text = (B.ir.ROOT / 'build/ipk/Packages').read_text().rstrip('\n') + '\n\n' + ipklib.stanza(ctl, fn, data).decode() + '\n'
    return tmp, text


def open_browser(row):
    typewin.typ('upkg browse\r')
    time.sleep(8)
    for _ in range(row):
        typewin.key(B.DOWN, wait=0.3)
    typewin.key(typewin.RET, wait=4)      # the info box: Install, Back
    typewin.key(typewin.RET, wait=2)      # Install


def text():
    return typewin.screen_text('RAM:browse-screen.txt')


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-browse-extra'))
    a = ap.parse_args(argv)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    ck = {}

    def check(name, ok, detail=''):
        ck[name] = bool(ok)
        B.note('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail))

    def must(ok, what, seen=''):
        B.must(ok, what, seen)

    with paths.rig_lock():
        rel = upkg_rig.Release()
        st = B.State()
        tmp = None
        try:
            B.setup(rel, st, must)
            tmp, packages = slow_feed()
            rel.set(feed_dir=tmp, packages=packages)
            rc, o = upkg_rig.upkg('extra-update', 'update', conf=False)
            must(rc == 0, 'step 0: upkg update', o)
            names = B.listed('list')
            must(ASK in names and SLOW in names, 'rows', ' '.join(names))
            for p in (ASK, SLOW):
                if p in B.listed('list-installed'):
                    upkg_rig.upkg('extra-rm', 'remove %s' % p, conf=False)
            ir.run('Delete >NIL: %s QUIET' % REMOTE)
            vshwin.open_vsh(TITLE, box='0/20/640/300')
            typewin._last_title = TITLE

            # B3
            open_browser(names.index(ASK))
            time.sleep(10)
            B.shot(out, 'b3-question.png')
            t = text()
            B.note('B3 window text:\n' + t.rstrip()[-700:])
            check('B3: the question is on the plain window', QUESTION in t)
            check('B3: not in a programbox (no "upkg install" box title)', 'upkg install ' + ASK not in t)
            for _ in range(30):
                typewin.key(typewin.BS, wait=0.1)
            typewin.typ(ANSWER)
            typewin.key(typewin.RET, wait=2)
            end = time.time() + 420
            t = ''
            while time.time() < end and 'Press Return to go back to the list' not in t:
                time.sleep(5)
                t = text()
            B.shot(out, 'b3-done.png')
            check('B3: "Press Return to go back to the list."', 'Press Return to go back to the list' in t)
            rc, got = ir.run('Type %s' % REMOTE)
            check('B3: %s holds the typed answer' % REMOTE, rc == 0 and ANSWER in got, repr(got[-100:]))
            typewin.key(typewin.RET, wait=6)
            B.shot(out, 'b3-back.png')
            check('B3: %s in list-installed' % ASK, ASK in B.listed('list-installed'))
            typewin.typ('q')
            time.sleep(3)

            # B4: the browser reloads its lists at start; up-slow is the row of its own
            names = B.listed('list')
            open_browser(names.index(SLOW))
            t0 = time.time()
            first = last = None
            while time.time() - t0 < 150 and last is None:
                time.sleep(3)
                t = text()
                now = time.time() - t0
                if first is None and 'SLOW-FIRST' in t:
                    first = now
                    check('B4: SLOW-LAST not there yet when SLOW-FIRST shows', 'SLOW-LAST' not in t)
                    B.shot(out, 'b4-first.png')
                if 'SLOW-LAST' in t:
                    last = now
            B.shot(out, 'b4-last.png')
            B.note('B4 first=%s last=%s' % (first, last))
            check('B4: SLOW-FIRST reached the window while the postinst slept', first is not None)
            check('B4: SLOW-LAST came %d s or more later' % SLOW_GAP, first is not None and last is not None and last - first >= SLOW_GAP,
                  '%s -> %s' % (first, last))
            time.sleep(5)
            typewin.key(typewin.RET, wait=6)   # OK in the programbox
            typewin.typ('q')
            time.sleep(3)
            for p in (ASK, SLOW):
                if p in B.listed('list-installed'):
                    upkg_rig.upkg('extra-rm', 'remove %s' % p, conf=False)
            ir.run('Delete >NIL: %s QUIET' % REMOTE)
        finally:
            B.teardown(rel, st)
            if tmp:
                shutil.rmtree(tmp, ignore_errors=True)
    bad = [k for k, v in ck.items() if not v]
    head = 'Verdict: %s (%d of %d checks)' % ('FAIL' if bad or len(ck) < 8 else 'PASS', len(ck) - len(bad), len(ck))
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(B.LOG) + '\n')
    print(head)
    return 1 if bad or len(ck) < 8 else 0


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] in ('--help', '-h', 'help', '?'):
        print(__doc__)
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
