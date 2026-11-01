"""FP-1 rig: theme pick -> confirm requester. Check 1 Keep leaves Workbench.prefs byte 73 = 1; check 2 timeout restores the pre-apply md5. UPTERM_RIG=<n> python3 tools/rig/amarchy_confirm_rig.py after amarchy_gui_rig.py --setup."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import amarchy_desk_rig as d
import ami, condev_rig as c, amarchy_theme_save_rig as t, amarchy_gui_rig as g

def prefs(flat):
    d.put_flat_prefs(flat)
    # add desktop-theme on
    import tempfile
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(('; Amarchy preferences.\n[profile default]\ndesktop-theme = on\nflat-system-look = %s\nflat-window-gadgets = %s\n' % (('on',)*2 if flat else ('off',)*2)).encode('latin-1'))
    d.put_prefs(f.name); os.unlink(f.name)
def md5(p):
    return c.run('VTCX:Apps/bin/md5sum %s' % p, 30)[1].strip()[:32]
def b73():
    return ami.read_file('ENV:Sys/Workbench.prefs')[73]
def sent(n): return int(t.env(n) or 0)

import threading
seen=[]
auto={'keep':False,'stop':False}
def titles():
    with ami._conn(30) as cn:
        return [(w['title']) for sc in ami._m().read_ui(cn, every_screen=True) for w in sc['windows']]
def watcher():
    while not auto['stop']:
        try:
            ts=titles()
            if any('Amarchy desktop theme' in x for x in ts):
                seen.append(time.time())
                if auto['keep']:
                    with ami._conn(30) as cn: ami._m().ui_click(cn,'Keep','Amarchy desktop theme',nth=1)
                    auto['keep']=False
                    print('[INFO] watcher clicked Keep', flush=True)
        except BaseException as e: pass
        time.sleep(0.5)
threading.Thread(target=watcher,daemon=True).start()
c.run('MakeDir >NIL: ENV:Amarchy')
prefs(False)
auto['keep']=True
d.pick()   # settle baseline (may show requester)
time.sleep(1)
for r in ami.requesters(): print('[INFO] baseline requester', ami.describe_requester(r))
try:
    with ami._conn(30) as cn: ami._m().ui_click(cn, 'Keep', 'Amarchy desktop theme', nth=1)
except BaseException as e: print('[INFO] baseline keep click:', e)
time.sleep(2)
print('[INFO] counters base', [sent(n) for n in ('AMARCHY_CONFIRM_SHOWN','AMARCHY_CONFIRM_KEEPS','AMARCHY_CONFIRM_REVERTS_USER','AMARCHY_CONFIRM_REVERTS_TIMEOUT')])
time.sleep(20); auto['keep']=False
# check 1
prefs(True)
auto['keep']=True; seen.clear()
s0, k0 = sent('AMARCHY_CONFIRM_SHOWN'), sent('AMARCHY_CONFIRM_KEEPS')
d.pick()
time.sleep(1)
time.sleep(3)
d.say(sent('AMARCHY_CONFIRM_SHOWN') == s0 + 1, 'check1: requester shown (sentinel +1)')
d.say(len(seen)>0, 'check1: requester title in windows list (wasabi windows)')
d.say(sent('AMARCHY_CONFIRM_KEEPS') == k0 + 1, 'check1: keeps sentinel +1')
d.say(b73() == 1, 'check1: Workbench.prefs byte 73 == 1 after Keep', str(b73()))
# check 2
auto['keep']=False; seen.clear()
prefs(False)
pre = md5('ENV:Sys/Workbench.prefs'); print('[INFO] pre md5', pre)
sh0, rt0 = sent('AMARCHY_CONFIRM_SHOWN'), sent('AMARCHY_CONFIRM_REVERTS_TIMEOUT')
d.pick()
mid = md5('ENV:Sys/Workbench.prefs'); print('[INFO] applied md5', mid, 'byte73', b73())
time.sleep(20)
post = md5('ENV:Sys/Workbench.prefs'); print('[INFO] post md5', post)
d.say(sent('AMARCHY_CONFIRM_SHOWN') == sh0 + 1, 'check2: requester shown')
d.say(sent('AMARCHY_CONFIRM_REVERTS_TIMEOUT') == rt0 + 1, 'check2: timeout revert sentinel +1')
d.say(post == pre, 'check2: md5 after 15 s equals pre-apply', '%s vs %s' % (post, pre))
print('failed', d.failed)
