#!/usr/bin/env python3
"""winchleak_rig.py -- RIG9-INSTALLER-CRASH (hunt 12): the "vtcon SIGWINCH" input handler of a
host whose shell ended (host_retire) must leave input.device's chain with it. A window is
resized (that adds the handler), gets a second tab, the host's shell ends, then the last tab's
shell ends and the window closes; PASS: the chain (VTC:inhandlers) held the node while the
window lived and holds none afterwards (before the fix the node stayed, inside freed memory,
and the next input event jumped through it).
`make build/amiga/inhandlers` first; the rig up with the current handler installed."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shutil, subprocess
import paths, ami
import condev_rig as c
import tabs_rig as T

ROOT = paths.RIG.parents[1] if hasattr(paths.RIG, 'parents') else None
NAME = 'vtcon SIGWINCH'
WASABI = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../build/wasabi/wasabi')


def menu(path):
    """pick a menu item by name (the default keymap's Right Amiga keys do not reach a fresh rig's window)"""
    n = int(os.environ.get('UPTERM_RIG', '1') or 1)
    subprocess.run([sys.executable, WASABI, '--host', '127.0.0.1', '--port', str(1233 + n), '--key', 'rigwasabi',
                    'ui', 'menu', path], check=True, capture_output=True)


def winch_nodes():
    rc, out = c.run('VTC:inhandlers')
    return rc, [l for l in out.splitlines() if l.strip() == NAME]


def main():
    shutil.copyfile(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../build/amiga/inhandlers'),
                    T.VTC / 'inhandlers')
    T.clamp_to_screen()
    c.run('Copy VTC:vsh C:vsh CLONE QUIET')  # the second tab runs it
    base = winch_nodes()
    T.check(base[0] == 0 and not base[1], 'no SIGWINCH node before the window', base)
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/wl/CLOSE"' % T.ASK)
    T.check(T.until(lambda: T.ours() == 'wl'), 'the window opens', T.ours())
    T.click(*T.EMPTY)
    zoom = ami.window('wl')['sys:zoom']
    kx, ky = ami.pointer_scale()
    for _ in range(2):  # a resize adds the handler
        x, y, w, h = ami.window('wl')['sys:zoom']
        ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
                   ('wait', 2), ('button', 0, 0), ('wait', 5))
        time.sleep(3)
    T.check(len(winch_nodes()[1]) == 1, 'the resize added the handler (the test reaches it)', winch_nodes())
    T.click(*T.EMPTY)
    menu('UP-Term/New tab')
    T.until(lambda: T.ours() == 'UP-Term')
    time.sleep(3)
    menu('UP-Term/Previous tab')  # the host's tab
    T.until(lambda: T.ours() == 'wl')
    T.t('EndCLI')  # host_retire
    T.check(T.until(lambda: T.ours() == 'UP-Term'), 'the host shell ends, the window stays on the other tab', T.ours())
    time.sleep(2)
    T.t('exit')
    T.check(T.until(lambda: T.ours() is None), 'the last tab ends, the window closes', T.ours())
    time.sleep(3)
    rc, nodes = winch_nodes()
    T.check(rc == 0 and not nodes, 'no "%s" node left in input.device\'s chain' % NAME, nodes)
    c.run('Delete C:vsh T:UP-Term-tab QUIET')
    print('winchleak_rig: passed %d of %d' % (T.passed, T.total))
    return 0 if T.passed == T.total else 1


if __name__ == '__main__':
    sys.exit(main())
