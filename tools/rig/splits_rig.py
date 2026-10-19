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
  pane_arrows RA+Left / RA+Right move focus between the panes (typed text lands in the focused pane)
  zoom      RA+Shift+Enter zooms the focused pane to full width, again restores the half width
  race      Split down from both panes back to back after a `cd` to two different dirs; every pane's `pwd`
            is the dir of the pane it was split from (the shared T:UP-Term-tab script race)

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
CASES = ['sentinel', 'size', 'focus', 'paint', 'race', 'winch', 'pane_close', 'pane_arrows', 'zoom']
RET, CTRL = typewin.RET, typewin.CTRL
SHIFT, RAMIGA = typewin.SHIFT, typewin.RAMIGA
K_W, K_D, K_ENTER, K_RIGHT, K_LEFT = 0x11, 0x22, 0x44, 0x4E, 0x4F    # raw keycodes (RA+key = pane commands)
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
    """layout.c: avail = W - 1 (pixel divider); first = round(avail/2 / cell) cells; the rest is the second"""
    W = full_cols * cell
    avail = W - 1
    first = ((avail // 2 + cell // 2) // cell) * cell     # ratio 1/2, snapped to whole cells (first_ext)
    return first // cell, (avail - first) // cell


def pane_point(which, cols_left, nrows=1, row=0):
    """screen pixel inside a pane: which 'L' or 'R' of a two-column layout; row/nrows picks the band"""
    x0, y0, ww, hh = window_box()
    left_px = cols_left * CELL_W
    inner_top, inner_h = y0 + TOP_PAD + 16, hh - TOP_PAD - 16 - 6       # below the title bar and the tab bar row
    x = x0 + BORDER_L + (left_px // 2 if which == 'L' else left_px + 1 + (ww - BORDER_L - RIGHT_PAD - left_px - 1) // 2)
    y = inner_top + inner_h * (2 * row + 1) // (2 * nrows)
    return x, y


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
        click(*pane_point('L', want_l))
        left = stty_size('left')
        ck(left is not None, 'left pane answers `stty size` (click focus, NS3.4)', left)
        print('measured: full %dx%d, left %s, right %s, expected cols L=%d R=%d (cell %d)'
              % (rows, full, left, right, want_l, want_r, CELL_W))
        if left and right:
            ck(left[1] == want_l and right[1] == want_r,
               'cols match the layout: left %d right %d' % (want_l, want_r), (left[1], right[1]))
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
        ck(len(who) == 2 and int(who[1]) == expected_cols(full)[1],
           'the line typed after the split ran in the new (right) pane: its width', who)
        text = typewin.screen_text()
        ck(marker in text, 'marker is in the focused pane\'s text', text[-200:])
        # the host's AmigaDOS-or-vsh left pane got nothing: a line goes to one pane only, so the proof is the
        # right-pane width in `who`; reading the left pane's text needs its focus (click), then the marker is absent
        click(*pane_point('L', want_l))
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
        line(stream_cmd(44, 'RIGHT', want_r - 1), wait=0.5)     # the new (right) pane has the keys: blue
        click(*pane_point('L', want_l))
        line(stream_cmd(41, 'LEFT', want_l - 1), wait=0.5)
        box = window_box()
        bound = box[0] + BORDER_L + want_l * CELL_W              # the divider column; the right pane starts one past
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
        click(*pane_point('L', want_l))
        pick('Split down')
        click(*pane_point('R', want_l))
        pick('Split down')
        time.sleep(8)
        ck(tr.counters()[0].get('splits', 0) >= 3, 'three splits counted', tr.counters()[0].get('splits'))
        # P0 top-left, P2 bottom-left (split from P0), P1 top-right, P3 bottom-right (split from P1)
        probes = [('P0', 'L', 0, 'A'), ('P2', 'L', 1, 'A'), ('P1', 'R', 0, 'B'), ('P3', 'R', 1, 'B')]
        for name, side, row, src in probes:
            click(*pane_point(side, want_l, nrows=2, row=row))
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
    """panes of the active tab: `UPTerm state` key "panes" if the handler reports it (UNVERIFIED key name), else None"""
    return tr.counters()[0].get('panes')


def case_winch(ck):
    rows, full = fresh()
    try:
        split_right_and_count(ck)
        w0 = window_box()
        r0 = stty_size('right0')
        want_l, _ = expected_cols(full)
        click(*pane_point('L', want_l))
        l0 = stty_size('left0')
        ck(bool(l0 and r0), 'both panes answer `stty size` before the resize', (l0, r0))
        # resize mechanism: size-gadget drag (as winch_rig); shrink by 160 px = 20 cells
        drag_size_gadget(-160, 0)
        w1 = window_box()
        ck(bool(w0 and w1 and w1[2] < w0[2]), 'window got narrower', (w0, w1))
        new_full = full + (w1[2] - w0[2]) // CELL_W if w0 and w1 else full
        want_l1, want_r1 = expected_cols(new_full)
        r1 = stty_size('right1')
        click(*pane_point('L', want_l1))
        l1 = stty_size('left1')
        print('measured: before L %s R %s, after L %s R %s, expected after L=%d R=%d' % (l0, r0, l1, r1, want_l1, want_r1))
        if l0 and r0 and l1 and r1:
            ck(l1[1] != l0[1] and r1[1] != r0[1], 'cols of both panes changed', (l0, r0, l1, r1))
            ck(l1[1] == want_l1 and r1[1] == want_r1, 'cols are the halves of the new width: L %d R %d' % (want_l1, want_r1),
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
            if n is not None:
                ck(n == 1, '%s: `UPTerm state` panes == 1' % how, n)
    finally:
        teardown()


def case_pane_arrows(ck):
    rows, full = fresh()
    want_l, want_r = expected_cols(full)
    try:
        split_right_and_count(ck)                  # right pane focused
        r = stty_size('right')
        ck(bool(r) and r[1] == want_r, 'new (right) pane has focus', r)
        ra(K_LEFT)
        l = stty_size('left')
        ck(bool(l) and l[1] == want_l, 'RA+Left: typed text runs in the left pane', l)
        ra(K_RIGHT)
        r2 = stty_size('right2')
        ck(bool(r2) and r2[1] == want_r, 'RA+Right: typed text runs in the right pane again', r2)
        if want_l == want_r:                       # widths cannot tell the panes apart: use the typed marker too
            marker = 'NSARW%d' % int(time.time() % 100000)
            ra(K_LEFT)
            line('echo %s' % marker)
            ck(marker in typewin.screen_text(), 'marker is on the left pane\'s text')
    finally:
        teardown()


def case_zoom(ck):
    rows, full = fresh()
    want_l, want_r = expected_cols(full)
    try:
        split_right_and_count(ck)
        half = stty_size('half')
        ck(bool(half) and half[1] == want_r, 'split: focused pane is half width', half)
        ra(K_ENTER, shift=True)
        z = stty_size('zoom')
        ck(bool(z) and z[1] == full, 'RA+Shift+Enter: zoomed pane reports the unsplit width %d' % full, z)
        ra(K_ENTER, shift=True)
        back = stty_size('back')
        ck(bool(back) and back[1] == want_r, 'RA+Shift+Enter again: half width %d restored' % want_r, back)
    finally:
        teardown()


# ---- driver -----------------------------------------------------------------------------------

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
