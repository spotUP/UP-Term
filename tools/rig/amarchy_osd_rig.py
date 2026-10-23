#!/usr/bin/env python3
"""amarchy_osd_rig.py -- the Amarchy quick menu as an on-screen display (AM-OSD O6, one reachability test).

Entry point: the Amarchy commodity's hotkey (Left Amiga + Space), typed as a user would. The menu
window it opens must be the OSD the plan states (thoughts/shared/plans/2026-10-23-amarchy-osd.md):
  * titled "Amarchy", no gadgets at all (NOBORDER: no close, depth, drag or size gadget, no scroll bar)
  * sized to the menu, mn_osd_size: root 24x6 cells (amarchy/menu_core.c, pinned by its host test); the width is
    cols * cell width, the height rows * cell height
  * centred on the front screen (centre within 1 cell)
  * Esc at the top level closes it
  * another window taking the focus closes it (CLOSEINACTIVE) and amarchy-menu exits
Sentinel that the new code ran: the old spec opened XCON:0/20/640/300/Amarchy/CLOSE with gadgets at
x 0, y 20; a window of exactly 24 x 6 cells, no gadget and the screen's centre cannot come from it,
and the close-on-focus-loss exists only in the CLOSEINACTIVE branch of the handler.
Self-checking. Rig up (rig.py start --max), handler, Amarchy and amarchy-menu installed (rig.py install,
then stop and start the rig so the new handler is resident). UPTERM_RIG=<n>."""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c, tabstate, vshwin, stage_rig

LAMIGA, SPACE, ESC = 0x40, 0x40, 0x45
COLS, ROWS = 24, 6          # mn_osd_size for the root node (menu_core.c; the host test pins the number)
TITLE = 'Amarchy'
bad = 0


def check(ok, what):
    global bad
    bad += not ok
    print('%s %s' % ('ok' if ok else 'FAIL', what), flush=True)
    return ok


def screen_size():
    b = ami.req(0x0A)
    # count, then per screen: index, width, height (big-endian H), depth, ... ; the front screen is the first
    w, h = struct.unpack('>HH', b[2:6])
    return w, h


def gadgets():
    out = []
    cur = None
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W '):
            cur = line.split()[-1] == TITLE or ('"%s"' % TITLE) in line
        elif cur and line.startswith('G '):
            out.append(line)
    return out


def wait_window(present, secs=15):
    end = time.time() + secs
    while time.time() < end:
        if bool(ami.window(TITLE)) == present:
            return True
        time.sleep(0.5)
    return False


def menu_running():
    return 'amarchy-menu' in c.run('Status')[1]


def open_menu():
    ami.key(SPACE, LAMIGA)
    return wait_window(True)


def main():
    # the commodity and amarchy-menu on the rig must be this build (stage_rig compares SHA-256 and puts them)
    stage_rig.report(stage_rig.stage_build(build='--no-build' not in sys.argv))
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    time.sleep(3)
    tabstate.open_host('osdhost')
    check(ami.window(TITLE) is None, 'no "%s" window before the hotkey' % TITLE)

    # the hotkey opens the OSD
    if not check(open_menu(), 'Super+Space opened a window titled "%s"' % TITLE):
        return 1
    time.sleep(2)
    w = ami.window(TITLE)
    x, y, ww, hh = w['box']
    sw, sh = screen_size()
    cw, ch = ww // COLS, hh // ROWS
    print('info window %dx%d at %d,%d on a %dx%d screen; cell %dx%d' % (ww, hh, x, y, sw, sh, cw, ch))
    check(ww == cw * COLS and hh == ch * ROWS and 4 <= cw <= 16 and 6 <= ch <= 20,
          'size is %dx%d cells: %dx%d px is whole cells of %dx%d' % (COLS, ROWS, ww, hh, cw, ch))
    check(abs(x - (sw - ww) // 2) <= cw and abs(y - (sh - hh) // 2) <= ch,
          'centred on the screen (x %d want %d, y %d want %d, within one cell)' % (x, (sw - ww) // 2, y, (sh - hh) // 2))
    g = gadgets()
    check(not g, 'no gadgets, no scroll bar (G lines: %d)' % len(g))
    check(menu_running(), 'amarchy-menu is running while the window is open')

    # Esc at the top level closes it
    ami.key(ESC)
    check(wait_window(False), 'Esc closed the window')
    time.sleep(2)
    check(not menu_running(), 'amarchy-menu exited after Esc')

    # another window taking the focus closes it
    if check(open_menu(), 'second open by the hotkey'):
        time.sleep(3)
        check(bool(ami.window(TITLE)['active']), 'the menu window is active when the other window opens')
        vshwin.open_vsh('osdother', wait=4)
        check(wait_window(False, 30), 'a window taking the focus closed the menu (CLOSEINACTIVE)')
        time.sleep(2)
        check(not menu_running(), 'amarchy-menu exited after the focus loss')
    c.run('Delete >NIL: RAM:osd.#? QUIET')
    print('FAIL' if bad else 'PASS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
