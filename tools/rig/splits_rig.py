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
import os, pathlib, re, shutil, struct, subprocess, sys, time

# the rig number is chosen before paths is imported (it reads UPTERM_RIG once)
_args = sys.argv[1:]
_rig = next((a for a in _args if a.isdigit()), os.environ.get('UPTERM_RIG') or '4')
if _rig == '2':
    raise SystemExit('splits_rig: rig 2 is not for this test (use 4)')
os.environ['UPTERM_RIG'] = _rig
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, typewin, vshwin, verdicts
import condev_rig as c
import ixpty_rig, winch_rig
import tutorial_rig as tr          # counters(), active_title()

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
OUT = paths.RIG / 'splits_rig'
TITLE = 'splitswin'
BOX = '0/20/600/240'
CELL_H = int(os.environ.get('SPLITS_CELL_H') or 16)   # measured rig 4 (2026-10-20): cursor bbox 8 x 16 px
CELL_W = int(os.environ.get('SPLITS_CELL_W') or 8)    # topaz 8 (tutorial_rig / mouse_rig.cell_xy)
BORDER_L, RIGHT_PAD, TOP_PAD = 4, 24, 14               # window inner left; scroll bar + border; title bar (drive.py)
CASES = ['sentinel', 'size', 'focus', 'paint', 'race', 'winch', 'pane_close', 'host_exit_cycles', 'pane_cycle', 'zoom', 'drag', 'tab_close_split', 'menu_keys', 'tab_layout', 'gadget_close_split', 'font', 'restore', 'cursor_focus', 'mouse_report', 'view_split', 'perf']
RET, CTRL = typewin.RET, typewin.CTRL
SHIFT, RAMIGA = typewin.SHIFT, typewin.RAMIGA
K_W, K_D, K_ENTER, K_LBRACKET, K_RBRACKET = 0x11, 0x22, 0x44, 0x1A, 0x1B
K_1, K_2 = 0x01, 0x02                                   # RA+digit shows that tab (handler tab_key)    # raw keycodes (RA+key = pane commands; [ ] = previous / next pane)
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


def pick(label, menu='UP-Term'):
    """a real menu pick, `menu` > label (default UP-Term), in the active window (demo_rig.pick_tour / tutorial_rig use ui_menu)"""
    with ami._conn(60) as conn:
        ami._m().ui_menu(conn, menu + '/' + label, tr.active_title())


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


def input_handlers(tag):
    """input.device's handler list (VTC:inputlist): (count, intuition, console, [names]) or None"""
    shutil.copyfile(ROOT / 'build/amiga/inputlist', VTC / 'inputlist')    # rig.py install refreshes only files VTC: holds
    f = 'RAM:ns31-%s.out' % tag
    c.run('Delete %s QUIET' % f)
    c.run('VTC:inputlist >%s' % f)
    rows = [l.split() for l in read(f).splitlines() if l.strip()]
    if not rows or rows[-1][0] != 'handlers' or len(rows[-1]) != 4:
        return None
    names = [' '.join(r[:-3]) for r in rows[:-1]]
    return int(rows[-1][1]), rows[-1][2] == 'intuition=1', rows[-1][3] == 'console=1', names


def check_handlers(ck, base, what):
    """after a case's windows are gone: the list holds intuition and console, no SIGWINCH handler of a freed
    window, and as many handlers as at case start (a window's handler left behind hung input.device,
    thoughts/spot/crash/accum2.md)"""
    h = input_handlers('il-' + what)
    ck(h is not None, '%s: VTC:inputlist answers' % what, h)
    if h:
        ck(h[1] and h[2], '%s: intuition and console are on the input handler list' % what, h[3])
        ck(h[0] == base, '%s: %d input handlers, %d at case start' % (what, h[0], base), h[3])
        ck('vtcon SIGWINCH' not in h[3], '%s: no vtcon SIGWINCH handler survives the windows' % what, h[3])


def teardown():
    """end every pane's shell: exit, click elsewhere, repeat until the window is gone. A window that outlives
    the exits is reported with its counters and the rig restarted, so the next case never types into it
    (2026-10-20: a hung pane_cycle left a split window that took 10 blind exits per later case)."""
    for i in range(6):
        w = ami.window(TITLE) or ami.window('UP-Term')
        if not w:
            return
        typewin.key(0x33, CTRL, wait=0.5)           # Ctrl-C: a stream loop ends
        line('exit', wait=1.5)
        if not (ami.window(TITLE) or ami.window('UP-Term')):
            return
        x, y, ww, hh = w['box']
        click(x + 20 + 60 * (i % 4), y + 40 + 40 * (i % 3))
    print('[ERROR] teardown: the window survived 6 exits (UPTerm state %r): rig.py stop + start'
          % (tr.counters()[0],), flush=True)
    rig = [sys.executable, str(ROOT / 'tools/rig/rig.py')]
    subprocess.run(rig + ['stop'], capture_output=True, timeout=120)
    subprocess.run(rig + ['start'], capture_output=True, timeout=400)


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

def drag_size_gadget(dx, dy, settle=4):
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
    time.sleep(settle)


def ra(k, shift=False):
    """a Right-Amiga pane command (IEQUALIFIER_RCOMMAND, plus IEQUALIFIER_LSHIFT when shift)"""
    typewin.key(k, RAMIGA | (SHIFT if shift else 0), wait=2.5)


def pane_count():
    """panes of the shown tab: `UPTerm state` key "panes" (host-owned, handler/slash.c state_keys), or None"""
    return tr.counters()[0].get('panes')


def state_key(key):
    """one `UPTerm state` key of the shown tab (panes, zoom, tabs, ...), or None"""
    return tr.counters()[0].get(key)


def winch_report(path):
    """ixwinch's report (winch_rig.parse): ((cols, rows) at start, SIGWINCHs, (cols, rows) now, read, errno)"""
    try:
        return winch_rig.parse(path.read_text().strip())
    except (OSError, ValueError, IndexError):
        return None


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
        # each pane runs ixwinch (winch_rig's SIGWINCH catcher) in read mode: a caught signal ends the read and
        # rewrites its report at once, so the time from the resize to both reports is the delivery time
        shutil.copyfile(ROOT / 'build/amiga/ixwinch', VTC / 'ixwinch')
        ixpty_rig.use_ixemul()
        outs = {k: VTC / ('winch-%s.out' % k) for k in 'LR'}
        for k in 'LR':
            if outs[k].exists():
                outs[k].unlink()
            click(*pane_point(k, want_l[0]))
            line('VTC:ixwinch 60 read VTC:winch-%s.out' % k, wait=3)
        ck(all(winch_report(outs[k]) and winch_report(outs[k])[1] == 0 for k in 'LR'),
           'ixwinch runs in both panes, no SIGWINCH yet', [winch_report(outs[k]) for k in 'LR'])
        # resize mechanism: size-gadget drag (as winch_rig); shrink by 160 px = 20 cells
        drag_size_gadget(-160, 0, settle=0)
        t0 = None
        lat = {}
        end = time.time() + 15
        while time.time() < end and len(lat) < 2:
            if t0 is None and (window_box() or w0)[2] != (w0 or (0, 0, 0))[2]:
                t0 = time.time()                    # Intuition resized the window: the clock starts
            for k in 'LR':
                if t0 is not None and k not in lat and winch_report(outs[k]) and winch_report(outs[k])[1] >= 1:
                    lat[k] = time.time() - t0
            time.sleep(0.05)
        print('SIGWINCH latency after the resize: %s' % {k: round(v, 2) for k, v in lat.items()})
        ck(len(lat) == 2 and max(lat.values()) < 1.0, 'both panes caught SIGWINCH within 1 s of the resize', lat)
        time.sleep(2)
        w1 = window_box()
        ck(bool(w0 and w1 and w1[2] < w0[2]), 'window got narrower', (w0, w1))
        new_full = full + (w1[2] - w0[2]) // CELL_W if w0 and w1 else full
        want_l1, want_r1 = expected_cols(new_full)
        for k in 'LR':
            click(*pane_point(k, want_l1[0]))
            typewin.key(0x33, CTRL, wait=1)            # Ctrl-C: ixwinch ends
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
    base = input_handlers('il-base')
    rows, full = fresh()
    try:
        # 'left-exit': the host's own shell (the left pane) ends while the right pane lives -- the window stays
        for how in ('RA+Shift+W', 'exit', 'left-exit'):
            split_right_and_count(ck)
            ck(stty_size('two-' + how) is not None and stty_size('two-' + how)[1] < full, '%s: two panes before closing' % how)
            if how == 'left-exit':
                click(*col_point(2))
                line('exit', wait=4)
                ck(bool(ami.window('UP-Term') or ami.window(TITLE)),     # not active_title(): a requester is active then
                   'left-exit: the window stays for the right pane', None)
            elif how == 'exit':
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
    check_handlers(ck, base[0] if base else -1, 'pane_close')


def case_host_exit_cycles(ck):
    """the host's own shell ends first, the window closes later, ROUNDS times on one boot; then a fresh window
    still takes input. Before the fix each round left its SIGWINCH input handler on input.device pointing at a
    freed con; the machine hung on input within 2-4 rounds (thoughts/spot/crash/accum.md, 2026-10-20)."""
    base = input_handlers('il-base')
    for r in range(ROUNDS):
        rows, full = fresh()
        try:
            split_right_and_count(ck)
            click(*col_point(2))
            line('exit', wait=4)
            ck(bool(ami.window('UP-Term') or ami.window(TITLE)), 'round %d: the window stays for the right pane' % r, None)
        finally:
            teardown()
    try:
        sz = fresh()
        ck(sz is not None, 'after %d rounds a fresh window takes input' % ROUNDS, sz)
    finally:
        teardown()
    check_handlers(ck, base[0] if base else -1, 'host_exit_cycles')


ROUNDS = 5


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
            # NS4.2: the divider moved 10 cells, so each pane changes by exactly that many columns, the sum is kept
            ck(l1[1] == l0[1] - 10, 'left pane lost exactly 10 columns', (l0, l1))
            ck(r1[1] == r0[1] + 10, 'right pane gained exactly 10 columns', (r0, r1))
            ck(l1[1] + r1[1] == l0[1] + r0[1], 'column sum stays equal', (l0, r0, l1, r1))
            ck(l1[0] == l0[0] and r1[0] == r0[0], 'rows are unchanged', (l0, r0, l1, r1))
        else:
            ck(False, 'both panes answer `stty size` after the drag', (l1, r1))
    finally:
        teardown()


MOUSE_OUT = 'RAM:ns42-mouse.txt'


def case_mouse_report(ck):
    """NS4.2: a click inside the right pane (not on the divider) reaches a ?1000/?1006 program there with the
    PANE-relative cell: ESC [ < 0 ; col ; row M then m. ixmouse (tests/amiga/ixmouse.c; rawprobe's WaitForChar
    returns at once on a vsh stdin) enables the modes and logs every byte it reads as hex. Two cells; a click in the left pane
    (which has no program) must add no report."""
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        want_l, _ = expected_cols(full)
        r0 = stty_size('mr-r')
        click(*pane_point('L', want_l[0]))
        l0 = stty_size('mr-l')
        ck(bool(l0 and r0), 'both panes answer `stty size`', (l0, r0))
        if not (l0 and r0):
            return
        click(*pane_point('R', l0[1]))              # the right pane has the keys again
        c.run('Delete %s QUIET' % MOUSE_OUT)
        shutil.copyfile(ROOT / 'build/amiga/ixmouse', VTC / 'ixmouse')    # rig.py install refreshes only files VTC: holds
        line('VTC:ixmouse %s 90' % MOUSE_OUT, wait=3.0)
        box = window_box()
        div_x = box[0] + BORDER_L + l0[1] * CELL_W
        cells = [(5, 3), (12, 7)]
        for col, row in cells:
            click(div_x + 1 + (col - 1) * CELL_W + CELL_W // 2, box[1] + TOP_PAD + (row - 1) * CELL_H + CELL_H // 2)
        click(*pane_point('L', l0[1]))              # left pane: not a report for the right pane's program
        ra(K_RBRACKET)                              # right pane again so the ctrl-c below ends rawprobe
        typewin.key(0x33, CTRL, wait=1.0)           # ixmouse ignores ISIG: end it with `q`
        typewin.typ('q')
        time.sleep(1.5)
        got = re.findall(r'\x1b\[<(\d+);(\d+);(\d+)([Mm])', read_hex(MOUSE_OUT))
        print('reports:', got, repr(read(MOUSE_OUT)[:120]))
        want = []
        for col, row in cells:
            want += [('0', str(col), str(row), 'M'), ('0', str(col), str(row), 'm')]
        ck(got == want, 'reports are the pane-relative cells %s (press, release each); none for the left pane' % cells,
           got)
    finally:
        teardown()


def read_hex(path):
    """a ixmouse output file (hex bytes, space separated) as a latin-1 string"""
    return ''.join(chr(int(h, 16)) for h in read(path).split() if re.fullmatch('[0-9a-f]{2}', h))


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


# every pane/split id in handler/menu_def.h MENU_DEF_ROWS (the table tests/test_menudef.c counts from), in table order:
MENU_PANE_IDS = ('Split right', 'Split down', 'Close pane', 'Zoom pane', 'Previous pane', 'Next pane')


def at_pane(ck, tag, want, what):
    """the focused pane is the one that sits in RAM:<want> (each pane is told apart by its directory)"""
    got = pwd_of(tag)
    ck(got.lower().endswith(want), '%s: focus is on the pane in %s' % (what, want), got)


def case_menu_keys(ck):
    """NS4.1: every pane/split menu id is picked through the real menu and asserts pane count / focus / zoom;
    the Right Amiga keys RA+D (Split right), RA+Shift+D (Split down), RA+Shift+W (Close pane) are driven too"""
    base = input_handlers('il-base')
    rows, full = fresh()
    try:
        line('makedir RAM:nsm-a RAM:nsm-b RAM:nsm-c RAM:nsm-d', wait=2)
        line('cd RAM:nsm-a', wait=2)
        # menu: Split right -> 2 panes, the new pane has the focus
        n0 = state_key('splits')
        pick('Split right'); time.sleep(6)
        ck(state_key('splits') == n0 + 1 and pane_count() == 2, 'menu Split right: splits +1, 2 panes', (state_key('splits'), pane_count()))
        line('cd RAM:nsm-b', wait=2)
        at_pane(ck, 'm1', 'nsm-b', 'menu Split right')
        pick('Previous pane'); time.sleep(2.5)
        at_pane(ck, 'm2', 'nsm-a', 'menu Previous pane')
        pick('Next pane'); time.sleep(2.5)
        at_pane(ck, 'm3', 'nsm-b', 'menu Next pane')
        pick('Zoom pane'); time.sleep(2.5)
        z = stty_size('mz')
        ck(state_key('zoom') == 1 and bool(z) and z[1] == full, 'menu Zoom pane: zoom 1, full width %d' % full, (state_key('zoom'), z))
        pick('Zoom pane'); time.sleep(2.5)
        z = stty_size('mz2')
        ck(state_key('zoom') == 0 and bool(z) and z[1] < full, 'menu Zoom pane again: zoom 0, half width', (state_key('zoom'), z))
        pick('Close pane'); time.sleep(3)
        ck(pane_count() == 1, 'menu Close pane: 1 pane', pane_count())
        at_pane(ck, 'm4', 'nsm-a', 'menu Close pane (focus moved to the survivor)')
        pick('Split down'); time.sleep(6)
        sz = stty_size('mdn')
        ck(pane_count() == 2 and bool(sz) and sz[0] < rows and sz[1] == full, 'menu Split down: 2 panes, focused pane is a full-width lower half', (pane_count(), sz, rows))
        pick('Close pane'); time.sleep(3)
        ck(pane_count() == 1, 'menu Close pane after Split down: 1 pane', pane_count())
        # keys: RA+D -> 2 panes (Split right)
        typewin.key(K_D, RAMIGA, wait=6)
        ck(pane_count() == 2, 'RA+D: 2 panes', pane_count())
        sz = stty_size('kd')
        ck(bool(sz) and sz[1] < full and sz[0] == rows, 'RA+D split right: focused pane is a half-width, full-height column', (sz, rows, full))
        line('cd RAM:nsm-c', wait=2)
        at_pane(ck, 'k1', 'nsm-c', 'RA+D')
        # RA+Shift+D -> 3 panes, the new one is the lower half of the focused pane (rows halve)
        typewin.key(K_D, RAMIGA | SHIFT, wait=6)
        ck(pane_count() == 3, 'RA+Shift+D: 3 panes', pane_count())
        sz = stty_size('kdd')
        ck(bool(sz) and sz[0] < rows and sz[1] < full, 'RA+Shift+D split down: focused pane has fewer rows than the window', (sz, rows))
        line('cd RAM:nsm-d', wait=2)
        at_pane(ck, 'k2', 'nsm-d', 'RA+Shift+D')
        # RA+Shift+W twice -> 2, then 1
        ra(K_W, shift=True); time.sleep(3)
        ck(pane_count() == 2, 'RA+Shift+W: 2 panes', pane_count())
        ra(K_W, shift=True); time.sleep(3)
        ck(pane_count() == 1, 'RA+Shift+W again: 1 pane', pane_count())
        sz = stty_size('kend')
        ck(bool(sz) and sz[1] == full, 'one pane again at full width %d' % full, sz)
    finally:
        teardown()
    check_handlers(ck, base[0] if base else -1, 'menu_keys')


def pane_sizes(tag, n):
    """(rows, cols) of each of the shown tab's n panes in cycle order, starting at the focused one; n RA+] steps
    bring the focus back where it was"""
    out = []
    for i in range(n):
        out.append(stty_size('%s%d' % (tag, i)))
        ra(K_RBRACKET)
    return out


def typed_in(ck, tag, want, what):
    """a line typed now runs in the focused pane: it writes a file in that pane's directory"""
    f = 'mk-%s' % tag
    for d in ('nst-a1', 'nst-a2', 'nst-b1', 'nst-b2', 'nst-b3'):
        c.run('Delete RAM:%s/%s QUIET' % (d, f))
    line('echo x >%s' % f)
    where = [d for d in ('nst-a1', 'nst-a2', 'nst-b1', 'nst-b2', 'nst-b3') if read('RAM:%s/%s' % (d, f))]
    ck(where == [want], '%s: typing landed in the focused pane (%s)' % (what, want), where)


def case_tab_layout(ck):
    """NS5.2: each tab owns its layout. Tab 1 has two panes, tab 2 three; switching by menu (Next / Previous tab)
    and by key (RA+1 / RA+2) shows the tab with the pane count and per-pane `stty size` it had, the focus where
    it was, and typing lands in that tab's focused pane"""
    base = input_handlers('il-base')
    rows, full = fresh()
    try:
        line('makedir RAM:nst-a1 RAM:nst-a2 RAM:nst-b1 RAM:nst-b2 RAM:nst-b3', wait=2)
        line('cd RAM:nst-a1', wait=2)
        split_right_and_count(ck)
        line('cd RAM:nst-a2', wait=2)
        size_a = pane_sizes('a', 2)
        ck(pane_count() == 2 and all(size_a), 'tab 1: 2 panes, each answers stty size', size_a)
        pick('New tab'); time.sleep(5)
        ck(state_key('tabs') == 2 and state_key('active') == 2 and pane_count() == 1, 'New tab: 2 tabs, tab 2 (`active` is 1-based) shown with 1 pane',
           (state_key('tabs'), state_key('active'), pane_count()))
        line('cd RAM:nst-b1', wait=2)
        split_right_and_count(ck)
        line('cd RAM:nst-b2', wait=2)
        split_right_and_count(ck)
        line('cd RAM:nst-b3', wait=2)
        size_b = pane_sizes('b', 3)
        ck(pane_count() == 3 and all(size_b), 'tab 2: 3 panes, each answers stty size', size_b)
        # the second tab makes the tab bar appear: it takes one text row from every tab, columns do not change
        size_a = [(r - 1, cl) for r, cl in size_a]
        ck(size_a != size_b and len(size_a) != len(size_b), 'the two tabs really differ (2 vs 3 panes)', (size_a, size_b))
        steps = (('Previous tab', lambda: pick('Previous tab'), 1, size_a, 'nst-a2'),
                 ('RA+2', lambda: typewin.key(K_2, RAMIGA, wait=3), 2, size_b, 'nst-b3'),
                 ('RA+1', lambda: typewin.key(K_1, RAMIGA, wait=3), 1, size_a, 'nst-a2'),
                 ('Next tab', lambda: pick('Next tab'), 2, size_b, 'nst-b3'))
        for i, (name, go, active, want, focus) in enumerate(steps):
            go(); time.sleep(3)
            ck(state_key('active') == active, '%s: tab %d is shown' % (name, active), state_key('active'))
            ck(pane_count() == len(want), '%s: the shown tab has its %d panes' % (name, len(want)), pane_count())
            at_pane(ck, 'tl%d' % i, focus, name)
            typed_in(ck, 'tl%d' % i, focus, name)
            got = pane_sizes('ts%d-' % i, len(want))
            ck(got == want, '%s: every pane has the stty size it had before the switch' % name, (got, want))
            at_pane(ck, 'tf%d' % i, focus, name + ' after a full pane cycle')
    finally:
        teardown()
    check_handlers(ck, base[0] if base else -1, 'tab_layout')


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


def cursor_shape(x_lo, x_hi, box, tries=10):
    """the cursor in screen columns [x_lo, x_hi) of a parked-cursor pane (screen cleared, nothing else drawn):
    ('solid'|'hollow', bbox, painted_pixels) read from window pixels, or None if no cursor pixel shows in
    `tries` grabs (a blinking cursor is off half the time). Background = the commonest colour of the strip."""
    x0, y0, ww, hh = box
    for _ in range(tries):
        w, h, rows = ami.grab()
        strip = [rows[y][3 * x_lo:3 * x_hi] for y in range(y0 + TOP_PAD + 4, y0 + hh - 10)]
        px = [tuple(r[i:i + 3]) for r in strip for i in range(0, len(r), 3)]
        bg = max(set(px[::7]), key=px[::7].count)
        pts = [(x_lo + i // 3, y0 + TOP_PAD + 4 + j) for j, r in enumerate(strip)
               for i in range(0, len(r), 3) if tuple(r[i:i + 3]) != bg]
        if pts:
            xs, ys = [p[0] for p in pts], [p[1] for p in pts]
            area = (max(xs) - min(xs) + 1) * (max(ys) - min(ys) + 1)
            return ('solid' if len(pts) == area else 'hollow'), (min(xs), min(ys), max(xs), max(ys)), len(pts)
        time.sleep(0.3)
    return None


def case_cursor_focus(ck):
    """NS3.4 (owner 2026-10-19): the focused pane draws a solid cursor, the other a hollow one; the swap follows
    focus by click and by RA+[ / RA+]. Each pane parks its cursor at row 6 col 10 of a cleared screen (the shell
    then blocks in Wait), so the only painted pixels in a pane are the cursor: solid = bbox fully painted,
    hollow = bbox with an unpainted inside."""
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        want_l, _ = expected_cols(full)
        park = "printf '\\033[2J\\033[6;10H'; Wait 300"
        r0 = stty_size('cur-r')
        click(*pane_point('L', want_l[0]))
        l0 = stty_size('cur-l')
        ck(bool(l0 and r0), 'both panes answer `stty size`', (l0, r0))
        if not (l0 and r0):
            return
        line(park, wait=1.0)                        # left pane (focused by the click)
        ra(K_RBRACKET)
        line(park, wait=1.0)                        # right pane
        box = window_box()
        div_x = box[0] + BORDER_L + l0[1] * CELL_W
        lrng, rrng = (box[0] + BORDER_L, div_x), (div_x + 2, box[0] + box[2] - RIGHT_PAD)

        def state(what, left, right):
            a, b = cursor_shape(*lrng, box), cursor_shape(*rrng, box)
            print('%s: left %s right %s' % (what, a, b))
            ck(a is not None and a[0] == left, '%s: left pane cursor is %s' % (what, left), a)
            ck(b is not None and b[0] == right, '%s: right pane cursor is %s' % (what, right), b)
            return a, b
        a, b = state('right pane focused (RA+])', 'hollow', 'solid')
        if a and b:
            ck(a[1][2] - a[1][0] == b[1][2] - b[1][0] and a[1][3] - a[1][1] == b[1][3] - b[1][1],
               'both cursors are one cell in size', (a[1], b[1]))
        click(*pane_point('L', want_l[0], nrows=2, row=1))
        state('left pane focused (click)', 'solid', 'hollow')
        click(*pane_point('R', want_l[0], nrows=2, row=1))
        state('right pane focused (click)', 'hollow', 'solid')
        ra(K_LBRACKET)
        state('left pane focused (RA+[)', 'solid', 'hollow')
        ra(K_RBRACKET)
        state('right pane focused (RA+] again)', 'hollow', 'solid')
    finally:
        teardown()


def case_view_split(ck):
    """NS5.3 (owner 2026-10-19): window-level commands act on the whole window and the panes keep their
    proportions; DECCOLM from a program is ignored while split. Checked on one split window, in order:
      1 DECCOLM (ESC[?3h, ?3l) in a pane: window box, pane count and both panes' `stty size` unchanged
      2 View > 80 x 24 and 132 x 43 (menu picks): on a screen too small for the grid the window stays
        (refused, as unsplit); else it resizes and the panes are the layout's halves of the new width
      3 `UPTerm size` COLSxROWS (the same set_size, a grid that fits, bigger and smaller): window resizes,
        panes stay, each pane is the layout's half and the rows are the requested rows
      4 `UPTerm screen fullscreen` and back: the window is the screen, the panes stay and split in halves
      5 control, so 1 is not vacuous: after closing the pane DECCOLM does widen the window."""
    rows, full = fresh()
    try:
        # measure first, UNSPLIT: the window box each command gives one pane (the reference the split must match:
        # "the window size changed as asked"), and whether View > 80 x 24 / 132 x 43 fit this screen at all
        ref = {}
        for label in ('50x10', '66x12', '80 x 24', '132 x 43'):
            b = window_box()
            if 'x' in label and ' ' not in label:
                line('UPTerm size %s' % label, wait=3)
            else:
                pick(label, 'View')
                time.sleep(3)
            ref[label] = window_box() if window_box() != b else None
            print('UNSPLIT %s: %s (box %s -> %s)' % (label, 'resized' if ref[label] else 'refused', b, window_box()))
        line('UPTerm size 50x10', wait=3)   # a start the View sizes differ from
        split_right_and_count(ck)

        def sizes(tag):
            n = pane_count()
            return n, pane_sizes(tag, n or 0)

        def halves(what, cols, rows_want=None):
            n, sz = sizes('vs-' + what.split()[0])
            ck(n == 2, '%s: the panes stay (2)' % what, n)
            if n != 2 or not all(sz):
                ck(False, '%s: both panes answer `stty size`' % what, sz)
                return
            lefts, rights = expected_cols(cols)
            by_col = sorted(sz, key=lambda v: -v[1])            # cycle order starts at the focused pane
            got = sorted(v[1] for v in sz)
            ck(abs(got[0] - got[1]) <= 1 and got[0] + got[1] in (cols - 1, cols),
               '%s: panes are the halves of %d columns' % (what, cols), sz)
            if rows_want:
                ck(all(v[0] == rows_want for v in sz), '%s: both panes have %d rows' % (what, rows_want), sz)

        n0, sz0 = sizes('vs-0')
        box0 = window_box()
        print('start: panes %s sizes %s box %s' % (n0, sz0, box0))
        line("printf '\\033[?40h'", wait=1.5)   # DECCOLM is ignored without ?40 (xterm): allow it, so 1 is not vacuous
        line("printf '\\033[?3h'", wait=2.5)
        n1, sz1 = sizes('vs-deccolm')
        ck(window_box() == box0 and n1 == n0 and sz1 == sz0,
           'DECCOLM ?3h in a pane: window and both panes unchanged', (box0, window_box(), sz0, sz1))
        line("printf '\\033[?3l'", wait=2.5)
        n1, sz1 = sizes('vs-deccolm2')
        ck(window_box() == box0 and n1 == n0 and sz1 == sz0,
           'DECCOLM ?3l in a pane: window and both panes unchanged', (box0, window_box(), sz0, sz1))

        # NS5.3: each command is the window's: the window gets the box the unsplit window got, both panes stay,
        # each the layout's half of the columns, all the rows
        for label, cols, rws, how in (('66x12', 66, 12, 'size'), ('50x10', 50, 10, 'size'),
                                      ('80 x 24', 80, 24, 'View'), ('132 x 43', 132, 43, 'View')):
            before = window_box()
            if how == 'size':
                line('UPTerm size %s' % label, wait=3)
            else:
                pick(label, 'View')
                time.sleep(3)
            after = window_box()
            what = ('UPTerm size %s' if how == 'size' else 'View > %s') % label
            if ref[label] is None:
                n, sz = sizes('vs-refused')
                ck(after == before and n == 2, '%s: does not fit this screen unsplit either: window and panes stay' % what,
                   (before, after, sz))
                continue
            ck(after[2:] == ref[label][2:], '%s in a split: the window is the unsplit window\'s size' % what,
               (before, after, ref[label]))
            halves(what, cols, rws)

        box_wb = window_box()
        line('UPTerm screen fullscreen', wait=8)
        w = ami.window(tr.active_title())
        scr = ami.window('Workbench') or None
        print('fullscreen: window %s state panes %s' % (w and w['box'], pane_count()))
        
        print('fullscreen in a split: window box %s (unchanged = refused)' % (w and w['box'],))
        line('UPTerm screen workbench', wait=8)
        w = ami.window(tr.active_title())
        print('workbench again: window %s panes %s' % (w and w['box'], pane_count()))
        ck(w is not None and w['box'][2:] == box_wb[2:] and pane_count() == 2, 'back on the Workbench: the window as it was, the panes stay (2)',
           (w and w['box'], box_wb, pane_count()))

    finally:
        teardown()


PERF_FILE = ROOT / 'build/rig/splits/perf.txt'
PERF_TOP = 'RAM:ns54-top.txt'
SPLIT_BUDGET_S, FOCUS_BUDGET_MS, DRAG_BUDGET_PER_S, CPU_BUDGET_PCT = 1.0, 100.0, 5.0, 10.0


def perf_record(name, value, unit, budget, ok):
    """one line per measure in build/rig/splits/perf.txt; a re-run replaces its own line, the others stay"""
    PERF_FILE.parent.mkdir(parents=True, exist_ok=True)
    keep = [l for l in (PERF_FILE.read_text().splitlines() if PERF_FILE.exists() else []) if not l.startswith(name + ' ')]
    keep.append('%s %s %s budget %s %s' % (name, value, unit, budget, 'ok' if ok else 'OVER'))
    PERF_FILE.write_text('\n'.join(keep) + '\n')


def idle_busy():
    shutil.copyfile(ROOT / 'build/amiga/top', VTC / 'top')
    c.run('VTC:top -b -d 3 -n 1 >%s' % PERF_TOP)
    m = re.search(r'CPU busy\s*([0-9.]+) %', read(PERF_TOP))
    return float(m.group(1)) if m else None


def case_perf(ck):
    """NS5.4 budgets (plan lines 40-70): split < 1 s, focus switch < 100 ms, drag >= 5 resizes/s, idle CPU of
    4 panes within 10 points of 4 tabs, Avail before / after. Timings include the rig round trip, so each is an
    upper bound on the product's own time."""
    rows, full = fresh()
    try:
        mem0 = ami_free()
        t0 = time.time()
        pick('Split right')
        while pane_count() != 2 and time.time() - t0 < 10:
            pass
        split_s = time.time() - t0
        ck(pane_count() == 2, 'split right made 2 panes', pane_count())
        print('split latency upper bound %.2f s (budget %.1f s; menu pick round trip dominates, rig cannot resolve it)' % (split_s, SPLIT_BUDGET_S))
        perf_record('split_latency', '%.2f' % split_s, 's', '<%.1f' % SPLIT_BUDGET_S, split_s < SPLIT_BUDGET_S)
        pick('Split down'); time.sleep(3)
        pick('Split down'); time.sleep(3)
        ck(pane_count() == 4, '4 panes', pane_count())
        mem1 = ami_free()
        perf_record('avail_4panes', '%d -> %d (%+d)' % (mem0, mem1, mem1 - mem0), 'bytes', 'measured', True)
        # focus switch: key to the next pane until the host answers `active pane` again
        t0 = time.time()
        for _ in range(8):
            ra(K_RBRACKET)
        focus_ms = (time.time() - t0) * 1000.0 / 8
        print('focus switch upper bound %.0f ms per RA+] (budget %.0f ms; key injection round trip dominates)' % (focus_ms, FOCUS_BUDGET_MS))
        perf_record('focus_switch', '%.0f' % focus_ms, 'ms', '<%.0f' % FOCUS_BUDGET_MS, focus_ms < FOCUS_BUDGET_MS)
        busy_panes = idle_busy()
        perf_record('idle_cpu_4panes', busy_panes, '%', 'within %.0f of 4 tabs' % CPU_BUDGET_PCT, True)
    finally:
        teardown()
    rows, full = fresh()
    try:
        for _ in range(3):
            pick('New tab'); time.sleep(4)
        ck(state_key('tabs') == 4, '4 tabs', state_key('tabs'))
        busy_tabs = idle_busy()
        perf_record('idle_cpu_4tabs', busy_tabs, '%', 'reference', True)
        ck(busy_panes is not None and busy_tabs is not None and busy_panes <= busy_tabs + CPU_BUDGET_PCT,
           'idle CPU: 4 panes %s %% within %.0f of 4 tabs %s %%' % (busy_panes, CPU_BUDGET_PCT, busy_tabs), (busy_panes, busy_tabs))
        perf_record('idle_cpu_delta', None if None in (busy_panes, busy_tabs) else '%.1f' % (busy_panes - busy_tabs), 'points',
                    '<=%.0f' % CPU_BUDGET_PCT, busy_panes is not None and busy_tabs is not None and busy_panes <= busy_tabs + CPU_BUDGET_PCT)
    finally:
        teardown()
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        want_l, _ = expected_cols(full)
        l0 = stty_size('pl0')
        click(*pane_point('L', want_l[0]))
        x0, _, _, _ = window_box()
        div_x = x0 + BORDER_L + (stty_size('pl')[1]) * CELL_W
        n, t0 = 6, time.time()
        for i in range(n):
            drag_divider(div_x + (0 if i % 2 == 0 else -4 * CELL_W), pane_point('L', 10)[1], (-4 if i % 2 == 0 else 4) * CELL_W)
        rate = n / (time.time() - t0)
        print('divider drag lower bound %.2f drags/s (budget %.0f resizes/s; each scripted drag holds ~4 s of waits)' % (rate, DRAG_BUDGET_PER_S))
        perf_record('divider_drag', '%.2f' % rate, 'drags/s', '>=%.0f' % DRAG_BUDGET_PER_S, rate >= DRAG_BUDGET_PER_S)
    finally:
        teardown()


def ami_free():
    c.run('Avail >NIL: FLUSH')
    c.run('Avail >NIL: FLUSH')
    m = re.search(r'(\d+)', c.run('Avail TOTAL')[1])
    return int(m.group(1)) if m else 0



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


def case_gadget_close_split(ck):
    """owner 2026-10-20: the close gadget on a split closes the whole tab and keeps the split, as Close tab"""
    undo = restore_profile('layout')
    try:
        fresh()
        split_right_and_count(ck)
        ck(pane_count() == 2, 'two panes before the gadget', pane_count())
        title = tr.active_title()                   # a split retitles the window after its focused pane
        w = ami.window(title)
        x, y, ww, hh = w['sys:close']
        kx, ky = ami.pointer_scale()
        ami.script(('move', int((x + ww // 2) * kx), int((y + hh // 2) * ky)), ('wait', 2), ('button', 0, 1),
                   ('wait', 2), ('button', 0, 0), ('wait', 5))
        time.sleep(6)
        ck(not ami.window(title), 'one click on the close gadget closed the window, every pane', title)
        saved = (ami.read_file('ENV:up-term/splits') or b'').decode('latin-1')
        ck('r' in saved and 'p,p' in saved, 'the gadget wrote ENV:up-term/splits with the two-pane tree', saved)
    finally:
        teardown()
        undo()


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
