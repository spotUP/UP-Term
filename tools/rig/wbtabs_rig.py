"""wbtabs_rig.py -- tab count and tab function across a Workbench screen reset (WB-RESET-SCREENNOTIFY, s40).

UP-Term with exactly one extra tab (Left Amiga T), `/state`-style counters read through C:UPTerm state (tabstate.py,
the key `tabs`), then `SYS:Prefs/ScreenMode FROM VTC:screenmode.aga16.prefs USE`, the count again, and a marker
command typed in each tab (RAM:mark1, RAM:mark2 written by the tab's own shell). UPTERM_RIG=<n> on the rig."""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c, tabstate

RET, RCMD = 0x44, 0x0080
bad = []


def check(ok, what, seen=''):
    print('[%s] %s %s' % ('OK' if ok else 'FAIL', what, seen))
    if not ok:
        bad.append(what)


def typed(cmd, wait=3):
    ami.req(0x08, bytes([4]) + cmd.encode())
    time.sleep(0.5)
    ami.key(RET)
    time.sleep(wait)


def tabs():
    return tabstate.counters().get('tabs')


def marker(tab, name):
    c.run('Delete RAM:%s QUIET' % name)
    ami.key(0x01 + tab, RCMD)  # Right Amiga 1 / 2
    time.sleep(1.5)
    typed('Echo mark-%s >RAM:%s' % (name, name))
    out = c.run('Type RAM:%s' % name)[1]
    check('mark-%s' % name in out, 'tab %d runs a command after the reset' % (tab + 1), out.strip())


def main():
    c.run('Delete RAM:cnt.out QUIET')
    import subprocess, os
    subprocess.run([sys.executable, __file__.rsplit('/', 1)[0] + '/ami.py', 'open'], env=os.environ)
    time.sleep(6)
    ami.key(0x14, 0x0040)  # Left Amiga T
    time.sleep(4)
    before = tabs()
    check(before == 2, 'two tabs before the reset', before)
    c.run('SYS:Prefs/ScreenMode FROM VTC:screenmode.aga16.prefs USE', timeout=90)
    time.sleep(12)
    ami.click_px(300, 5)  # the reopened window is behind the Workbench window (only its top strip shows) and not active
    time.sleep(1)
    after = tabs()
    check(after == 2, 'two tabs after the reset', after)
    marker(0, 'm1')
    marker(1, 'm2')
    print('[%s] wbtabs' % ('FAIL' if bad else 'OK'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
