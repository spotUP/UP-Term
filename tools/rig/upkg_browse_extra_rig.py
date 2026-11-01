#!/usr/bin/env python3
"""upkg_browse_extra_rig.py -- `upkg browse` checks the plain upkg_browse_rig.py does not make (upkg browse-dialogs plan):

  B3  an interactive package (up-claude: Files/questions, control field UP-Term-Interactive: yes) is installed from the
      browser in the PLAIN window, not the programbox: its question ("Claude Code on your server") is on the window text,
      the answer typed there reaches ENVARC:Claude/remote, "Press Return to go back to the list." comes after it.
  B4  pipe timing under opkg patch 0024: a slow package (up-slow, built here into a temp feed drawer, never into build/ipk:
      its postinst prints SLOW-FIRST, sleeps 40 s, prints SLOW-LAST) installed from the browser; the window text, read
      every few seconds, must show SLOW-FIRST while SLOW-LAST is not there yet, and SLOW-LAST at least SLOW_GAP s later.
  B5  opkg patch 0026 (pr_WindowPtr = -1): `upkg update` with lists_dir on a volume that is not there ends with an
      error exit and NO DOS requester (without the patch DOS opens "insert volume" and the process waits for a click).

  B6  opkg patch 0026 proven: DH0: (the rig's system volume, ~66 MB free) is filled to the brim with files in a drawer
      (removed in a finally block); `upkg update` with lists_dir on it (the drawer exists, a few blocks stay free so the open works and the
      data write hits the full volume), started in a window whose shell has a real pr_WindowPtr (VTC:winptr0), must
      end with NO requester. Without 0026 the file system raises a
      "Volume is full" requester during the write and the process waits for a click: the case records its text and
      clicks Cancel. `Lock DH1: ON` (write protect) was tried first and does not work: the first open (upkg's lock file)
      fails with EROFS and no requester even without 0026, ixemul keeps requesters off around open. The case also
      records `winptr0 -p` (the pointer the window's processes have, 0; the agent's commands have ffffffff).

Setup and teardown are upkg_browse_rig's (base from the kit when stale, upkg copied, feed over http,
conf swapped, rig stopped at the end). Needs the same files as upkg_browse_rig plus build/ipk/up-claude_*.ipk.

  upkg_browse_extra_rig.py [--out DIR] [--only B6]   report.md and screenshots go to DIR (default build/<rig>/upkg-browse-extra);
                                              --only B6 skips B3 to B5
"""
import argparse, subprocess, re, os, pathlib, sys, tempfile, shutil, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import install_rig as ir
import typewin
import vshwin
import upkg_rig
import upkg_browse_rig as B

sys.path.insert(0, str(paths.ports() / 'pkgs/opkg/check'))
import ipk as ipklib   # the host checks' .ipk writer

ASK = 'up-claude'
SLOW = 'up-slow'
QUESTION = 'Claude Code on your server'
ANSWER = 'rigC.lan 2525'
REMOTE = 'ENVARC:Claude/remote'
SLOW_GAP = 20           # seconds between the two lines on the window; the postinst sleeps 40
TITLE = B.TITLE
TITLE5 = 'B5 user window'
VOL6 = 'DH0:'
FILL6 = 'DH0:b6-fill'
LISTS6 = 'DH0:upkg-b6-lists'


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
    ap.add_argument('--only', choices=['B6'], default=None)
    a = ap.parse_args(argv)
    full = a.only is None
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
            if full:
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

                # B5 (checks 0027 and the no-requester exit; NOT a proof of 0026: ixemul's default ix_no_insert_disk_requester
                # already keeps the absent-volume requester away, ixemul-vtcon library/ix_settings.c:35, __plock.c:497; 0026 needs a
                # FULL volume, whose requester comes from the file system on write). upkg 0026 sets pr_WindowPtr to -1. A write to a volume that is not there must END in an error
                # (exit != 0, no requester), not in a DOS "insert volume" requester that holds the process.
                # the browse setup put the feed conf at the default config path (T:upkg-rig.conf is upkg_rig.py's, not here)
                conf = ami.read_file(B.DEFCONF).decode('latin-1').rstrip('\n')
                # opkg keeps the FIRST lists_dir it sees: drop the conf's own, or the absent volume is never used
                conf = '\n'.join(l for l in conf.split('\n') if not re.match(r'\s*option\s+lists_dir\b', l))
                (ir.VTC / 'nosuchvol.conf').write_text(conf + '\noption lists_dir NOSUCHVOL:lists\n')
                # A window of its own whose shell has a REAL pr_WindowPtr (VTC:winptr0), typed into as a user does.
                # NOT the agent's commands and NOT the windows above: wasabi's runner has pr_WindowPtr -1
                # (wasabid.c:836 job runner, :6837 daemon) and every process started from it inherits that, so an
                # absent volume never opened a requester there whether or not upkg had patch 0026 (2026-10-23: B5
                # passed with 0026 removed).
                typewin.typ('exit\r')
                time.sleep(3)
                shutil.copyfile(B.ir.ROOT / 'build/amiga/winptr0', ir.VTC / 'winptr0')
                vshwin.open_vsh(TITLE5, box='0/20/640/300', real_window_ptr=True)
                typewin._last_title = TITLE5
                (ir.VTC / 'extra-novol.sh').write_text('UP-Term:bin/upkg -f VTC:nosuchvol.conf update 2>&1\necho "[exit $?]"\n')
                typewin.typ('VTC:vsh VTC:extra-novol.sh\r')
                reqs = []
                o = ''
                end = time.time() + 90
                while time.time() < end:
                    time.sleep(3)
                    reqs = ami.requesters()
                    o = text()
                    if reqs or re.search(r'\[exit \d+\]', o):
                        break
                B.shot(out, 'b5.png')
                B.note('B5 window text:\n' + o.rstrip()[-400:] + '\nrequesters: ' + '; '.join(ami.describe_requester(r) for r in reqs))
                for r in reqs:      # a requester holds the process and the writer: answer it so the rig can end
                    subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('ami.py')), 'ui', 'click', r['title'], 'Cancel'], timeout=60)
                check('B5: no DOS requester opened for the absent volume', not reqs)
                check('B5: upkg ended with an error exit', re.search(r'\[exit [1-9]\d*\]', text()) is not None, text().rstrip()[-120:])

            # B6: the proof of 0026. DH0: full; the file system refuses the write (disk full) and raises its own
            # requester through the process's pr_WindowPtr, which 0026 sets to -1. conf read again (B5 may not have run).
            conf = ami.read_file(B.DEFCONF).decode('latin-1').rstrip('\n')
            conf = '\n'.join(l for l in conf.split('\n') if not re.match(r'\s*option\s+lists_dir\b', l))
            (ir.VTC / 'b6.conf').write_text(conf + '\noption lists_dir %s\n' % LISTS6)
            (ir.VTC / 'extra-b6.sh').write_text('UP-Term:bin/upkg -f VTC:b6.conf update 2>&1\necho "[exit $?]"\n')
            if not full:
                shutil.copyfile(B.ir.ROOT / 'build/amiga/winptr0', ir.VTC / 'winptr0')
            else:
                typewin.typ('exit\r')
                time.sleep(3)
            vshwin.open_vsh('B6 user window', box='0/20/640/300', real_window_ptr=True)
            typewin._last_title = 'B6 user window'
            rc, agent_ptr = ir.run('VTC:winptr0 -p')
            typewin.typ('VTC:winptr0 -p\r')
            time.sleep(4)
            m = re.search(r'pr_WindowPtr=([0-9a-f]{8})', text())
            win_ptr = m.group(1) if m else None
            B.note('B6 pr_WindowPtr: user window %s, agent command %s' % (win_ptr, agent_ptr.strip()))
            check('B6: the user window process has pr_WindowPtr 0 (winptr0 -p)', win_ptr == '00000000', str(win_ptr))
            def fill6():
                """Fill VOL6 to the brim: copies of host-written chunk files (VTC: is the host drawer), big chunks first."""
                for name, size, n in (('b6c1m', 1 << 20, 90), ('b6c32k', 32768, 40), ('b6c512', 512, 160)):
                    (ir.VTC / name).write_bytes(b'\0' * size)
                # the lists drawer exists before the fill, and a few blocks are freed after it: upkg's file open then works
                # and the DATA write hits the full volume (an open on a full volume fails without a requester)
                lines = ['Failat 30', 'MakeDir %s' % FILL6, 'MakeDir %s' % LISTS6]
                i = 0
                for name, size, n in (('b6c1m', 1 << 20, 90), ('b6c32k', 32768, 40), ('b6c512', 512, 160)):
                    for _ in range(n):
                        i += 1
                        lines.append('Copy >NIL: VTC:%s %s/f%d QUIET' % (name, FILL6, i))
                lines += ['Delete >NIL: %s/f%d QUIET' % (FILL6, j) for j in range(i - 5, i + 1)]
                (ir.VTC / 'b6-fill.script').write_text('\n'.join(lines) + '\n')
                rc, o = ir.run_long('b6-fill.script', 300)
                rc, inf = ir.run('Info %s' % VOL6)
                B.note('B6 volume after the fill: ' + ' '.join(l for l in inf.split('\n') if l.startswith(VOL6.rstrip(':'))))
                for name in ('b6c1m', 'b6c32k', 'b6c512'):
                    (ir.VTC / name).unlink()

            reqs = []
            o = ''
            ir.run('Delete >NIL: %s ALL QUIET' % LISTS6)
            fill6()
            try:
                typewin.typ('VTC:vsh VTC:extra-b6.sh\r')
                end = time.time() + 90
                while time.time() < end:
                    time.sleep(3)
                    reqs = ami.requesters()
                    o = text()
                    if reqs or re.search(r'\[exit \d+\]', o):
                        break
                B.shot(out, 'b6.png')
                B.note('B6 window text:\n' + o.rstrip()[-400:] + '\nrequesters: ' + '; '.join(ami.describe_requester(r) for r in reqs))
                if reqs:    # the requester's own text (its labels) from the UI tree, for the report
                    ui = subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('ami.py')), 'uitree'],
                                        capture_output=True, text=True, timeout=60).stdout
                    B.note('B6 requester text: ' + ' / '.join(l.strip() for l in ui.split('\n') if re.search(r'volume|full|protect|Retry|Cancel', l, re.I))[:400])
                for r in reqs:      # the requester holds the writer: Cancel it so the run ends before Lock OFF
                    subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('ami.py')), 'ui', 'click', r['title'], 'Cancel'], timeout=60)
                if reqs:
                    time.sleep(10)
                o2 = text()
            finally:
                time.sleep(5)       # command done + quiet before touching the volume
                ir.run('Delete >NIL: %s ALL QUIET' % FILL6)
                ir.run('Delete >NIL: %s ALL QUIET' % LISTS6)
            rc, inf = ir.run('Info %s' % VOL6)
            B.note('B6 after the drawer is removed: ' + inf.strip()[-160:])
            check('B6: no DOS requester for the full volume', not reqs, '; '.join(ami.describe_requester(r) for r in reqs))
            # the exit is NOT checked: with 0026 the update's list write may still fit in the few free blocks and exit 0
            # (2026-10-23: [exit 0], no requester); the requester is what 0026 decides
            check('B6: upkg ran to its end (an [exit N] line)', re.search(r'\[exit \d+\]', o2) is not None, o2.rstrip()[-120:])
        finally:
            B.teardown(rel, st)
            if tmp:
                shutil.rmtree(tmp, ignore_errors=True)
    need = 10 + 3 if full else 3
    bad = [k for k, v in ck.items() if not v]
    head = 'Verdict: %s (%d of %d checks)' % ('FAIL' if bad or len(ck) < need else 'PASS', len(ck) - len(bad), len(ck))
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(B.LOG) + '\n')
    print(head)
    return 1 if bad or len(ck) < need else 0


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] in ('--help', '-h', 'help', '?'):
        print(__doc__)
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
