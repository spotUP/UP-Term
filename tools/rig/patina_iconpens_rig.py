#!/usr/bin/env python3
"""s28 F15: do the 16 icon pens (32..47) hold the theme RGB, and what do the "Patina.IconPens.6" owner bits read, in each state:
  1 icons APPLY done, flat not running   2 patina-flat running, switch ON   3 switch OFF, patina-flat still running
  4 patina-flat quit (Ctrl-C)           5 icons RESTORE (expect owners 0, record freed)
  UPTERM_RIG=<n> tools/rig/rig.py start --max --os 32 --purpose "<session>: F15 icon pens"; rig.py aga256
  UPTERM_RIG=<n> python3 tools/rig/patina_iconpens_rig.py [--expect <file of 'PEN n rrggbb' lines>]
Needs build/amiga/{iconpens,patina-icons,patina-flat,flatwin} (make rig-helpers; make patina-icons patina-flat flatwin).
Prints each state's iconpens output and a per-state verdict against --expect when given."""
import os, re, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ami, condev_rig as c

B = os.path.join(HERE, '..', '..', 'build', 'amiga')


def probe(label, exp):
    out = c.run('RAM:iconpens', 60)[1]
    print('--- %s' % label)
    print(out.strip())
    pens = dict((int(m.group(1)), m.group(2)) for m in re.finditer(r'^PEN (\d+) ([0-9a-f]{6})', out, re.M))
    own = re.search(r'owners=(\d+)', out)
    verdict = ''
    if exp is not None and pens:
        bad = [n for n in exp if pens.get(n) != exp[n]]
        verdict = ' pens-match=%s%s' % ('yes' if not bad else 'NO', '' if not bad else ' differing=%s' % bad)
    print('[STATE] %s owners=%s%s' % (label, own.group(1) if own else 'none(record freed)', verdict))


def main():
    exp = None
    if '--expect' in sys.argv:
        exp = dict((int(m.group(1)), m.group(2)) for m in re.finditer(r'PEN (\d+) ([0-9a-f]{6})',
                   open(sys.argv[sys.argv.index('--expect') + 1]).read()))
    c.run('MakeDir >NIL: VTCX:Apps/Patina')
    c.run('MakeDir >NIL: VTCX:Apps/Patina/Icons')
    c.run('Assign UP-Term: VTCX:Apps ADD', 20)
    if ami.read_file('UP-Term:Patina/Icons/lucide.dat') is None:
        ami.put(os.path.join(HERE, '..', '..', 'dist', 'Patina', 'Icons', 'lucide.dat'), 'UP-Term:Patina/Icons/lucide.dat')
    for n in ('iconpens', 'patina-icons', 'patina-flat', 'flatwin'):
        ami.put(os.path.join(B, n), 'RAM:' + n)
        c.run('Protect RAM:%s +e' % n)
    c.run('Stack 262144')
    probe('0 before APPLY', exp)
    print('[INFO] APPLY:', c.run('RAM:patina-icons APPLY', 300)[1].strip()[-200:])
    probe('1 APPLY done, flat not running', exp)
    c.run('Run >NIL: RAM:patina-flat')
    time.sleep(4)
    print('[INFO] flat:', c.run('RAM:patina-flat STATUS')[1].strip())
    c.run('RAM:flatwin 1 SET 1 0 1 2 0', 120)
    time.sleep(2)
    print('[INFO] flat:', c.run('RAM:patina-flat STATUS')[1].strip())
    probe('2 flat running, switch ON', exp)
    c.run('RAM:flatwin 1 SET 0 0 1 2 0', 120)
    time.sleep(2)
    print('[INFO] flat:', c.run('RAM:patina-flat STATUS')[1].strip())
    probe('3 switch OFF, patina-flat still running', exp)
    for m in re.finditer(r'Process\s+(\d+):.*patina-flat', c.run('Status')[1]):
        c.run('Break %s C' % m.group(1))
    time.sleep(4)
    probe('4 patina-flat quit', exp)
    print('[INFO] RESTORE:', c.run('RAM:patina-icons RESTORE', 300)[1].strip()[-200:])
    time.sleep(2)
    probe('5 RESTORE done', exp)


if __name__ == '__main__':
    main()
