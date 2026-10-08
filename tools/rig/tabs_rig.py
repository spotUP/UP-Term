"""T1 on the rig: tabs in one UP-Term window (plan
thoughts/shared/plans/2026-10-03-tabs.md, TB6).

  1. Right Amiga T in a window opens a second tab: the bar shows, the window
     carries the new tab's title, the tab runs a shell of its own (a cd in one
     tab does not move the other).
  2. Switching: Right Amiga 1 / 2 and a click on the bar.
  3. The new tab takes the active tab's profile (its Profile menu), and the
     profile file changing moves the tabs on that profile live (L1), while a
     change to another profile moves neither.
  4. The close gadget closes the active tab only: the bar goes, the host stays.
  5. The host's own shell ending leaves the window to the other tab; that
     tab's shell ending closes the window.

The profile lives in ENV:up-term/up-term only (ENVARC: is never touched); the
file there before is put back at the end. Run with the rig up and the handler
installed (rig.py install + a reboot):
  python3 tools/rig/tabs_rig.py
"""
import pathlib, struct, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

VTC = pathlib.Path(__file__).resolve().paths.RIG / 'vtc'
RET, RCMD = 0x44, 0x0080
# the window, its bar and two points inside it (screen pixels, topaz 8 on
# the rig's Workbench): the bar is one font row under the title bar
WX, WY, WW, WH = 0, 20, 640, 300
BAR_Y = WY + 16
TAB_X = (WW // 4, 3 * WW // 4)
EMPTY = (WW // 2, WY + WH - 40)
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def t(s):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)
    ami.key(RET)
    time.sleep(1.5)


def ours():
    """our window's line in UITREE: (title, box) or None"""
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        if l.startswith('W ') and ' %d %d %dx%d ' % (WX, WY, WW, WH) in l:
            return l.split('"')[-2]
    return None


def until(cond, secs=10.0):
    end = time.time() + secs
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.5)
    return cond()


def pixel(x, y):
    b = ami.req(0x07)
    w = struct.unpack('>H', b[2:4])[0]
    o = 8 + (y * w + x) * 3
    return tuple(b[o:o + 3])


def near(p, rgb):
    # the screen's pens are the nearest the palette has (0000AA comes out
    # 0044DD on the rig): the colour's own channel leads, the others trail
    k = max(range(3), key=lambda i: rgb[i])
    return all(p[k] > p[i] + 0x40 for i in range(3) if i != k)


def click(x, y):
    kx, ky = ami.pointer_scale()
    ami.script(('move', int(x * kx), int(y * ky)), ('wait', 2), ('button', 0, 1), ('wait', 2),
               ('button', 0, 0), ('wait', 5))
    time.sleep(1.5)


def profile_checked():
    for l in ami.req(0x0F).decode('latin-1').splitlines():
        f = l.split()
        if l.startswith('I 1 13 ') and len(f) > 4 and f[3] != '-' and 'checked' in f[5]:
            return l.split('"')[-2]
    return None


def cwd_of_active(n, vsh=True):
    """the active tab's shell writes its directory to RAM:tabN.out: the
    host runs the AmigaDOS Shell (Cd), a new tab vsh (pwd)"""
    c.run('Delete RAM:tab%d.out QUIET' % n)
    t(('pwd >RAM:tab%d.out' if vsh else 'Cd >RAM:tab%d.out') % n)
    rc, out = c.run('Type RAM:tab%d.out' % n)
    return out if rc == 0 else ''


PROFILES = '''[profile default]
bg = 000000
[profile blue]
bg = %s
'''


def main():
    had = c.run('Copy ENV:up-term/up-term RAM:tabs-rig.bak QUIET')[0] == 0
    (VTC / 'tabs-rig.conf').write_text(PROFILES % '0000AA')
    c.run('MakeDir ENV:up-term QUIET')
    c.run('Copy VTC:tabs-rig.conf ENV:up-term/up-term QUIET')
    c.run('Copy VTC:vsh C:vsh CLONE QUIET')
    try:
        c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/tabs/CLOSE/PROFILE blue"' % (WX, WY, WW, WH))
        check(until(lambda: ours() == 'tabs'), 'the window opens', ours())
        click(*EMPTY)  # active, for the keys
        check(near(pixel(*EMPTY), (0, 0, 0xAA)), 'the window draws on its profile (blue)', pixel(*EMPTY))
        bare = pixel(TAB_X[0], BAR_Y)
        t('Cd RAM:')

        # 1. a second tab
        ami.key(0x14, RCMD)
        check(until(lambda: ours() == 'UP-Term'), 'Right Amiga T: the new tab is active, its title on the window', ours())
        time.sleep(3)
        check(pixel(TAB_X[0], BAR_Y) != bare, 'the tab bar is drawn', (bare, pixel(TAB_X[0], BAR_Y)))
        cwd2 = cwd_of_active(2)
        check(cwd2 and 'Ram' not in cwd2, 'the new tab runs a shell of its own (the host\'s Cd RAM: is not its)', cwd2)
        check(profile_checked() == 'blue', 'the new tab takes the host\'s profile', profile_checked())

        # 2. switching
        ami.key(0x01, RCMD)
        check(until(lambda: ours() == 'tabs'), 'Right Amiga 1: the host tab', ours())
        cwd1 = cwd_of_active(1, vsh=False)
        check('Ram' in cwd1, 'typing in tab 1 reaches tab 1\'s shell', cwd1)
        click(TAB_X[1], BAR_Y)
        check(until(lambda: ours() == 'UP-Term'), 'a click on the bar: tab 2', ours())
        click(TAB_X[0], BAR_Y)
        check(until(lambda: ours() == 'tabs'), 'a click on the bar: tab 1', ours())
        ami.key(0x02, RCMD)
        check(until(lambda: ours() == 'UP-Term'), 'Right Amiga 2: tab 2', ours())

        # 3. L1 per profile: the other profile changing moves nothing, ours does
        (VTC / 'tabs-rig.conf').write_text(PROFILES.replace('bg = 000000', 'bg = AA0000') % '0000AA')
        c.run('Copy VTC:tabs-rig.conf ENV:up-term/up-term QUIET')
        time.sleep(6)
        check(near(pixel(*EMPTY), (0, 0, 0xAA)), 'the default profile changing leaves the tab (blue)', pixel(*EMPTY))
        (VTC / 'tabs-rig.conf').write_text(PROFILES % '00AA00')
        c.run('Copy VTC:tabs-rig.conf ENV:up-term/up-term QUIET')
        check(until(lambda: near(pixel(*EMPTY), (0, 0xAA, 0))), 'its profile changing moves the active tab live',
              pixel(*EMPTY))
        ami.key(0x01, RCMD)
        time.sleep(2)
        check(near(pixel(*EMPTY), (0, 0xAA, 0)), 'and the hidden tab on it as well', pixel(*EMPTY))

        # 4. the close gadget closes the active tab only
        ami.key(0x02, RCMD)
        until(lambda: ours() == 'UP-Term')
        w = ami.window('UP-Term')
        x, y, gw, gh = w['sys:close']
        click(x + gw // 2, y + gh // 2)
        check(until(lambda: ours() == 'tabs'), 'the close gadget: tab 2 goes, the host tab is back', ours())
        time.sleep(2)
        check(pixel(TAB_X[0], BAR_Y) == pixel(*EMPTY), 'one tab: no bar (text background there)',
              (pixel(TAB_X[0], BAR_Y), pixel(*EMPTY)))

        # 5. the host's shell ends first, then the last tab's
        ami.key(0x14, RCMD)
        until(lambda: ours() == 'UP-Term')
        time.sleep(3)
        ami.key(0x01, RCMD)
        until(lambda: ours() == 'tabs')
        t('EndCLI')
        check(until(lambda: ours() == 'UP-Term'), 'the host\'s shell ends: the window stays, on the other tab', ours())
        cwd = cwd_of_active(3)
        check('Ram' not in cwd and cwd.strip() != '', 'and that tab still runs its shell', cwd)
        t('exit')
        check(until(lambda: ours() is None), 'the last tab\'s shell ends: the window closes', ours())
    finally:
        if had:
            c.run('Copy RAM:tabs-rig.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:tabs-rig.bak RAM:tab#?.out C:vsh T:UP-Term-tab QUIET')
        (VTC / 'tabs-rig.conf').unlink(missing_ok=True)
    print('tabs_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
