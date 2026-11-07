#!/usr/bin/env python3
"""amarchy_dopus_rig.py -- B8b: the Amarchy theme reaches a Directory Opus 5.104 own screen.

DOpus runs on its own screen (`rig.py dopus --own-screen`). Steps: Amarchy + amarchy-flat run; `dopus screen` / `dopus query
palette` give the baseline; the OLD command sequence (set palette, set pens, refresh lister) is sent by hand and the live
palette is read (it must NOT move: the old build); then the real entry point, the theme pick (menu Style > Amarchy theme ->
desk_dos_apply / gt_dos_apply_saved -> dopus_theme_send), runs with the fixed build and the live palette must carry the theme.
A new lister is opened and amarchy-flat STATUS is read for the bevel half (measure only).
Usage: UPTERM_RIG=94 python3 tools/rig/amarchy_dopus_rig.py   (needs build/amiga/Amarchy, amarchy-flat)"""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_gui_rig as g
import amarchy_desk_rig as d

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
B = os.path.join(ROOT, 'build', 'amiga')


def rx(s):
    return c.run('SYS:Rexxc/rx "OPTIONS RESULTS; ADDRESS DOPUS.1; %s; SAY RESULT"' % s)[1].strip()


def pal4():
    return rx('dopus query palette').split()[:4]


def status():
    out = c.run('RAM:amarchy-flat STATUS')[1]
    keep = [l for l in out.splitlines() if re.match(r'(dopus=|flat=|frame=|pens |flat )', l) or 'dopus=' in l or 'calls=' in l]
    return ' | '.join(keep)[:400]


def lister():
    rc, out = c.run('SYS:Rexxc/rx "ADDRESS DOPUS.1; command DeviceList NEW"')
    time.sleep(4)
    return status()


def main():
    for n in ('amarchy-flat',):
        ami.put(os.path.join(B, n), 'RAM:' + n)
        c.run('Protect RAM:%s +e' % n)
    c.run('MakeDir >NIL: ENV:Amarchy')
    for m in re.finditer(r'Process\s+(\d+):.*RAM:Amarchy\b', c.run('Status')[1]):
        c.run('Break %s C' % m.group(1))     # a leftover RAM: copy has no sibling menu programs
        time.sleep(3)
    if not g.process_running('Amarchy'):
        c.run('Run >NIL: VTC:Amarchy')        # the kit drawer: Amarchy starts its menu/gui siblings from there
        time.sleep(8)
    print('process Amarchy running:', g.process_running('Amarchy'))
    d.flat_stop()
    d.put_flat_prefs(True)
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(3)
    print('screen(baseline):', rx('dopus screen'))
    print('palette4(baseline):', pal4())
    print('status(baseline):', status())
    print('status(after lister 1):', lister())
    # OLD sequence by hand: what the build before the fix sent (palette + pens + refresh lister)
    for l in ('dopus set palette 0x1a1b26 0xa9b1d6 0x414868 0x7aa2f7', 'dopus set pens dirs 3 0', 'dopus refresh lister'):
        c.run('SYS:Rexxc/rx "ADDRESS DOPUS.1; %s"' % l)
    time.sleep(3)
    print('palette4(OLD sequence sent, refresh lister):', pal4())
    print('screen(OLD):', rx('dopus screen'))
    # NEW build, real entry point
    g.close_menu()
    name, up = d.pick()
    time.sleep(4)
    print('picked theme:', name)
    print('port DOPUS.1 alive:', rx("SAY SHOW('P','DOPUS.1')"))
    print('screen(after pick):', rx('dopus screen'))
    print('palette4(after pick, NEW):', pal4())
    print('status(after pick):', status())
    print('status(after lister 2):', lister())
    print('status(after lister 3):', lister())


if __name__ == '__main__':
    main()
