#!/usr/bin/env python3
"""splits_rig.py -- NS3.1 reachability test: native split panes in one UP-Term window
(plan thoughts/shared/plans/2026-10-19-native-splits.md, NS3.1 and the spawn-race note).

Cases (each opens its own vsh window, so `--only` runs one alone):
  sentinel  menu UP-Term > "Split right" is picked through the real menu; `UPTerm state` key "splits" goes up
            by 1 (the host split path ran, not a new tab)
  size      after the split each pane's `stty size` is the layout's: window inner width W px, 1 px divider,
            left pane = round(((W-1)/2) / cell) cells (render/layout.c first_ext), right = the rest in whole cells
  focus     a line typed with the new (right) pane focused runs in that pane only (vsh-only `stty size` writes
            the file; the typed marker is on the focused pane's text)
  paint     both panes stream paced wrapped text (SGR 41 left, 44 right, `Wait 1` every 8 lines) for ~20 s;
            pixels of each colour that cross the divider column are counted: must be 0 (NS0.2 attempt-3 method)
  winch     NS3.2: after a split, resizing the window (size-gadget drag) changes both panes' `stty size` cols to the
            layout's halves of the new width
  pane_close NS3.3: RA+Shift+W closes the focused pane (one pane again, input still works); `exit` in a pane does too
  pane_cycle RA+[ / RA+] cycle the focus to the previous / next pane (checked by each pane's own `pwd`)
  zoom      RA+Shift+Enter zooms the focused pane to full width, again restores the half width
  race      Split down from both panes back to back after a `cd` to two different dirs; every pane's `pwd`
            is the dir of the pane it was split from (the shared T:UP-Term-tab script race)
  restore   NS5.5: with split-restore = programs, Close tab on a 3-pane window writes ENV:up-term/splits; the next
            window takes it (file gone, `UPTerm state` split-restores 1), has 3 panes, each in its saved directory
            left to right, and the middle pane's running command started again (its script logs a second run)

Verdicts are written as each case ends (verdicts.Store, build/rig<N>/splits_rig/<case>.json), keyed by the
handler + vsh build, so a rerun skips passed cases.

  python3 tools/rig/splits_rig.py [rig=4] [--only CASE ...] [--failed] [--force] [--list]

Needs the rig up on a build with Split right / Split down and the `splits` counter. Rig 2 is refused.
Click-to-focus (NS3.4) moves between panes here; if that is not built, cases needing the left pane fail
with a clear message. Screenshots go to build/rig<N>/splits_rig/shots; the script never opens them.
"""
import os, pathlib, shutil, struct, sys, time

# the rig number is chosen before paths is imported (it reads UPTERM_RIG once)
_args = sys.argv[1:]
_rig = next((a for a in _args if a.isdigit()), os.environ.get('UPTERM_RIG') or '4')
if _rig == '2':
    raise SystemExit('splits_rig: rig 2 is not for this test (use 4)')
os.environ['UPTERM_RIG'] = _rig
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, typewin, vshwin, verdicts
import condev_rig as c
import tutorial_rig as tr          # counters(), active_title()

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
OUT = paths.RIG / 'splits_rig'
TITLE = 'splitswin'
BOX = '0/20/600/240'
CELL_W = int(os.environ.get('SPLITS_CELL_W') or 8)    # topaz 8 (tutorial_rig / mouse_rig.cell_xy)
BORDER_L, RIGHT_PAD, TOP_PAD = 4, 24, 14               # window inner left; scroll bar + border; title bar (drive.py)
CASES = ['sentinel', 'size', 'focus', 'paint', 'race', 'winch', 'pane_close', 'pane_cycle', 'zoom', 'drag', 'tab_close_split', 'font', 'restore']
RET, CTRL = typewin.RET, typewin.CTRL
SHIFT, RAMIGA = typewin.SHIFT, typewin.RAMIGA
K_W, K_D, K_ENTER, K_LBRACKET, K_RBRACKET = 0x11, 0x22, 0x44, 0x1A, 0x1B    # raw keycodes (RA+key = pane commands; [ ] = previous / next pane)
STREAM_SECS = 20


# ---- window / pane helpers ----------------------------------------------------------------

def line(s, wait=1.5):
    typewin.typ(s)
    typewin.key(RET, wait=wait)


def read(path):
    rc, out = c.run('Type %s' % path)
    return out.strip() if rc == 0 else ''


def stty_size(tag):
    """rows, cols of the focused pane (vsh `stty size`), or None"""
    f = 'RAM:ns31-%s.out' % tag
    c.run('Delete %s QUIET' % f)
    line('stty size >%s' % f)
    v = read(f).split()
    return (int(v[0]), int(v[1])) if len(v) == 2 and all(x.isdigit() for x in v) else None


def pwd_of(tag):
    f = 'RAM:ns31-%s.out' % tag
    c.run('Delete %s QUIET' % f)
    line('pwd >%s' % f)
    return read(f)


def window_box():
    """(x, y, w, h) of the active window; the split window keeps one window for all panes"""
    w = ami.window(tr.active_title())
    return w['box'] if w else None


def pick(label):
    """a real menu pick, UP-Term > label, in the active window (demo_rig.pick_tour / tutorial_rig use ui_menu)"""
    with ami._conn(60) as conn:
        ami._m().ui_menu(conn, 'UP-Term/' + label, tr.active_title())


def expected_cols(full_cols, cell=CELL_W):
    """layout.c: avail = W - 1 (pixel divider); first = round(avail/2 / cell) cells; the rest is the second.
    W, the window's inner width, is not known to the pixel: full_cols cells plus 0..cell-1 spare pixels
    (rig 4, 2026-10-20: 72 columns, both halves 36, so W > 576). Returns (lefts, rights): the pane widths
    that range allows, smallest first; a click or a fill uses lefts[0], a check takes any of them."""
    lefts, rights = set(), set()
    for W in range(full_cols * cell, full_cols * cell + cell):
        avail = W - 1
        first = ((avail // 2 + cell // 2) // cell) * cell     # ratio 1/2, snapped to whole cells (first_ext)
        lefts.add(first // cell)
        rights.add((avail - first) // cell)
    return tuple(sorted(lefts)), tuple(sorted(rights))


def pane_point(which, cols_left, nrows=1, row=0):
    """screen pixel inside a pane: which 'L' or 'R' of a two-column layout; row/nrows picks the band"""
    x0, y0, ww, hh = window_box()
    left_px = cols_left * CELL_W
    inner_top, inner_h = y0 + TOP_PAD + 16, hh - TOP_PAD - 16 - 6       # below the title bar and the tab bar row
    x = x0 + BORDER_L + (left_px // 2 if which == 'L' else left_px + 1 + (ww - BORDER_L - RIGHT_PAD - left_px - 1) // 2)
    y = inner_top + inner_h * (2 * row + 1) // (2 * nrows)
    return x, y


def col_point(col):
    """screen pixel at a window text column, half way down: a click there focuses the pane holding it"""
    x0, y0, ww, hh = window_box()
    inner_top, inner_h = y0 + TOP_PAD + 16, hh - TOP_PAD - 16 - 6
    return x0 + BORDER_L + col * CELL_W + CELL_W // 2, inner_top + inner_h // 2


def click(x, y):
    kx, ky = ami.pointer_scale()
    ami.script(('move', int(x * kx), int(y * ky)), ('wait', 2), ('button', 0, 1), ('wait', 2),
               ('button', 0, 0), ('wait', 5))
    time.sleep(1.0)


def fresh():
    """one window, vsh, focused; returns the full-width column count"""
    shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
    vshwin.open_vsh(TITLE, box=BOX, wait=6)
    typewin._last_title = TITLE
    typewin.click_in(TITLE)
    time.sleep(1)
    sz = stty_size('full')
    if not sz:
        raise SystemExit('splits_rig: no `stty size` in a fresh vsh window')
    return sz


def teardown():
    """end every pane's shell: exit, click elsewhere, repeat until the window is gone"""
    for i in range(10):
        if not ami.window(tr.active_title() or TITLE) or not (ami.window(TITLE) or ami.window('UP-Term')):
            break
        typewin.key(0x33, CTRL, wait=0.5)           # Ctrl-C: a stream loop ends
        line('exit', wait=1.5)
        w = ami.window(TITLE)
        if w:
            x, y, ww, hh = w['box']
            click(x + 20 + 60 * (i % 4), y + 40 + 40 * (i % 3))


def split_right_and_count(ck, before=None):
    """pick Split right; returns the splits counter delta check"""
    if before is None:
        before = tr.counters()[0].get('splits')
    pick('Split right')
    time.sleep(6)                                  # NewShell + REGISTER of the new pane
    after = tr.counters()[0].get('splits')
    ck(before is not None and after == before + 1,
       'Split right: `UPTerm state` splits %s -> %s (the host split path ran)' % (before, after), (before, after))
    return before, after


# ---- cases ----------------------------------------------------------------------------------

def case_sentinel(ck):
    fresh()
    try:
        split_right_and_count(ck)
    finally:
        teardown()


def case_size(ck):
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        want_l, want_r = expected_cols(full)
        right = stty_size('right')                  # the new pane has the keys
        ck(right is not None, 'right pane answers `stty size`', right)
        click(*pane_point('L', want_l[0]))
        left = stty_size('left')
        ck(left is not None, 'left pane answers `stty size` (click focus, NS3.4)', left)
        print('measured: full %dx%d, left %s, right %s, expected cols L=%s R=%s (cell %d)'
              % (rows, full, left, right, want_l, want_r, CELL_W))
        if left and right:
            ck(left[1] in want_l and right[1] in want_r and left[1] + right[1] <= full,
               'cols match the layout: left %s right %s' % (want_l, want_r), (left[1], right[1]))
            ck(left[1] + right[1] in (full - 1, full) and min(left[1], right[1]) >= 10,
               'cols sum to full-1 or full (1 px divider), both >= 10', (left[1], right[1], full))
            ck(left[0] == right[0] == rows, 'rows unchanged by a vertical divider', (left[0], right[0], rows))
    finally:
        teardown()


def case_focus(ck):
    rows, full = fresh()
    want_l, _ = expected_cols(full)
    try:
        split_right_and_count(ck)
        marker = 'NS31MARK%d' % int(time.time() % 100000)
        c.run('Delete RAM:ns31-who.out QUIET')
        line('echo %s; stty size >RAM:ns31-who.out' % marker)
        who = read('RAM:ns31-who.out').split()
        ck(len(who) == 2 and int(who[1]) in expected_cols(full)[1],
           'the line typed after the split ran in the new (right) pane: its width', who)
        text = typewin.screen_text()
        ck(marker in text, 'marker is in the focused pane\'s text', text[-200:])
        # the host's AmigaDOS-or-vsh left pane got nothing: a line goes to one pane only, so the proof is the
        # right-pane width in `who`; reading the left pane's text needs its focus (click), then the marker is absent
        click(*pane_point('L', want_l[0]))
        ltext = typewin.screen_text()
        ck(marker not in ltext, 'marker is not in the left pane\'s text', ltext[-200:])
    finally:
        teardown()


def pixel_leaks(bound, box, im=None):
    """(red_total, red_right, blue_total, blue_left) over the pane area, boundary column `bound` (NS0.2 attempt 3)"""
    x0, y0, ww, hh = box
    w, h, rows = ami.grab()
    red = blue = red_right = blue_left = 0
    for y in range(y0 + 40, y0 + hh - 10):
        row = rows[y]
        for x in range(x0 + 2, x0 + ww - RIGHT_PAD):
            r, g, b = row[3 * x], row[3 * x + 1], row[3 * x + 2]
            if r > g + 0x40 and r > b + 0x40:
                red += 1
                red_right += x >= bound
            elif b > r + 0x40 and b > g + 0x40:
                blue += 1
                blue_left += x < bound
    return red, red_right, blue, blue_left


def shot(name):
    try:
        from PIL import Image
        (OUT / 'shots').mkdir(parents=True, exist_ok=True)
        w, h, rows = ami.grab()
        im = Image.new('RGB', (w, h))
        im.frombytes(b''.join(rows))
        im.save(OUT / 'shots' / (name + '.png'))
    except ImportError:
        pass


def stream_cmd(sgr, label, width):
    # paced: 8 lines, then Wait 1 (an unpaced loop killed wasabid); lines longer than the pane so they wrap
    return ("while :; do for i in 1 2 3 4 5 6 7 8; do printf '\\033[%dm%%%ds\\033[0m %s long wrapped text "
            "to cross the pane edge and wrap again\\n' ''; done; Wait 1; done" % (sgr, width, label))


def case_paint(ck):
    rows, full = fresh()
    want_l, want_r = expected_cols(full)
    try:
        split_right_and_count(ck)
        line(stream_cmd(44, 'RIGHT', min(want_r) - 1), wait=0.5)     # the new (right) pane has the keys: blue
        click(*pane_point('L', want_l[0]))
        line(stream_cmd(41, 'LEFT', want_l[0] - 1), wait=0.5)
        box = window_box()
        bound = box[0] + BORDER_L + want_l[0] * CELL_W              # the divider column; the right pane starts one past
        t0 = time.time()
        for at, name in ((10, 't10'), (STREAM_SECS, 't20')):
            time.sleep(max(0, t0 + at - time.time()))
            shot('paint-' + name)
            red, red_right, blue, blue_left = pixel_leaks(bound, box)
            print('%s: red %d (at/over divider %d), blue %d (left of it %d), divider x=%d, box %s'
                  % (name, red, red_right, blue, blue_left, bound, box))
            ck(red > 0 and blue > 0, '%s: both panes show their colour' % name, (red, blue))
            ck(red_right == 0 and blue_left == 0, '%s: 0 leaked pixels across the divider' % name,
               (red_right, blue_left))
        ck(c.run('Version')[0] == 0, 'the rig is still alive after the stream')
    finally:
        teardown()


def case_race(ck):
    rows, full = fresh()
    want_l, _ = expected_cols(full)
    dirs = {'A': 'RAM:ns31a', 'B': 'RAM:ns31b'}
    for d in dirs.values():
        c.run('MakeDir %s QUIET' % d)
    try:
        line('cd %s' % dirs['A'])
        split_right_and_count(ck)                    # P1 right, focused, started where P0 was
        line('cd %s' % dirs['B'])
        # back to back: Split down from P0 (left), then at once from P1 (right)
        click(*pane_point('L', want_l[0]))
        pick('Split down')
        click(*pane_point('R', want_l[0]))
        pick('Split down')
        time.sleep(8)
        ck(tr.counters()[0].get('splits', 0) >= 3, 'three splits counted', tr.counters()[0].get('splits'))
        # P0 top-left, P2 bottom-left (split from P0), P1 top-right, P3 bottom-right (split from P1)
        probes = [('P0', 'L', 0, 'A'), ('P2', 'L', 1, 'A'), ('P1', 'R', 0, 'B'), ('P3', 'R', 1, 'B')]
        for name, side, row, src in probes:
            click(*pane_point(side, want_l[0], nrows=2, row=row))
            got = pwd_of(name)
            ck(got.lower().endswith(dirs[src][4:].lower()),
               '%s (split from the pane in %s) starts in %s' % (name, src, dirs[src]), got)
    finally:
        teardown()


# ---- NS3.2 / NS3.3 / arrows / zoom ---------------------------------------------------------------

def drag_size_gadget(dx, dy):
    """drag the active window's size gadget by (dx, dy) screen px (same method as winch_rig.drag)"""
    w = ami.window(tr.active_title())
    x, y, ww, hh = w['sys:size']
    kx, ky = ami.pointer_scale()
    p = lambda a, b: ('move', int(a * kx), int(b * ky))
    fx, fy = x + ww // 2, y + hh // 2
    ev = [p(fx, fy), ('wait', 2), ('button', 0, 1), ('wait', 3)]
    for i in range(1, 9):
        ev += [p(fx + dx * i // 8, fy + dy * i // 8), ('wait', 2)]
    ev += [('wait', 3), ('button', 0, 0), ('wait', 10)]
    ami.script(*ev)
    time.sleep(4)


def ra(k, shift=False):
    """a Right-Amiga pane command (IEQUALIFIER_RCOMMAND, plus IEQUALIFIER_LSHIFT when shift)"""
    typewin.key(k, RAMIGA | (SHIFT if shift else 0), wait=2.5)


def pane_count():
    """panes of the shown tab: `UPTerm state` key "panes" (host-owned, handler/slash.c state_keys), or None"""
    return tr.counters()[0].get('panes')


def state_key(key):
    """one `UPTerm state` key of the shown tab (panes, zoom, tabs, ...), or None"""
    return tr.counters()[0].get(key)


def case_winch(ck):
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        w0 = window_box()
        r0 = stty_size('right0')
        want_l, _ = expected_cols(full)
        click(*pane_point('L', want_l[0]))
        l0 = stty_size('left0')
        ck(bool(l0 and r0), 'both panes answer `stty size` before the resize', (l0, r0))
        # resize mechanism: size-gadget drag (as winch_rig); shrink by 160 px = 20 cells
        drag_size_gadget(-160, 0)
        w1 = window_box()
        ck(bool(w0 and w1 and w1[2] < w0[2]), 'window got narrower', (w0, w1))
        new_full = full + (w1[2] - w0[2]) // CELL_W if w0 and w1 else full
        want_l1, want_r1 = expected_cols(new_full)
        r1 = stty_size('right1')
        click(*pane_point('L', want_l1[0]))
        l1 = stty_size('left1')
        print('measured: before L %s R %s, after L %s R %s, expected after L=%s R=%s' % (l0, r0, l1, r1, want_l1, want_r1))
        if l0 and r0 and l1 and r1:
            ck(l1[1] != l0[1] and r1[1] != r0[1], 'cols of both panes changed', (l0, r0, l1, r1))
            ck(l1[1] in want_l1 and r1[1] in want_r1, 'cols are the halves of the new width: L %s R %s' % (want_l1, want_r1),
               (l1[1], r1[1]))
    finally:
        teardown()


def case_pane_close(ck):
    rows, full = fresh()
    try:
        for how in ('RA+Shift+W', 'exit'):
            split_right_and_count(ck)
            ck(stty_size('two-' + how) is not None and stty_size('two-' + how)[1] < full, '%s: two panes before closing' % how)
            if how == 'exit':
                line('exit', wait=4)
            else:
                ra(K_W, shift=True)
            time.sleep(3)
            sz = stty_size('one-' + how)
            ck(sz is not None, '%s: the remaining pane takes input' % how, sz)
            ck(bool(sz) and sz[1] == full, '%s: one pane again (full width %d)' % (how, full), (sz, full))
            n = pane_count()
            ck(n == 1, '%s: `UPTerm state` panes == 1' % how, n)
    finally:
        teardown()


def case_pane_cycle(ck):
    """RA+[ / RA+] cycle the focus through the panes in reading order, wrapping. The panes are told apart by
    their directory (the left pane sits in RAM:nsc-l, the right in RAM:nsc-r), not by width."""
    rows, full = fresh()
    try:
        line('makedir RAM:nsc-l RAM:nsc-r', wait=2)
        line('cd RAM:nsc-l', wait=2)
        split_right_and_count(ck)                  # the right pane is new and focused, started in RAM:nsc-l
        line('cd RAM:nsc-r', wait=2)
        got = pwd_of('cyc0')
        ck(got.lower().endswith('nsc-r'), 'new (right) pane has the keys: pwd is nsc-r', got)
        ra(K_LBRACKET)
        got = pwd_of('cyc1')
        ck(got.lower().endswith('nsc-l'), 'RA+[ : previous pane is the left one (pwd nsc-l)', got)
        ra(K_RBRACKET)
        got = pwd_of('cyc2')
        ck(got.lower().endswith('nsc-r'), 'RA+] : next pane is the right one (pwd nsc-r)', got)
        ra(K_RBRACKET)
        got = pwd_of('cyc3')
        ck(got.lower().endswith('nsc-l'), 'RA+] from the right pane wraps to the left one (pwd nsc-l)', got)
        ra(K_LBRACKET)
        got = pwd_of('cyc4')
        ck(got.lower().endswith('nsc-r'), 'RA+[ from the left pane wraps to the right one (pwd nsc-r)', got)
    finally:
        teardown()


def case_zoom(ck):
    rows, full = fresh()
    want_l, want_r = expected_cols(full)
    try:
        split_right_and_count(ck)
        half = stty_size('half')
        ck(bool(half) and half[1] in want_r, 'split: focused pane is half width', half)
        ra(K_ENTER, shift=True)
        ck(state_key('zoom') == 1, 'RA+Shift+Enter: `UPTerm state` zoom == 1', state_key('zoom'))
        z = stty_size('zoom')
        ck(bool(z) and z[1] == full, 'RA+Shift+Enter: zoomed pane reports the unsplit width %d' % full, z)
        ra(K_ENTER, shift=True)
        ck(state_key('zoom') == 0, 'RA+Shift+Enter again: `UPTerm state` zoom == 0', state_key('zoom'))
        back = stty_size('back')
        ck(bool(back) and half and back[1] == half[1], 'RA+Shift+Enter again: half width %d restored' % (half or (0, 0))[1], back)
    finally:
        teardown()


def drag_divider(x, y, dx):
    """press on the divider at screen (x, y), move by dx px in 8 steps, release (same event style as click / drag_size_gadget)"""
    kx, ky = ami.pointer_scale()
    p = lambda a: ('move', int(a * kx), int(y * ky))
    ev = [p(x), ('wait', 2), ('button', 0, 1), ('wait', 3)]
    for i in range(1, 9):
        ev += [p(x + dx * i // 8), ('wait', 2)]
    ev += [('wait', 3), ('button', 0, 0), ('wait', 10)]
    ami.script(*ev)
    time.sleep(3)


def case_drag(ck):
    """divider drag (divider_under / divider_move / divider_drop): 10 cells left, left pane loses, right gains"""
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        want_l, _ = expected_cols(full)
        r0 = stty_size('right0')                    # the new (right) pane has the keys
        click(*pane_point('L', want_l[0]))
        l0 = stty_size('left0')
        ck(bool(l0 and r0), 'both panes answer `stty size` before the drag', (l0, r0))
        if not (l0 and r0):
            return
        # divider pixel: window x + inner left border + left pane cols * cell width (the 1 px divider starts there)
        x0, _, _, _ = window_box()
        div_x = x0 + BORDER_L + l0[1] * CELL_W
        drag_divider(div_x, pane_point('L', l0[1])[1], -10 * CELL_W)
        l1 = stty_size('left1')                     # focus is still the left pane
        click(*pane_point('R', l0[1] - 10))
        r1 = stty_size('right1')
        print('measured: before L %s R %s, after L %s R %s' % (l0, r0, l1, r1))
        if l1 and r1:
            ck(l1[1] < l0[1] - 5, 'left pane lost columns', (l0, l1))
            ck(r1[1] > r0[1] + 5, 'right pane gained columns', (r0, r1))
            ck(abs((l1[1] + r1[1]) - (l0[1] + r0[1])) <= 2, 'column sum stays equal', (l0, r0, l1, r1))
        else:
            ck(False, 'both panes answer `stty size` after the drag', (l1, r1))
    finally:
        teardown()


def case_tab_close_split(ck):
    """regression a926073c: Close tab on a split tab closes every pane, no orphan pane is left"""
    rows, full = fresh()
    try:
        tabs0, panes0 = state_key('tabs'), pane_count()
        ck(tabs0 is not None and panes0 == 1, 'one tab, one pane to start', (tabs0, panes0))
        pick('New tab')
        time.sleep(5)
        ck(state_key('tabs') == (tabs0 or 0) + 1, '`UPTerm state` tabs +1 after New tab', state_key('tabs'))
        split_right_and_count(ck)
        ck(pane_count() == 2, 'the new tab has two panes', pane_count())
        pick('Close tab')
        time.sleep(5)
        ck(state_key('tabs') == tabs0, '`UPTerm state` tabs back to %s after Close tab' % tabs0, state_key('tabs'))
        ck(pane_count() == panes0, 'shown tab has its original %s pane(s): no orphan pane' % panes0, pane_count())
    finally:
        teardown()


# ---- driver -----------------------------------------------------------------------------------

def case_font(ck):
    """NS5.3: the font is the whole window's. A font change in one pane of a
    split reaches every pane (/state slot-fonts counts it), the panes stay."""
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        right = stty_size('font-r0')
        ra(K_LBRACKET)
        left = stty_size('font-l0')
        fonts = state_key('slot-fonts') or 0
        # the face's next designed size: a rig with tall pixels has topaz
        # 8 -> 9 up and nothing down, square pixels topaz 11 down (as the
        # menus rig); topaz 9 keeps the width, so the whole size is compared
        way = None
        for w in ('bigger', 'smaller'):
            line('UPTerm font-size ' + w, wait=3)
            if (state_key('slot-fonts') or 0) == fonts + 1:
                way = w
                break
        ck(way is not None, 'font size step in a split pane: `UPTerm state` slot-fonts +1',
           state_key('slot-fonts'))
        ck(pane_count() == 2, 'font step: the panes stay (2)', pane_count())
        l2 = stty_size('font-l1')
        ck(bool(left and l2) and l2 != left, 'font %s: the focused pane has a new size' % way, (left, l2))
        ra(K_RBRACKET)
        r2 = stty_size('font-r1')
        ck(bool(right and r2) and r2 != right, 'font %s: the other pane took the font too' % way, (right, r2))
    finally:
        teardown()


RESTORE_DIRS = ['RAM:nsr-a', 'RAM:nsr-b', 'RAM:nsr-c']
RESTORE_JOB, RESTORE_LOG = 'RAM:nsr-job', 'RAM:nsr-log'
PROFILE = 'ENV:up-term/up-term'


def log_runs():
    return len([l for l in (ami.read_file(RESTORE_LOG) or b'').decode('latin-1').splitlines() if l.strip()])


def restore_profile(mode):
    """set split-restore = mode in the profile, the saved split gone; returns the undo for finally"""
    had = ami.read_file(PROFILE)
    c.run('Copy %s %s.nsr QUIET' % (PROFILE, PROFILE) if had is not None else 'Delete %s.nsr QUIET' % PROFILE)
    c.run('Echo >>%s "[profile default]"' % PROFILE)
    c.run('Echo >>%s "split-restore = %s"' % (PROFILE, mode))
    c.run('Delete ENV:up-term/splits ENVARC:up-term/splits QUIET')

    def undo():
        c.run('Delete ENV:up-term/splits ENVARC:up-term/splits QUIET')
        if had is not None:
            c.run('Copy %s.nsr %s QUIET' % (PROFILE, PROFILE))
            c.run('Delete %s.nsr QUIET' % PROFILE)
        else:
            c.run('Delete %s QUIET' % PROFILE)
    return undo


def case_restore(ck):
    """NS5.5: split-restore programs, end to end through Close tab and a new window"""
    undo = restore_profile('programs')
    c.run('Delete %s QUIET' % RESTORE_LOG)
    for d in RESTORE_DIRS:
        c.run('MakeDir %s QUIET' % d)
    c.run('Echo >%s "Echo >>%s run"' % (RESTORE_JOB, RESTORE_LOG))
    c.run('Echo >>%s "Wait 600"' % RESTORE_JOB)
    try:
        fresh()
        line('cd %s' % RESTORE_DIRS[0])
        split_right_and_count(ck)
        line('cd %s' % RESTORE_DIRS[1])
        split_right_and_count(ck)
        line('cd %s' % RESTORE_DIRS[2])
        ck(pane_count() == 3, 'three panes before Close tab', pane_count())
        # r(a, r(b, c)) at 72 columns: a 36, b and c 18 each; a click focuses a pane (RA+Left does
        # not reach the window: Intuition's Amiga + cursor key pointer control takes it)
        cols = (18, 45, 63)
        click(*col_point(cols[1]))
        line('Execute %s' % RESTORE_JOB, wait=3)          # from here `UPTerm state` cannot run in this pane
        ck(log_runs() == 1, 'the middle pane runs the job once before the close', log_runs())
        pick('Close tab')
        time.sleep(6)
        saved = (ami.read_file('ENV:up-term/splits') or b'').decode('latin-1')
        ck('nsr-b' in saved and 'Execute %s' % RESTORE_JOB in saved,
           'Close tab wrote ENV:up-term/splits with the cwds and the running command', saved)
        ck(not ami.window(TITLE), 'the window closed with its tab', None)
        shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
        vshwin.open_vsh(TITLE, box=BOX, wait=25)          # three shells spawn one after another
        typewin._last_title = TITLE
        typewin.click_in(TITLE)
        time.sleep(2)
        ck(ami.read_file('ENV:up-term/splits') is None, 'the split file was taken (deleted)', None)
        ck(log_runs() == 2, 'the middle pane started its command again', log_runs())
        got = []
        for i in range(3):
            click(*col_point(cols[i]))
            typewin.key(0x33, CTRL, wait=1)               # Ctrl-C: the restarted Wait ends
            got.append(pwd_of('restore%d' % i))
        # `UPTerm state` types into the focused pane: only now, after every pane's Ctrl-C, is it at a prompt
        ck(state_key('split-restores') == 1, '`UPTerm state` split-restores 1 (the restore ran)',
           state_key('split-restores'))
        ck(pane_count() == 3, 'the new window has the three panes', pane_count())
        for want, g in zip(RESTORE_DIRS, got):
            ck(g.lower().endswith(want[4:]), 'pane in %s restored there' % want, got)
    finally:
        teardown()
        c.run('Delete %s %s QUIET' % (RESTORE_JOB, RESTORE_LOG))
        undo()


def run_case(name, store, fp):
    ck = verdicts.Checks()
    try:
        globals()['case_' + name](ck)
    except SystemExit as e:
        ck(False, 'case aborted', e)
    except Exception as e:                           # a rig fault is a failed case, with the reason recorded
        ck(False, 'case raised %s' % type(e).__name__, e)
    store.record(name, not ck.failed, '; '.join(ck.failed) or 'ok (%d checks)' % ck.total, fp)
    return not ck.failed


def main(argv):
    args = [a for a in argv if a != _rig]
    only = [args[i + 1] for i, a in enumerate(args) if a == '--only' and i + 1 < len(args)]
    if '--help' in args or '-h' in args:
        print(__doc__)
        return 0
    store = verdicts.Store(OUT)
    fp = verdicts.fingerprint(ROOT / 'build/amiga/vtcon-handler', ROOT / 'build/amiga/vsh')
    if '--list' in args:
        for cs in CASES:
            v = store.verdict(cs)
            print('%-9s %s' % (cs, (v['verdict'] + ('' if v['fp'] == fp else ' (older build)')) if v else 'never run'))
        return 0
    for o in only:
        if o not in CASES:
            raise SystemExit('splits_rig: unknown case %r (%s)' % (o, ' '.join(CASES)))
    run, skip = verdicts.select(store, CASES, lambda cs: fp, only=only, failed='--failed' in args,
                                force='--force' in args)
    for cs, why in skip:
        print('skip %s: %s' % (cs, why))
    ok = True
    with paths.rig_lock():
        for cs, why in run:
            print('== %s (%s) ==' % (cs, why))
            ok = run_case(cs, store, fp) and ok
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
