import sys, time, os
sys.path.insert(0, 'tools/rig')
sys.argv = ['x']
import amarchy_desk_rig as d
c, ami = d.c, d.ami
R = d.ROOT
c.run('Stack 262144')
c.run('Delete ENV:Sys/wbpattern.prefs ENVARC:Sys/wbpattern.prefs QUIET')
for n in ['Amarchy', 'amarchy-menu', 'amarchy-gui', 'amarchy-flat', 'titleprobe']:
    ami.put(os.path.join(R, 'build', 'amiga', n), 'RAM:' + n); c.run('Protect RAM:%s +e' % n)
c.run('MakeDir >NIL: ENV:Amarchy')
print('stop', d.flat_stop())
d.put_flat_prefs(False)
c.run('Run >NIL: RAM:amarchy-flat'); time.sleep(3)
print('before', d.flat_pens()[3])
d.put_flat_prefs(True)
c.run('Run >NIL: VTCX:Apps/bin/Amarchy'); time.sleep(6)
print(c.run('Status')[1][:600])
try:
    d.pick()
except Exception as e:
    print('PICKFAIL', e)
print('after', d.flat_pens()[3])
rc, out = c.run('RAM:titleprobe', 60)
print('PROBE rc', rc); print(out)
print('final', d.flat_pens()[3])
