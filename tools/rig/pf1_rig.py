#!/usr/bin/env python3
"""pf1_rig.py -- ledger W1/W2 (SB1 scroll bar, PF1 settings) on the rig.

  1. A window on profile "bsp" (backspace = bs): Settings > Backspace key
     sends ticks "Backspace" and the Backspace key reads 08 (rawprobe).
  2. /profile plain (a profile without the key): "Delete" is ticked and the
     key reads 7f -- the profile switch resets backspace_bs (d24d0a6).
  3. "Programs may": exactly one item ticked, following /program-clipboard.
  4. The scroll bar: after enough output for scrollback the window has a
     proportional gadget in its right border; /scrollbar hide removes it,
     /scrollbar show brings it back.

Run with the rig up and the handler installed (rig.py install + reboot):
  python3 tools/rig/pf1_rig.py [--dump]
"""
import os, shlex, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

HERE = os.path.dirname(os.path.abspath(__file__))
TITLE = 'pf1'
BACKSPACE = 0x41
fails = []


def check(name, ok, detail=''):
    print('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, detail if not ok else ''), flush=True)
    if not ok:
        fails.append(name)


def run(cmd, timeout=30):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return b[4:].decode('latin-1')


def put(path, data):
    tmp = os.path.join(HERE, '_pf1rig_tmp')
    open(tmp, 'wb').write(data)
    ami.main(['put', tmp, path])
    os.unlink(tmp)


def get(path):
    tmp = os.path.join(HERE, '_pf1rig_get')
    if os.path.exists(tmp):
        os.unlink(tmp)
    ami.main(['get', path, tmp])
    data = open(tmp, 'rb').read() if os.path.exists(tmp) else b''
    if os.path.exists(tmp):
        os.unlink(tmp)
    return data


def typeline(s, wait=1.5):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(wait)


def submenu(menu, item):
    """[(name, checked)] of the active window's menu > item > sub-items"""
    titles, items, out = {}, {}, []
    for l in ami.req(0x0F).decode('latin-1').splitlines():
        f = shlex.split(l)
        if f[0] == 'M':
            titles[f[1]] = f[-1]
        elif f[0] == 'I' and f[3] == '-' and titles.get(f[1]) == menu and f[-1] == item:
            items[(f[1], f[2])] = 1
        elif f[0] == 'I' and f[3] != '-' and (f[1], f[2]) in items:
            out.append((f[-1], 'checked' in f[5].split(',')))
    return out


def ticked(menu, item):
    return [n for n, c in submenu(menu, item) if c]


def key_bytes(tag):
    """what the Backspace key sends in the window, as rawprobe reads it"""
    out = 'RAM:pf1_%s.txt' % tag
    typeline('VTC:rawprobe * "" 4 >%s' % out, wait=1.5)
    ami.key(BACKSPACE)
    time.sleep(6)
    return get(out).decode('latin-1').strip()


def gadgets():
    """the window's gadget lines from UITREE"""
    out, mine = [], False
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        if l.startswith('W '):
            mine = shlex.split(l)[-1] == TITLE
        elif mine and l.startswith('G '):
            out.append(l)
    return out


def has_bar():
    w = ami.window(TITLE)
    if not w:
        return False
    x, y, ww, h = w['box']
    for l in gadgets():
        f = shlex.split(l)
        gx, gy = int(f[2]), int(f[3])
        gw, gh = map(int, f[4].split('x'))
        if f[5] == 'custom' and gx >= x + ww - 24 and gh > h // 3:
            return True
    return False


def main():
    run('MakeDir >NIL: ENV:up-term')
    put('ENV:up-term/up-term',
        b'[profile bsp]\nbackspace = bs\nscrollback = 200\n\n'
        b'[profile plain]\nscrollback = 200\n')
    run('run >NIL: newshell "XCON:0/12/640/200/%s/CLOSE/PROFILE bsp"' % TITLE)
    for _ in range(5):
        time.sleep(2)
        if ami.window(TITLE):
            break
    else:
        raise SystemExit('window never appeared')
    if '--dump' in sys.argv:
        print('\n'.join(gadgets()))
        print(submenu('Settings', 'Backspace key sends'))
        return

    check('bsp profile: Backspace ticked', ticked('Settings', 'Backspace key sends') == ['Backspace'],
          str(submenu('Settings', 'Backspace key sends')))
    b = key_bytes('bs')
    check('bsp profile: the key sends 08', b == '08', repr(b))

    typeline('/profile plain')
    check('plain profile: Delete ticked', ticked('Settings', 'Backspace key sends') == ['Delete'],
          str(submenu('Settings', 'Backspace key sends')))
    b = key_bytes('del')
    check('plain profile: the key sends 7f', b == '7f', repr(b))

    want = {'write': 'Set the clipboard', 'read-write': 'Set and read the clipboard',
            'off': 'Not use the clipboard'}
    check('Programs may: default is Set the clipboard',
          ticked('Settings', 'Programs may') == [want['write']], str(submenu('Settings', 'Programs may')))
    for v in ('read-write', 'off', 'write'):
        typeline('/program-clipboard %s' % v)
        check('Programs may follows /program-clipboard %s' % v,
              ticked('Settings', 'Programs may') == [want[v]], str(submenu('Settings', 'Programs may')))

    typeline('Type VTC:big.txt', wait=8)
    check('scroll bar shown with scrollback', has_bar(), '\n'.join(gadgets()))
    typeline('/scrollbar hide')
    check('/scrollbar hide removes it', not has_bar())
    typeline('/scrollbar show')
    check('/scrollbar show brings it back', has_bar())
    ami.main(['shot', os.path.join(HERE, '../../build/rig/pf1_rig.png')])

    print('%d failed' % len(fails), flush=True)
    # close by the gadget: on a 2 MB rig a typed EndShell can find no memory
    # for the command (the agent's command processes take 256 KB stacks)
    ami.main(['gclick', TITLE, 'sys:close'])
    time.sleep(3)
    run('Delete >NIL: ENV:up-term/up-term RAM:pf1_#?.txt QUIET')
    sys.exit(1 if fails else 0)


main()
