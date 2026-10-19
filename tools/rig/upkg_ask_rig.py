#!/usr/bin/env python3
"""upkg_ask_rig.py -- the install question of the Claude add-on (dist/addons/claude/questions) reaches
Files/install.dos through `upkg install Claude` on the rig, both ways (upkg plan P10.6):

  A  upkg-ui on the PATH: upkg-ask shows an inputbox (the only kind this add-on's questions file has: one string); the script clears the default with backspaces, types
     "rigA.lan 2323", Return. ENVARC:Claude/remote must end with that line.
  B  UP-Term:bin/upkg-ui renamed to upkg-ui.off: the plain prompt "... [rigA.lan 2323]: " (the default is the
     value pass A kept); the script types "rigB.lan 2424", Return. ENVARC:Claude/remote must end with that line.

Needs the Claude .ipk built with the question: build/dist/Claude/Files/questions in place, then
`python3 tools/pkgs.py ipk build && python3 tools/pkgs.py feed build` (or make dist-ipk). The rig is up
(`rig.py start --max`, UPTERM_RIG=3 for rig 3). dist/upkg-ask and the upkg-ui binary (upterm-ports pkgs/upkg-ui) are copied
into UP-Term:bin and left there; upkg-ui is renamed back at the end. A leftover UP-Term:bin/dialog is deleted. The feed is served over http from build/ipk.

  upkg_ask_rig.py [--out DIR]     the report (report.md) and the screenshots go to DIR (default build/<rig>/upkg-ask)
"""
import argparse, os, pathlib, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import install_rig as ir
import typewin
import vshwin
import upkg_rig

UPKG_UI = paths.repo("upterm-ports") / "build/sysroot/UP-Term/bin/upkg-ui"
REMOTE = 'ENVARC:Claude/remote'
CONF = upkg_rig.CONF
TITLE = 'upkg-ask'
LOG = []


def note(s):
    print(s)
    LOG.append(s)


def last_line(path):
    t = ami.read_file(path).decode('latin-1') if ir.run('List >NIL: %s' % path)[0] == 0 else None
    return None if t is None else t


def shot(out, name):
    ami.main(['shot', str(out / name)])


def install_claude(rel_wait=60):
    typewin.typ('upkg -f %s --force-reinstall install Claude\r' % CONF)
    time.sleep(rel_wait)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=str(paths.RIG / 'upkg-ask'))
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    verdict = {}

    # files onto the rig
    shutil.copy(ir.ROOT / 'dist/upkg-ask', ir.VTC / 'upkg-ask')
    shutil.copy(UPKG_UI, ir.VTC / 'upkg-ui')
    for c in ('Copy VTC:upkg-ask UP-Term:bin/upkg-ask CLONE', 'Delete >NIL: UP-Term:bin/upkg-ui.off UP-Term:bin/dialog UP-Term:bin/dialog.off QUIET',
              'Copy VTC:upkg-ui UP-Term:bin/upkg-ui CLONE', 'Protect UP-Term:bin/upkg-ui +rwed',
              'Protect UP-Term:bin/upkg-ask +rwed'):
        ir.run(c)
    note('TERM/TERMINFO: ' + ' | '.join(ir.run(c)[1].strip() for c in ('GetEnv TERMINFO', 'GetEnv TERM')))

    rel = upkg_rig.Release()
    rel.set()
    (ir.VTC / 'ask-rig.conf').write_text(upkg_rig.conf_for((ir.ROOT / 'dist/upkg.conf').read_text(), rel.url))
    ir.run('Copy VTC:ask-rig.conf %s' % CONF)
    ir.run('Delete >NIL: %s ALL QUIET' % upkg_rig.STAGE)
    try:
        vshwin.open_vsh(TITLE, box='0/20/640/300')
        typewin._last_title = TITLE
        typewin.typ('echo "TERM=$TERM TERMINFO=$TERMINFO"; echo "LANG=$LANG LC_ALL=$LC_ALL LC_CTYPE=$LC_CTYPE"; command -v upkg-ui dialog vsh upkg-ask\r')
        time.sleep(2)
        typewin.typ('upkg -f %s update\r' % CONF)
        time.sleep(15)
        if upkg_rig.installed_version('Claude'):
            typewin.typ('upkg -f %s remove Claude\r' % CONF)
            time.sleep(20)
        ir.run('Delete >NIL: ENVARC:Claude/remote QUIET')

        # --- pass A: upkg-ui
        typewin.typ('upkg -f %s install Claude\r' % CONF)
        time.sleep(30)
        shot(out, 'passA-ui.png')
        for _ in range(20):
            typewin.key(typewin.BS, wait=0.15)
        typewin.typ('rigA.lan 2323')
        shot(out, 'passA-typed.png')
        typewin.key(typewin.RET)
        time.sleep(40)
        typewin.typ('echo INSTALL-A-DONE\r')
        time.sleep(3)
        note('window text, pass A:\n' + typewin.screen_text('RAM:ask-screen.txt')[-1500:])
        verdict['A'] = last_line(REMOTE)
        note('A ENVARC:Claude/remote = %r' % verdict['A'])
        shot(out, 'passA-after.png')

        # --- pass B: no upkg-ui on the PATH
        ir.run('Rename UP-Term:bin/upkg-ui UP-Term:bin/upkg-ui.off')
        typewin.typ('upkg -f %s remove Claude\r' % CONF)    # --force-reinstall fails in opkg ("Failed to make temp file ... File name too long")
        time.sleep(20)
        note('after remove, %s = %r' % (REMOTE, last_line(REMOTE)))
        typewin.typ('upkg -f %s install Claude\r' % CONF)
        time.sleep(30)
        shot(out, 'passB-prompt.png')
        note('pass B screen at the prompt:\n' + typewin.screen_text('RAM:ask-screen.txt')[-1200:])
        typewin.typ('rigB.lan 2424\r')
        time.sleep(40)
        typewin.typ('echo INSTALL-B-DONE\r')
        time.sleep(3)
        verdict['B'] = last_line(REMOTE)
        note('B ENVARC:Claude/remote = %r' % verdict['B'])
        shot(out, 'passB-after.png')
        try:
            note('window text at the end:\n' + typewin.screen_text('RAM:ask-screen.txt')[-1500:])
        except SystemExit as e:
            note('screen_text: %s' % e)
    finally:
        ir.run('Rename UP-Term:bin/upkg-ui.off UP-Term:bin/upkg-ui')
        ir.run('Delete >NIL: %s QUIET' % CONF)
        rel.close()

    def ok(v, want):
        return v is not None and v.rstrip('\n').split('\n')[-1].strip() == want
    pa, pb = ok(verdict.get('A'), 'rigA.lan 2323'), ok(verdict.get('B'), 'rigB.lan 2424')
    head = 'Verdict: A %s, B %s' % ('PASS' if pa else 'FAIL', 'PASS' if pb else 'FAIL')
    (out / 'report.md').write_text(head + '\n\n' + '\n'.join(LOG) + '\n')
    print(head)
    return 0 if pa and pb else 1


if __name__ == '__main__':
    sys.exit(main())
