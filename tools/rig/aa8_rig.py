#!/usr/bin/env python3
"""AA-8 (plan 2026-11-08-ui-text-aa) on a rig (UPTERM_RIG=<n>, `rig.py start --max`, `rig.py install`, `make patina-flat
patina-gui build/amiga/Patina`): does ui-antialias reach the product? Two passes, ui-antialias on (the default) and off,
each: prefs written, Patina commodity (re)started, patina-flat started. Then a ROM CON: shell window (not Workbench) is
opened and STATUS labels= read before/after, and the drawn menu is opened with a bitmap font (patina-menu-font = topaz:8)
and ENV:PATINA_AA_GLYPHS read. Never Breaks patina-flat: the switch moves through the prefs and Patina's RELOAD."""
import os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import patina_desk_rig as d
import patina_gui_rig as g
import patina_rig, paths

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
fails = 0


def verdict(ok, what):
    global fails
    fails += not ok
    print('[%s] %s' % ('PASS' if ok else 'FAIL', what), flush=True)


def status():
    raw = c.run('RAM:patina-flat STATUS')[1].replace('\n', ' ').strip()
    m = re.search(r'labels=(\d+) lskip=(\d+)', raw)
    return (int(m.group(1)) if m else -1), raw


def prefs(aa):
    c.run('MakeDir >NIL: ENV:Patina')
    c.run('Delete >NIL: ENV:Patina/prefs QUIET')
    for i, l in enumerate(['; Patina preferences.', '[profile default]', 'flat-system-look = on', 'flat-window-gadgets = on',
                           'ui-antialias = %s' % aa, 'patina-menu-font = topaz:8', 'menu-style = graphical',
                           'menu-in-terminal = graphical']):
        c.run('Echo >%s "%s" ' % ('ENV:Patina/prefs', l) if i == 0 else 'Echo >>ENV:Patina/prefs "%s"' % l)
    patina_rig.arexx('RELOAD')
    time.sleep(2)


def shell(title):
    c.run('Echo >RAM:aa8.script "List SYS:C\nList SYS:Devs\nWait 2"')
    c.run('Run >NIL: NewShell "CON:0/60/560/180/%s/CLOSE" FROM RAM:aa8.script' % title)
    time.sleep(4)


def shell_close(title):
    g.close_window(title) if hasattr(g, 'close_window') else None
    time.sleep(1)


def one(aa):
    print('=== ui-antialias = %s' % aa, flush=True)
    prefs(aa)
    if 'flat=absent' in status()[1] or not g.process_running('patina-flat'):
        c.run('Run >NIL: RAM:patina-flat')
        time.sleep(4)
        patina_rig.arexx('RELOAD')            # flat_state.aa comes from the prefs once the block exists
        time.sleep(2)
    before, raw0 = status()
    print('[INFO] STATUS before: %s' % raw0, flush=True)
    shell('AA8 shell %s' % aa)
    mid, raw1 = status()
    print('[INFO] STATUS after open: %s' % raw1, flush=True)
    shell('AA8 second %s' % aa)
    after, raw2 = status()
    print('[INFO] STATUS after second window: %s' % raw2, flush=True)
    c.run('Delete >NIL: ENV:PATINA_AA_GLYPHS ENVARC:PATINA_AA_GLYPHS QUIET')
    ami.click_px(100, 100)
    time.sleep(0.5)
    s, w = g.open_menu()
    time.sleep(2)
    glyphs = c.run('GetEnv PATINA_AA_GLYPHS')[1].strip()
    print('[INFO] menu window %s, PATINA_AA_GLYPHS=%r' % (bool(w), glyphs), flush=True)
    g.close_menu()
    return before, mid, after, glyphs


def main():
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'patina-flat'), 'RAM:patina-flat')
    c.run('Protect RAM:patina-flat +e')
    c.run('Path SYS:Rexxc ADD')
    if not patina_rig.port_up():
        c.run('Run >NIL: %s/Patina' % paths.UPTERM_BIN)
        time.sleep(4)
    b, m, a, gl = one('on')
    verdict(a - b > 0, 'on: shell window blends text: labels %d -> %d -> %d' % (b, m, a))
    verdict(gl.isdigit() and int(gl) > 0, 'on: PATINA_AA_GLYPHS=%s (bitmap topaz 8)' % gl)
    b2, m2, a2, gl2 = one('off')
    verdict(a2 - b2 == 0, 'off: labels does not grow: %d -> %d -> %d' % (b2, m2, a2))
    verdict(gl2 in ('0', ''), 'off: PATINA_AA_GLYPHS=%r' % gl2)
    st = c.run('Status')[1]
    verdict('patina-flat' in st and 'flat=absent' not in status()[1], 'patina-flat still running, no Guru')
    print('[INFO] final STATUS: %s' % status()[1])
    return fails


if __name__ == '__main__':
    sys.exit(1 if main() else 0)
