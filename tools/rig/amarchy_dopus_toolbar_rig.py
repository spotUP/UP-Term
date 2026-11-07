#!/usr/bin/env python3
"""amarchy_dopus_toolbar_rig.py -- B7: the theme apply switches a Directory Opus lister that uses upstream's default
toolbar to the flat bank (DOpus5:Buttons/Amarchy-toolbar) and never touches a lister with a toolbar of its own.

Steps: the generated bank and images (build/dopus-toolbar, `make dopus-toolbar`) go into DOpus5:; Amarchy runs; two
listers open; lister B keeps the default toolbar, lister A gets a copy of it under another name (a custom bank);
`lister query <h> toolbar` is read for both; the real entry point (menu Style > Amarchy theme -> dopus_theme_send)
runs; both are read again: B must report the flat bank, A its custom path. One screenshot of the screen is written to
the path in argv[1] (default build/rig-reports/rig<N>/b7-toolbar.png) for the image reader.
Usage: UPTERM_RIG=94 python3 tools/rig/amarchy_dopus_toolbar_rig.py [shot.png]   (needs build/amiga/Amarchy)"""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_gui_rig as g
import amarchy_desk_rig as d

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TB = os.path.join(ROOT, 'build', 'dopus-toolbar')
CUSTOM = 'DOpus5:Buttons/Custom-bank'


def rx(s):
    return c.run('SYS:Rexxc/rx "OPTIONS RESULTS; ADDRESS DOPUS.1; %s; SAY RESULT"' % s)[1].strip()


def rxn(s):
    """A verb that returns no result (`lister set`): no SAY RESULT."""
    rc, out = c.run('SYS:Rexxc/rx "ADDRESS DOPUS.1; %s"' % s)
    return out.strip()


def handles():
    return rx('lister query all').split()


def toolbar(h):
    return rx('lister query %s toolbar' % h)


def main():
    shot = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'rig-reports', 'b7-toolbar.png')
    os.makedirs(os.path.dirname(shot), exist_ok=True)
    c.run('MakeDir >NIL: DOpus5:Images/Amarchy')
    for n in sorted(os.listdir(os.path.join(TB, 'Images'))):
        ami.put(os.path.join(TB, 'Images', n), 'DOpus5:Images/Amarchy/' + n)
    ami.put(os.path.join(TB, 'Buttons', 'Amarchy-toolbar'), 'DOpus5:Buttons/Amarchy-toolbar')
    print('installed images:', len(os.listdir(os.path.join(TB, 'Images'))), '| bank on rig:', c.run('List DOpus5:Buttons/Amarchy-toolbar')[1].strip()[:80])
    c.run('Copy DOpus5:Buttons/Toolbar %s' % CUSTOM)
    for m in re.finditer(r'Process\s+(\d+):.*RAM:Amarchy\b', c.run('Status')[1]):
        c.run('Break %s C' % m.group(1))
        time.sleep(3)
    if not g.process_running('Amarchy'):
        c.run('Run >NIL: VTC:Amarchy')
        time.sleep(8)
    print('process Amarchy running:', g.process_running('Amarchy'))
    print('dopus screen:', rx('dopus screen'))
    before = set(handles())
    for _ in range(2):
        c.run('SYS:Rexxc/rx "ADDRESS DOPUS.1; command DeviceList NEW"')
        time.sleep(4)
    new = [h for h in handles() if h not in before] or handles()[-2:]
    a, b = new[0], new[-1]
    print('handles:', handles(), '| A =', a, 'B =', b)
    print('toolbar A,B (upstream default):', toolbar(a), '|', toolbar(b))
    print('set A custom:', rxn("lister set %s toolbar '%s'" % (a, CUSTOM)) or 'ok')
    time.sleep(2)
    print('toolbar A,B (before apply):', toolbar(a), '|', toolbar(b))
    g.close_menu()
    name, up = d.pick()
    time.sleep(5)
    print('picked theme:', name)
    print('toolbar A (custom, after apply):', toolbar(a))
    print('toolbar B (default, after apply):', toolbar(b))
    time.sleep(2)
    print('dopus front:', rxn('dopus front') or 'ok')
    time.sleep(3)
    ami.main(['shot', shot])
    print('shot:', shot)


if __name__ == '__main__':
    main()
