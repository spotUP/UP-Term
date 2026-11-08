#!/usr/bin/env python3
"""vshexec_rig.py -- AmigaDOS Execute typed in vsh runs its script, as in a Shell.

C:Execute is a command file, not the Shell's own: vsh loaded it and ran it with
RunCommand in its runner process, and Execute only queues the script as the
Shell's next input. No Shell read that input, so the script never ran (rc 0, no
output) and stayed open: `upkg remove` ran no Uninstall and could not delete its
copy ("object is in use"). 2026-10-18, rig 2.

  1. Execute RAM:vx/a.dos writes RAM:vx/a.out (the script ran).
  2. Afterwards the script deletes (Execute did not leave it open).
  3. A .KEY script takes its argument: Execute RAM:vx/k.dos hello.
  4. The script's return code comes back: Quit 7 gives $? = 7.
  5. A program whose main returns -1 (opkg/upkg does on error) is not "not found": sh_os.run uses a negative
     status for that, so vsh masks the runner's negative rc to 0..255. VTC:rcneg gives $? = 255, no
     "not found" (needs `make build/amiga/rcneg`).
  6. A command's CLI standard input is the shell's own input, as the Shell keeps it: C:Type 47.4 (OS 3.2)
     takes any other as redirected and fails `Type file` with "wrong number of arguments" (A1200,
     2026-10-20; vshrc's banner). VTC:clistdin makes Type's test and returns 5 on the redirected path
     (needs `make build/amiga/clistdin`).

  7. AmigaDOS script mode (VSH-AMIGA-SCRIPT-MODE S7): each item runs one script under C:Execute in the Shell and under
     `VTC:vsh -c "Execute .."`, and compares output and return code (R23 multi-line backtick, R6 `?` with a reply fed,
     R27 Run banner, S7b-2 upper-case CD kept, R2 R17 R24 R3 R9 R13 R14 R16 R19 R11 regressions). After every item the
     open windows and requesters of all screens are listed (DefIcons "Cannot load preferences because" watch).
     `python3 tools/rig/vshexec_rig.py 7` runs only this case; verdicts go to build/rig-reports/<rig>/vshexec-case7.txt.

Run with the rig up: python3 tools/rig/vshexec_rig.py [7]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import pathlib, shutil
import condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def vsh(cmd):
    return c.run('VTC:vsh -c "%s"' % cmd.replace('"', '*"'))


def script(name, text):
    (paths.RIG / 'vtc' / name).write_text(text)
    c.run('Copy VTC:%s RAM:vx/%s QUIET' % (name, name))


SCRIPTS = {
    'q1': 'Quit 1\n', 'q5': 'Quit 5\n', 'q10': 'Quit 10\n', 'data3': 'l1\nl2\nl3\n', 'reply': 'foo\n',
    'r2': 'Echo a\n.KEY x\nEcho "<x>"\n', 'r3': '.KEY w/A\nEcho "got <w>"\n',
    'r6': '.KEY w/A\nEcho "got <w>"\n', 'r9': 'FailAt 5\nExecute RAM:rc/q10\nEcho notreached\n',
    'r11': 'If NOT EXISTS RAM:rc/nonex\nEcho c1\nEndIf\nIf EXISTS RAM:rc/r11\nEcho c2\nEndIf\nIf 3 GT 2\nEcho c3\nEndIf\n'
           'If WARN\nEcho warn\nElse\nEcho nowarn\nEndIf\nIf EXISTS RAM:rc/r11 VAL 1\nEcho c5\nEndIf\n',
    'r13': 'Skip\nEcho after\n', 'r14': 'Skip nolab\nEcho after\n', 'r16': 'FailAt\nEcho x\n',
    'r17b': 'FailAt 30\nExecute RAM:rc/q10\nEcho inner\n',
    'r17': 'FailAt 3\nExecute RAM:rc/r17b\nExecute RAM:rc/q5\nEcho back\n', 'r19': 'Quit abc\nEcho after\n',
    'r23': 'Echo [`Type RAM:rc/data3`]\n', 'r24': '.key a\nEcho "k24 <a>"\n', 'r27': 'Echo s27 ran\n',
    'empty': '', 'r4': '.KEY s/S\nEcho "[<s>]"\n', 'r5': 'Echo "x<y>z"\n',
    'r7': 'Set v=hi\nEcho "q:$v"\nEcho u:$v\n', 'r8': 'Echo hi >RAM:rc/r8.out there\nType RAM:rc/r8.out\n',
    'r10': 'If ABC EQ abc\nEcho eq\nElse\nEcho ne\nEndIf\n', 'r12': 'If 1 EQ 1\nEcho in\n',
    'r15': 'Execute RAM:rc/q1\nSkip end\nEcho skipped\nLab end\nEndSkip\nEcho "rc=$RC"\n',
    'r18b': 'Quit 3\nEcho notreached\n', 'r18': 'Execute RAM:rc/r18b\nEcho "after rc=$RC"\n',
    'x1': 'Execute RAM:rc/q5\nIf 1 EQ 1\nEcho in\nEndIf\nEcho "rc=$RC"\nLab q\nEcho "rc2=$RC"\n',
    'x2': 'Execute RAM:rc/q5\nIf 1 EQ 1\nEcho in\nEndIf\n', 'cdx': 'CD RAM:rc\n',
    'cddrv': 'CD RAM:\nExecute RAM:rc/cdx\nCD\n', 'x3': 'Echo a | Echo b\n',
    'r21': 'Echo "q:`Echo hi`"\nEcho u:`Echo hi`\n', 'r22': 'Echo `Echo hi\nEcho next\n',
    'r26': '.KEY a,b\nEcho "<a>|<b>"\n', 'r26b': 'Echo plain\n', 'marker': 'inram\n',
    'cdup': 'CD RAM:\n', 'cdw': 'Execute RAM:rc/cdup\nType marker\n', 'cdlow': 'cd RAM:\n', 'cdw2': 'Execute RAM:rc/cdlow\nType marker\n',
}
# (id, command run through Execute/vsh, regexp the vsh half must show or None, what the 3.2 Shell shows)
CASE7 = [
    ('R23', 'Execute RAM:rc/r23', '[l1 l2 l3]'),
    ('R6', 'Execute RAM:rc/r6 ? <RAM:rc/reply', 'got foo'),
    ('R27a', 'Run C:Wait 1', None),
    ('R27b', 'Run RAM:rc/r27', None),
    ('S7b-2', 'Execute RAM:rc/cdw', 'inram'),
    ('S7b-2 lower', 'Execute RAM:rc/cdw2', 'inram'),
    ('R2', 'Execute RAM:rc/r2 val', None), ('R17', 'Execute RAM:rc/r17', None), ('R24', 'RAM:rc/r24 foo', None),
    ('R3', 'Execute RAM:rc/r3', None), ('R9', 'Execute RAM:rc/r9', None), ('R13', 'Execute RAM:rc/r13', None),
    ('R14', 'Execute RAM:rc/r14', None), ('R16', 'Execute RAM:rc/r16', None), ('R19', 'Execute RAM:rc/r19', None),
    ('R11', 'Execute RAM:rc/r11', None),
]
# R1 to X3 in the order of the S7a run (script-mode-rchecks.md), for the DefIcons watch: windows listed after each.
REPLAY = [
    ('R1', 'If EXISTS RAM:rc'), ('R2', 'Execute RAM:rc/r2 val'), ('R3', 'Execute RAM:rc/r3'),
    ('R4a', 'Execute RAM:rc/r4 s'), ('R4b', 'Execute RAM:rc/r4'), ('R5', 'Execute RAM:rc/r5'),
    ('R6', 'Execute RAM:rc/r6 ? <NIL:'), ('R7', 'Execute RAM:rc/r7'), ('R8', 'Execute RAM:rc/r8'),
    ('R9', 'Execute RAM:rc/r9'), ('R10', 'Execute RAM:rc/r10'), ('R11', 'Execute RAM:rc/r11'),
    ('R12', 'Execute RAM:rc/r12'), ('R13', 'Execute RAM:rc/r13'), ('R14', 'Execute RAM:rc/r14'),
    ('R15', 'Execute RAM:rc/r15'), ('R16', 'Execute RAM:rc/r16'), ('R17', 'Execute RAM:rc/r17'),
    ('R18', 'Execute RAM:rc/r18'), ('R19', 'Execute RAM:rc/r19'), ('X1', 'Execute RAM:rc/x1'),
    ('X1b', 'Execute RAM:rc/x2'), ('X2', 'Execute RAM:rc/cddrv'), ('X3', 'Execute RAM:rc/x3'),
    ('R21', 'Execute RAM:rc/r21'), ('R22', 'Execute RAM:rc/r22'), ('R23', 'Execute RAM:rc/r23'),
    ('R24', 'RAM:rc/r24 foo'), ('R26', 'RAM:rc/r26 one two'), ('R26b', 'RAM:rc/r26b one two'),
]


def windows():
    """Every window on every screen as 'screen/title[buttons]' plus the requesters among them (text only)."""
    import ami
    with ami._conn(30) as conn:
        screens = ami._m().read_ui(conn, every_screen=True)
    rows = ['%s/%s%s' % (sc['title'], w['title'], [g['label'] for g in w['gadgets'] if g['label']][:6]) for sc in screens
            for w in sc['windows']]
    return rows, [ami.describe_requester(r) for r in ami.requesters_in(screens)]


def case7():
    log = []

    def say(t):
        log.append(t)
        print(t, flush=True)

    shutil.rmtree(paths.RIG / 'vtc/rcs', ignore_errors=True)
    (paths.RIG / 'vtc/rcs').mkdir()
    for n, t in SCRIPTS.items():
        (paths.RIG / 'vtc/rcs' / n).write_text(t)
    c.run('Delete RAM:rc ALL QUIET')
    c.run('MakeDir RAM:rc')
    c.run('Copy VTC:rcs RAM:rc ALL QUIET')
    c.run('Protect RAM:rc/r24 +s')
    c.run('Protect RAM:rc/r27 +s')
    c.run('Copy RAM:rc/marker RAM:marker QUIET')
    base = windows()[0]
    say('windows before: %s' % base)
    for name, cmd, want in CASE7 + [(n, k, None) for n, k in REPLAY]:
        real = c.run(cmd, 20)
        redir = ''
        vcmd = cmd
        if ' <' in cmd:
            vcmd, redir = cmd.split(' <')
            redir = ' <' + redir
        mine = c.run('VTC:vsh -c "%s"%s' % (vcmd.replace('"', '*"'), redir), 20)
        wins, reqs = windows()
        new = [w for w in wins if w not in base]
        mine = (mine[0], ''.join(l for l in mine[1].splitlines(True) if not l.startswith('VTC:vsh failed returncode')))
        same = real == mine
        say('%s %s | cmd %s' % (name, 'MATCH' if same else 'DIFFERS', cmd))
        say('   shell %r' % (real,))
        say('   vsh   %r' % (mine,))
        say('   new windows %s requesters %s' % (new, reqs))
        if want:
            check(want in mine[1], '%s: vsh shows %r' % (name, want), mine)
        base = base + new
    c.run('Delete RAM:rc ALL QUIET')
    c.run('Delete RAM:marker QUIET')
    shutil.rmtree(paths.RIG / 'vtc/rcs', ignore_errors=True)
    out = paths.reports_dir() / 'vshexec-case7.txt'   # kept output: rig.py stop deletes paths.RIG
    out.write_text('\n'.join(log) + '\n')


def main():
    shutil.copyfile(ROOT / 'build/amiga/vsh', paths.RIG / 'vtc/vsh')
    if sys.argv[1:] == ['7']:
        case7()
        print('%d/%d' % (passed, total))
        return 0 if passed == total else 1
    shutil.copyfile(ROOT / 'build/amiga/vsh', paths.RIG / 'vtc/vsh')
    shutil.copyfile(ROOT / 'build/amiga/rcneg', paths.RIG / 'vtc/rcneg')
    shutil.copyfile(ROOT / 'build/amiga/clistdin', paths.RIG / 'vtc/clistdin')
    c.run('Delete RAM:vx ALL QUIET')
    c.run('MakeDir RAM:vx')
    try:
        script('vxa.dos', 'Echo ran >RAM:vx/a.out\n')
        rc, out = vsh('Execute RAM:vx/vxa.dos')
        got = c.run('Type RAM:vx/a.out')[1]
        check('ran' in got, 'Execute runs its script (RAM:vx/a.out written)', (rc, out, got))
        rc, out = c.run('Delete RAM:vx/vxa.dos')
        check(rc == 0, 'the script deletes afterwards (not left open)', (rc, out))
        script('vxk.dos', '.KEY WORD/A\nEcho "<WORD>" >RAM:vx/k.out\n')
        vsh('Execute RAM:vx/vxk.dos hello')
        got = c.run('Type RAM:vx/k.out')[1]
        check('hello' in got, 'a .KEY script takes its argument', got)
        script('vxq.dos', 'Quit 7\n')
        rc, out = vsh('Execute RAM:vx/vxq.dos; echo rc=$?')
        check('rc=7' in out, "the script's return code comes back (Quit 7)", out)
        rc, out = vsh('VTC:rcneg; echo rc=$?')
        check('not found' not in out and 'rc=255' in out, 'main returning -1 gives $? = 255, not "not found"', out)
        rc, out = vsh('VTC:clistdin; echo rc=$?')
        check('redirected=0' in out and 'rc=0' in out, "the command's CLI standard input is the shell's input", out)
        case7()
    finally:
        c.run('Delete RAM:vx ALL QUIET')
        for n in ('vxa.dos', 'vxk.dos', 'vxq.dos'):
            (paths.RIG / 'vtc' / n).unlink(missing_ok=True)
    print('%d/%d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
