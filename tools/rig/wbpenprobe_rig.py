"""usage: drive.py <tag> [apply]  -- probe SYS: window pens on rig $UPTERM_RIG; out to scratchpad/ic/<tag>.txt"""
import os, sys, time
WT = '/Users/spot/Code/UP-Term/vtcon/.claude/worktrees/s28-icon-colours'
sys.path.insert(0, WT + '/tools/rig')
import ami, condev_rig as c
OUT = os.path.dirname(os.path.abspath(__file__))
tag = sys.argv[1]
if 'apply' in sys.argv:
    c.run('MakeDir >NIL: VTCX:Apps/Amarchy'); c.run('MakeDir >NIL: VTCX:Apps/Amarchy/Icons')
    c.run('Assign UP-Term: VTCX:Apps ADD', 20)
    ami.put(WT + '/dist/Amarchy/Icons/lucide.dat', 'UP-Term:Amarchy/Icons/lucide.dat')
    for n in ('amarchy-icons', 'wbpenprobe'):
        ami.put(WT + '/build/amiga/' + n, 'RAM:' + n); c.run('Protect RAM:%s +e' % n)
    print(c.run('RAM:amarchy-icons APPLY', 300)[1][-300:])
rx = ("address WORKBENCH; WINDOW 'SYS:' CLOSE; WINDOW 'SYS:' OPEN; "
      "GETATTR WINDOW.LEFT NAME 'SYS:' VAR l; GETATTR WINDOW.TOP NAME 'SYS:' VAR t; "
      "GETATTR WINDOW.WIDTH NAME 'SYS:' VAR w; GETATTR WINDOW.HEIGHT NAME 'SYS:' VAR h; say l t w h")
open(OUT + '/q.rexx', 'w').write('/* q */\n' + rx.replace('; ', '\n') + '\n')
ami.put(OUT + '/q.rexx', 'RAM:q.rexx')
out = c.run('rx RAM:q.rexx', 30)[1]
print('[INFO] rx:', out.strip())
time.sleep(3)
l, t, w, h = [int(x) for x in out.split()[-4:]]
c.run('RAM:wbpenprobe %d %d %d %d >RAM:p.txt' % (l, t, min(w, 320), min(h, 120)), 60)
data = ami.read_file('RAM:p.txt')
open('%s/%s.txt' % (OUT, tag), 'wb').write(data or b'')
print('[OK] %s %d bytes' % (tag, len(data or b'')))
