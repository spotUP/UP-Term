#!/usr/bin/env python3
"""upkg_install_all_rig.py -- `upkg install --all`, `upkg upgrade --all` and Shift+I / Shift+U in `upkg browse` on a rig
(upterm-ports PR 9, patch 0041). The entry points are typed commands and keys in a vsh window on the rig.

Feed: three fixture packages built here with tools/pkgs.py from a temp table and a temp staged tree:
  up-iaplain1, up-iaplain2   plain; their install.dos appends "<archive> <version> ans=-" to T:ia-log
  up-iaask                   interactive (Files/questions: ANS|string|...|dflt|); install.dos appends "<archive> <version> ans=<ANS>"
Checks (verdicts are recorded in <out>/verdicts.json; a PASS is skipped on the next run, --force reruns all, --only N one):
  1 binaries   upkg and upkg-ui on the rig have the md5 of the build ($UPTERM_PORTS/build/sysroot/UP-Term/bin)
  2 installall `upkg install --all` in a vsh window: the question is answered once by typing (ANS), then no further key;
               installed count = 3 and T:ia-log holds "up-iaask 1.0 ans=<typed>" once
  3 upgrade    up-iaask 1.1 in the feed: `upkg upgrade --all` upgrades 1 (up-iaask 1.1, answer kept in the log);
               up-iaask 1.2: plain `upkg upgrade` upgrades 1, answer kept
  4 browse     both iaplain 1.1 in the feed: Shift+U shows "Update all 2 add-ons?" default No (Return changes nothing), y upgrades;
               all removed: Shift+I shows "Install all 3 add-ons? Total download: X MB.", y installs all 3; footer hints
Needs: the kit of `make dist` (UPTERM_KIT), the ports checkout built (UPTERM_PORTS=<worktree>), UPTERM_RIG=8.
The script stops the rig at the end.

  UPTERM_RIG=8 UPTERM_PORTS=<ports> UPTERM_KIT=<kit> upkg_install_all_rig.py [--out DIR] [--force] [--only N]
"""
import argparse, hashlib, json, os, pathlib, re, shutil, sys, tempfile, time
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

PORTS = paths.ports()
BIN = PORTS / 'build/sysroot/UP-Term/bin'
TITLE = 'upkg-install-all'
LOG = 'T:ia-log'
ANSWER = 'Green42'
OTHER = 'Blue77'   # typed if an UPGRADE asks again: the log then shows it was asked, and the earlier answer lost
P1, P2, ASK = 'up-iaplain1', 'up-iaplain2', 'up-iaask'
ALL = (P1, P2, ASK)
TABLE_HEAD = '# id  archive  version  needs-base  ixemul  record  handed  consumes  launch\n'
QUESTIONS = '# the fixture question\nANS|string|Which colour should the fixture use?|dflt|\n'
UNINSTALL = 'FailAt 21\nDelete >NIL: "UP-Term:pkg/%(a)s" ALL QUIET\nEcho "%(a)s removed."\n'
INSTALL = ('.KEY DIR/A,ANS/K\n.BRA {\n.KET }\nFailAt 21\nCD "{DIR}"\nMakeDir >NIL: UP-Term:pkg UP-Term:pkg/%(a)s\n'
           'Copy payload UP-Term:pkg/%(a)s/payload CLONE QUIET\nCopy Uninstall UP-Term:pkg/%(a)s/Uninstall CLONE QUIET\n'
           'Copy VERSIONS UP-Term:pkg/%(a)s/VERSIONS CLONE QUIET\nCopy MANIFEST UP-Term:pkg/%(a)s/MANIFEST CLONE QUIET\n'
           'Echo "%(a)s %(v)s ans=%(ans)s" >>' + LOG + '\nEcho "%(a)s installed."\n')


def md5(path):
    return hashlib.md5(pathlib.Path(path).read_bytes()).hexdigest()


def make_feed(versions):
    """A temp build drawer with the fixtures staged at versions {archive: version}; returns the feed (ipk) drawer."""
    build = pathlib.Path(tempfile.mkdtemp(prefix='iafeed'))
    rows = []
    for a in ALL:
        i, v = a.replace('up-', ''), versions[a]
        files = build / 'dist' / a / 'Files'
        files.mkdir(parents=True)
        (files / 'payload').write_text('payload of %s\n' % a * 40)
        ans = '{ANS}' if a == ASK else '-'
        (files / 'install.dos').write_text(INSTALL % {'a': a, 'v': v, 'ans': ans})
        (files / 'Uninstall').write_text(UNINSTALL % {'a': a})
        (files / 'VERSIONS').write_text('%s %s fixture\n' % (a, v))
        (files / 'MANIFEST').write_text('pkg/%s\n' % a)
        if a == ASK:
            (files / 'questions').write_text(QUESTIONS)
        pkgs.SUMMARY[i] = 'install-all fixture %s' % a
        rows.append('%s  %s  %s  -  -  UP-Term:pkg/%s  -  vtcon  -\n' % (i, a, v, a))
    table = build / 'packages.txt'
    table.write_text(TABLE_HEAD + ''.join(rows))
    pkgs.ipk(build, table)
    pkgs.write_feed(build, table)
    return build


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-install-all'))
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--only', type=int, action='append')
    a = ap.parse_args(argv)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    vfile = out / 'verdicts.json'
    done = json.loads(vfile.read_text()) if vfile.exists() and not a.force else {}
    note, shot = ubr.note, ubr.shot
    for need in (BIN / 'upkg', BIN / 'upkg-ui'):
        if not need.exists():
            raise SystemExit('upkg_install_all_rig: no %s' % need)
    ck = {}

    def check(name, ok, detail=''):
        ck[name] = bool(ok)
        done[name] = {'ok': bool(ok), 'detail': detail}
        vfile.write_text(json.dumps(done, indent=1))
        note('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail))

    def window(label, want=None):
        for _ in range(3):   # clipboard reads can be stale
            t = typewin.screen_text('RAM:ia-screen.txt')
            if want is None or re.search(want, t):
                break
            time.sleep(2)
        note('window "%s":\n%s' % (label, t.rstrip()[-2500:]))
        return t

    def log_lines():
        rc, o = ir.run('Type %s' % LOG)
        return [l.strip() for l in o.split('\n') if l.strip()] if rc == 0 else []

    def installed():
        return {n: v for n, v in ((l.split(' - ')[0].strip(), l.split(' - ')[1].strip()) for l in
                                  upkg_rig.upkg('ia-li', 'list-installed', conf=False)[1].split('\n') if ' - ' in l) if n in ALL}

    def run_typed(cmd, sentinel, secs, answer=None):
        """Type cmd + a sentinel echo in the vsh window; wait for the sentinel; type `answer` when the question shows."""
        typewin.typ('%s; echo "%s rc=$?"\r' % (cmd, sentinel))
        end, asked, t = time.time() + secs, False, ''
        while time.time() < end:
            time.sleep(6)
            ubr.requester_stop(cmd)
            t = typewin.screen_text('RAM:ia-screen.txt')
            if answer and not asked and re.search(r'Which colou?r', t):
                asked = True
                shot(out, 'q-%s.png' % sentinel)
                note('question on screen:\n' + t.rstrip()[-500:])
                for _ in range(10):
                    typewin.key(typewin.BS, wait=0.15)
                typewin.typ(answer)
                typewin.key(typewin.RET, wait=1)
            if re.search(r'%s rc=\d+' % sentinel, t.replace('echo "%s rc=$?"' % sentinel, '')):
                break
        return t, asked

    def need(n):
        return (a.only and n in a.only) or (not a.only and not (done.get(KEYS[n]) or {}).get('ok'))
    KEYS = {1: 'binaries', 2: 'installall', 3: 'upgrade', 4: 'browse'}
    ck_names = {}

    feed = {x: '1.0' for x in ALL}
    builds = []
    cur = [None]
    orig_set = None

    def serve(rel, versions):
        b = make_feed(versions)
        builds.append(b)
        cur[0] = b / 'ipk'
        orig_set(cur[0])

    with paths.rig_lock():
        rel = upkg_rig.Release()
        orig_set = rel.set
        rel.set = lambda: orig_set(cur[0])   # ubr.setup serves with rel.set(): the fixture feed, not build/ipk
        st = ubr.State()
        ubr.UPKG = BIN / 'upkg'
        try:
            serve(rel, feed)
            ubr.setup(rel, st)
            for n in ('upkg', 'upkg-ui'):
                shutil.copy(BIN / n, ir.VTC / n)
                rc, o = ir.run('Copy VTC:%s UP-Term:bin/%s CLONE' % (n, n))
                ubr.must(rc == 0, 'Copy of %s' % n, o)
                ir.run('Protect UP-Term:bin/%s +rwed' % n)
            shutil.copy(BIN / 'md5sum', ir.VTC / 'md5sum')
            for n in ('upkg', 'upkg-ui'):
                rc, o = ir.run('VTC:md5sum UP-Term:bin/%s' % n)
                for _ in range(2):
                    if md5(BIN / n) in o:
                        break
                    rc, o = ir.run('VTC:md5sum UP-Term:bin/%s' % n)
                check('1 binary %s on the rig is the build (md5)' % n, md5(BIN / n) in o, '%s vs %s' % (md5(BIN / n), o.strip()[:40]))
            ir.run('Delete >NIL: %s QUIET' % LOG)
            for p in ALL:
                if p in installed():
                    upkg_rig.upkg('ia-prerm', 'remove %s' % p, conf=False)
            rc, o = upkg_rig.upkg('ia-update', 'update', conf=False)
            ubr.must(rc == 0, 'upkg update', o)
            vshwin.open_vsh(TITLE, box='0/20/640/300')
            typewin._last_title = TITLE

            # 2: install --all
            if need(2):
                t, asked = run_typed('upkg install --all', 'IA1', 400, ANSWER)
                shot(out, 'c2-after.png')
                note('window after install --all:\n' + t.rstrip()[-900:])
                inst, lg = installed(), log_lines()
                note('installed %r log %r' % (inst, lg))
                check('2 question was shown once and answered by typing', asked)
                check('2 installed count = feed count 3', len(inst) == 3, repr(inst))
                check('2 recorded answer = typed', lg.count('%s 1.0 ans=%s' % (ASK, ANSWER)) == 1, repr(lg))
                check('2 no further question after the answer', len(re.findall(r'Which colou?r', t)) <= 2 and 'IA1 rc=0' in t.replace('"', ''), 'rc line')

            # 3: upgrade --all, then plain upgrade
            if need(3):
                ir.run('Delete >NIL: %s QUIET' % LOG)
                for ver, cmd, tag in (('1.1', 'upkg upgrade --all', 'IA2'), ('1.2', 'upkg upgrade', 'IA3')):
                    serve(rel, dict(feed, **{ASK: ver}))
                    rc, o = upkg_rig.upkg('ia-update', 'update', conf=False)
                    before = installed()
                    t, asked = run_typed(cmd, tag, 300, OTHER)
                    after, lg = installed(), log_lines()
                    note('%s: before %r after %r log %r' % (cmd, before, after, lg))
                    changed = [p for p in ALL if before.get(p) != after.get(p)]
                    check('3 %s upgraded count = 1' % cmd, changed == [ASK] and after.get(ASK) == ver, 'changed %r now %r' % (changed, after.get(ASK)))
                    check('3 %s earlier answer kept (log; asked again: %s)' % (cmd, asked), ('%s %s ans=%s' % (ASK, ver, ANSWER)) in lg, repr(lg))
                    shot(out, 'c3-%s.png' % tag)

            # 4: browse
            if need(4):
                serve(rel, {P1: '1.1', P2: '1.1', ASK: '1.2'})
                upkg_rig.upkg('ia-update', 'update', conf=False)
                typewin.typ('clear; upkg browse\r')
                time.sleep(8)
                ubr.requester_stop('upkg browse')
                t = window('browse list', 'up-ia')
                shot(out, 'c4-list.png')
                check('4 footer hints Install All / Update All', 'Install All' in t and 'Update All' in t, '')
                before = installed()
                typewin.typ('U')
                time.sleep(3)
                t = window('Shift+U dialog', 'pdate all')
                shot(out, 'c4-update-dialog.png')
                m = re.search(r'Update all (\d+) add-on', t)
                check('4 Shift+U asks "Update all 2 add-ons?"', m and m.group(1) == '2', m.group(0) if m else 'no match')
                typewin.key(typewin.RET, wait=3)   # default: No
                time.sleep(3)
                check('4 default is No (Return changes nothing)', installed() == before, repr(installed()))
                typewin.typ('U')
                time.sleep(3)
                typewin.typ('y')
                end = time.time() + 240
                while time.time() < end and installed().get(P1) != '1.1':
                    time.sleep(8)
                    ubr.requester_stop('Update All')
                shot(out, 'c4-after-update.png')
                inst = installed()
                check('4 Yes upgrades the 2 outdated', inst.get(P1) == '1.1' and inst.get(P2) == '1.1', repr(inst))
                window('after Update All')
                typewin.key(typewin.RET, wait=3)   # the result dialog's OK
                typewin.typ('q')
                time.sleep(3)
                typewin.typ('clear; echo BROWSE-OUT\r')
                time.sleep(3)
                note('after q: ' + typewin.screen_text('RAM:ia-screen.txt').rstrip()[-200:])
                for p in ALL:
                    upkg_rig.upkg('ia-prerm', 'remove %s' % p, conf=False)
                ir.run('Delete >NIL: %s QUIET' % LOG)
                typewin.typ('clear; upkg browse\r')
                time.sleep(8)
                typewin.typ('I')
                time.sleep(3)
                t = window('Shift+I dialog', 'nstall all')
                shot(out, 'c4-install-dialog.png')
                m = re.search(r'Install all (\d+) add-on\w*\? Total download: [\d.]+ MB', t)
                check('4 Shift+I asks "Install all 3 add-ons? Total download: X MB."', m and m.group(1) == '3', m.group(0) if m else 'no match')
                typewin.typ('y')
                end, asked = time.time() + 360, False
                while time.time() < end and len(installed()) < 3:
                    time.sleep(8)
                    ubr.requester_stop('Install All')
                    t = typewin.screen_text('RAM:ia-screen.txt')
                    if not asked and re.search(r'Which colou?r', t):
                        asked = True
                        note('browse question:\n' + t.rstrip()[-500:])
                        for _ in range(10):
                            typewin.key(typewin.BS, wait=0.15)
                        typewin.typ(ANSWER)
                        typewin.key(typewin.RET, wait=1)
                shot(out, 'c4-after-install.png')
                inst, lg = installed(), log_lines()
                check('4 Yes installs all 3', len(inst) == 3, repr(inst))
                check('4 browse recorded answer = typed', ('%s 1.2 ans=%s' % (ASK, ANSWER)) in lg, repr(lg) + ' asked=%s' % asked)
                window('after Install All')
                typewin.typ('q')
                time.sleep(3)
        finally:
            for p in ALL:
                try:
                    upkg_rig.upkg('ia-cleanup', 'remove %s' % p, conf=False)
                except BaseException:
                    pass
            ubr.teardown(rel, st)
            for b in builds:
                shutil.rmtree(b, ignore_errors=True)
    bad = [k for k, v in done.items() if not v['ok']]
    head = 'Verdict: %s (%d of %d checks)' % ('FAIL' if bad else 'PASS', len(done) - len(bad), len(done))
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(ubr.LOG) + '\n')
    print(head)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
