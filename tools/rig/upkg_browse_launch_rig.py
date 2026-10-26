#!/usr/bin/env python3
"""upkg_browse_launch_rig.py -- `upkg browse` launches an installed package on Enter and removes it on Delete
(upkg-browse-launch plan, step L6). The entry point is the typed command `upkg browse` in a vsh window on the rig.

Feed: two packages built here with tools/pkgs.py (build_ipk / write_feed) from a temp table and a temp staged tree:
  up-launchchk  Launch: UP-Term:bin/launchprobe <token>  (tests/amiga/launchprobe.c: writes T:launch-sentinel with
                the token and its stack, carries "$STACK: 262144", exits 7)
  up-nano       Launch: UP-Term:bin/nano  (the ports build of nano; a real app)
Checks (each a PASS/FAIL line in report.md):
  binaries   the upkg and upkg-ui on the rig have the md5 of the build (the ports checkout, $UPTERM_PORTS)
  launch     Enter on the installed row up-launchchk: the sentinel holds the token, the window shows the browse list
             and the status "up-launchchk exited with code 7"; the reported stack is >= 262144
  delete     Delete on the installed row removes it: gone from `upkg list-installed`, no record drawer
  app        Enter on the installed row up-nano starts nano (screen text), Ctrl-X quits it, browse is back with
             "up-nano exited with code 0"
Needs: the kit of `make dist` (UPTERM_KIT=<kit drawer> when it is not in this checkout), the ports checkout with the
`upkg browse` build (UPTERM_PORTS=<checkout>, build/sysroot/UP-Term/bin/{upkg,upkg-ui,nano}), `make build/amiga/launchprobe`,
the rig up (UPTERM_RIG=8 rig.py start --max --purpose "..."). The rig's UP-Term:etc/upkg.conf is put back at the end; the
script stops the rig at the end.

  UPTERM_RIG=8 UPTERM_PORTS=<ports> UPTERM_KIT=<kit> upkg_browse_launch_rig.py [--out DIR]
"""
import argparse, hashlib, os, pathlib, re, shutil, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths
import ami
import install_rig as ir
import typewin
import vshwin
import upkg_rig
import upkg_browse_rig as ubr
import pkgs

PORTS = pathlib.Path(os.environ.get('UPTERM_PORTS') or paths.repo('upterm-ports'))
BIN = PORTS / 'build/sysroot/UP-Term/bin'
PROBE = ir.ROOT / 'build/amiga/launchprobe'
TOKEN = 'tok%d' % int(time.time())
SENTINEL = 'T:launch-sentinel'
COOKIE = 262144
DELETE = 0x46
TITLE = 'upkg-browse-launch'
CHK, NANO = 'up-launchchk', 'up-nano'
TABLE_HEAD = '# id  archive  version  needs-base  ixemul  record  handed  consumes  launch\n'
UNINSTALL = 'FailAt 21\nDelete >NIL: "UP-Term:bin/%(bin)s" ALL QUIET\nDelete >NIL: "UP-Term:pkg/%(a)s" ALL QUIET\nEcho "%(a)s removed."\n'
INSTALL = ('.KEY DIR/A,STEP/K\n.BRA {\n.KET }\nFailAt 21\nCD "{DIR}"\nMakeDir >NIL: UP-Term:bin UP-Term:pkg UP-Term:pkg/%(a)s\n'
           'Copy %(bin)s UP-Term:bin/%(bin)s CLONE QUIET\nCopy Uninstall UP-Term:pkg/%(a)s/Uninstall CLONE QUIET\n'
           'Copy VERSIONS UP-Term:pkg/%(a)s/VERSIONS CLONE QUIET\nCopy MANIFEST UP-Term:pkg/%(a)s/MANIFEST CLONE QUIET\n'
           'Echo "%(a)s installed."\n')


def md5(path):
    return hashlib.md5(pathlib.Path(path).read_bytes()).hexdigest()


def make_feed():
    """A temp build drawer with the two fixture packages staged; returns (build, ipk dir)."""
    build = pathlib.Path(tempfile.mkdtemp(prefix='launchfeed'))
    rows = []
    for a, i, binfile, launch in ((CHK, 'launchchk', PROBE, 'UP-Term:bin/launchprobe ' + TOKEN),
                                  (NANO, 'nano', BIN / 'nano', 'UP-Term:bin/nano')):
        files = build / 'dist' / a / 'Files'
        files.mkdir(parents=True)
        name = 'launchprobe' if i == 'launchchk' else 'nano'
        shutil.copy(binfile, files / name)
        (files / 'install.dos').write_text(INSTALL % {'a': a, 'bin': name})
        (files / 'Uninstall').write_text(UNINSTALL % {'a': a, 'bin': name})
        (files / 'VERSIONS').write_text('%s 1.0 fixture\n' % a)
        (files / 'MANIFEST').write_text('bin/%s\n' % name)
        pkgs.SUMMARY[i] = 'fixture package with Launch: %s' % launch
        rows.append('%s  %s  1.0  1.0  -  UP-Term:pkg/%s  -  vtcon  %s\n' % (i, a, a, launch))
    table = build / 'packages.txt'
    table.write_text(TABLE_HEAD + ''.join(rows))
    pkgs.ipk(build, table)
    pkgs.write_feed(build, table)
    return build, build / 'ipk'


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-browse-launch'))
    out = pathlib.Path(ap.parse_args(argv).out)
    out.mkdir(parents=True, exist_ok=True)
    for need in (BIN / 'upkg', BIN / 'upkg-ui', BIN / 'nano', PROBE):
        if not need.exists():
            raise SystemExit('upkg_browse_launch_rig: no %s' % need)
    ck = {}
    note, shot = ubr.note, ubr.shot

    def check(name, ok, detail=''):
        ck[name] = bool(ok)
        note('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail))

    def window(label, inverse_ok=False, want=None):
        # the clipboard read can hand back an older text (a stale copy): read again until `want` is in it
        for _ in range(3):
            t = window1(label, inverse_ok)
            if want is None or want in t:
                break
        return t

    def window1(label, inverse_ok=False):
        # nano draws its title and shortcut bars in inverse video, which is the same grey as a stuck selection:
        # for those screens the grey-row check is skipped (the text itself is still read and asserted)
        keep = typewin.assert_no_selection
        if inverse_ok:
            typewin.assert_no_selection = lambda title: None
        try:
            t = typewin.screen_text('RAM:launch-screen.txt')
        finally:
            typewin.assert_no_selection = keep
        note('window text "%s":\n%s' % (label, t.rstrip()[-900:]))
        return t

    build, feed = make_feed()
    ubr.IPK = feed / 'up-launchchk_1.0_m68k-amigaos.ipk'
    ubr.UPKG = BIN / 'upkg'
    with paths.rig_lock():
        rel = upkg_rig.Release()
        plain_set = rel.set
        rel.set = lambda: plain_set(feed)
        st = ubr.State()
        try:
            ubr.setup(rel, st)
            shutil.copy(BIN / 'md5sum', ir.VTC / 'md5sum')   # the base kit has no md5sum: the ports build's
            for n in ('upkg', 'upkg-ui'):
                shutil.copy(BIN / n, ir.VTC / n)
                rc, o = ir.run('Copy VTC:%s UP-Term:bin/%s CLONE' % (n, n))
                ubr.must(rc == 0, 'Copy of %s' % n, o)
                ir.run('Protect UP-Term:bin/%s +rwed' % n)
                rc, o = ir.run('VTC:md5sum UP-Term:bin/%s' % n)
                check('binary %s on the rig is the build (md5)' % n, md5(BIN / n) in o, '%s vs %s' % (md5(BIN / n), o.strip()[:40]))
            rc, o = upkg_rig.upkg('launch-update', 'update', conf=False)
            ubr.must(rc == 0, 'upkg update', o)
            for p in (CHK, NANO):
                if p in ubr.listed('list-installed'):
                    upkg_rig.upkg('launch-prerm', 'remove %s' % p, conf=False)
                rc, o = upkg_rig.upkg('launch-inst-' + p, 'install %s' % p, conf=False)
                ubr.must(rc == 0, 'upkg install %s' % p, o)
            inst = ubr.listed('list-installed')
            check('fixtures installed', CHK in inst and NANO in inst, ' '.join(inst))
            rc, o = ir.run('Type UP-Term:var/lib/opkg/info/%s.control' % CHK)
            note('control (rc %d): %s' % (rc, o.strip()[-300:]))
            names = ubr.listed('list')
            note('rows: ' + ' '.join(names))
            ir.run('Delete >NIL: %s QUIET' % SENTINEL)

            vshwin.open_vsh(TITLE, box='0/20/640/300')
            typewin._last_title = TITLE
            typewin.typ('upkg browse\r')
            time.sleep(8)
            ubr.requester_stop('upkg browse')
            shot(out, 'l1-list.png')
            t0 = window('browse list', want=NANO)
            check('footer offers Enter: Launch', 'Launch' in t0, '')

            # point 3/4: Enter on the installed up-launchchk row
            row = names.index(CHK)
            for _ in range(row):
                typewin.key(ubr.DOWN, wait=0.3)
            typewin.key(typewin.RET, wait=1)
            ubr.requester_stop('Enter on %s' % CHK)
            check('sentinel T:launch-sentinel appears', ubr.wait_for(SENTINEL, True, 60))
            time.sleep(6)
            shot(out, 'l2-after-launch.png')
            rc, o = ir.run('Type %s' % SENTINEL)
            note('sentinel: %s' % o.strip())
            check('sentinel holds the token', 'token %s ' % TOKEN in o, TOKEN)
            m = re.search(r'stack (\d+)', o)
            got = int(m.group(1)) if m else 0
            check('launched stack >= cookie %d' % COOKIE, got >= COOKIE, 'stack %d' % got)
            t1 = window('after launch', want=NANO)
            check('browse list is back', NANO in t1 and CHK in t1, '')
            check('status "%s exited with code 7"' % CHK, '%s exited with code 7' % CHK in t1, '')

            # point 6: Enter on up-nano (real app)
            ir.run('Delete >NIL: %s QUIET' % SENTINEL)
            for _ in range(names.index(NANO) - names.index(CHK)):
                typewin.key(ubr.DOWN, wait=0.3)
            typewin.key(typewin.RET, wait=8)
            ubr.requester_stop('Enter on %s' % NANO)
            shot(out, 'l3-nano.png')
            t2 = window('nano running', inverse_ok=True)
            check('nano is on screen', re.search(r'GNU nano|New Buffer|\^X Exit|\^G Help', t2) is not None, '')
            typewin.key(0x32, q=0x08, wait=1)   # Ctrl-X (rawkey X = 0x32, Control = 0x08)
            time.sleep(8)
            shot(out, 'l3-after-nano.png')
            t3 = window('after nano', want=CHK)
            check('browse list is back after nano', CHK in t3 and 'GNU nano' not in t3, '')
            check('status "%s exited with code 0"' % NANO, '%s exited with code 0' % NANO in t3, '')

            # point 5: Delete on the installed row up-launchchk
            for _ in range(names.index(NANO) - names.index(CHK)):
                typewin.key(ubr.UP, wait=0.3)
            typewin.key(DELETE, wait=3)
            ubr.requester_stop('Delete')
            shot(out, 'l4-delete.png')
            t4 = window('after Delete')
            if re.search(r'Remove', t4):
                typewin.typ('y')
                time.sleep(3)
            check('delete: record gone', ubr.wait_for('UP-Term:pkg/%s/Uninstall' % CHK, False, 240))
            time.sleep(5)
            typewin.key(typewin.RET, wait=6)
            ubr.requester_stop('OK after Delete')
            shot(out, 'l4-after-delete.png')
            window('after delete OK')
            check('delete: gone from list-installed', CHK not in ubr.listed('list-installed'))
            typewin.typ('q')
            time.sleep(3)
        finally:
            ubr.teardown(rel, st)
            shutil.rmtree(build, ignore_errors=True)
    bad = [k for k, v in ck.items() if not v]
    head = 'Verdict: %s (%d of %d checks)' % ('FAIL' if bad or len(ck) < 13 else 'PASS', len(ck) - len(bad), len(ck))
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(ubr.LOG) + '\n')
    print(head)
    return 1 if bad or len(ck) < 13 else 0


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] in ('--help', '-h', 'help', '?'):
        print(__doc__)
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
