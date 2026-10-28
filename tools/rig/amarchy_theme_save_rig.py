#!/usr/bin/env python3
"""Amarchy theme save rig: the drawn menu (Left Amiga + Space) > Style > Amarchy theme > Down n > Return must
rewrite the theme line of BOTH ENV:Amarchy/prefs and ENVARC:Amarchy/prefs to the picked theme, and switch live:
the menu stays open, recolours in the picked theme (AMARCHY_THEME) and marks it (current) with the cursor on it.
The header text is drawn in the picked theme's accent (menu_theme.h MT_HEAD_ROLE; owner 2026-10-28), read from the
window's pixels: accent pixels in rows that are not mostly accent (the border and the selected row's box are).

Every key waits for what it acts on: the menu window must exist and its box AND pixels must be identical on two
reads before the first key (the drawn menu takes seconds to open and resizes while opening); after each submenu
step the script waits for the level change (box or pixels differ from the level before) and then for it to be stable
again. No fixed sleeps stand in for a state. The stable-box wait per open is printed.

Per select: theme line of both prefs files before/after, both must name the picked theme; any requester or window
other than the menu and the host terminal that appears is a failure (its text quoted). Stages UP-Term:Amarchy/Themes
from dist/amarchy-themes when the rig has fewer than 6 themes there.

Rig up (UPTERM_RIG=<n> tools/rig/rig.py start --max), handler + Amarchy commodity running with the binaries under
test. Run: UPTERM_RIG=7 python3 tools/rig/amarchy_theme_save_rig.py [--selects 1-5] [--no-build]
Exit status 1 on any [FAIL]."""
import os, re, sys, time, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_gui_rig as g
from amarchy_menu_rig import THEME_DIR

STYLE_DOWNS = 3          # Style is the 4th entry of the top level: Down x3, Return
NEED_THEMES = 6
STABLE_READS = 2
POLL = 0.5
OPEN_SECS = 60
STEP_SECS = 30
SHIPPED = 'UP-Term:Amarchy/Themes'
PREFS = ('ENV:Amarchy/prefs', 'ENVARC:Amarchy/prefs')


def menu_state():
    """(box, pixel digest) of the menu window; (None, None) when it is not there."""
    s, w = g.where(g.TITLE)
    if not w:
        return None, None
    box = g.box_of(s, w)
    left, top, ww, hh = box
    _, _, rows = ami.grab((left, top, ww, hh))
    return box, hashlib.md5(b''.join(rows)).hexdigest()


def wait_stable(secs, differ_from=None, what='menu'):
    """Wait until the menu state is identical on STABLE_READS consecutive reads (and, with `differ_from`, differs
    from it). Returns (state, seconds waited); raises TimeoutError naming `what`."""
    t0 = time.time()
    last, same = None, 0
    while time.time() - t0 < secs:
        st = menu_state()
        if st[0] is None:
            last, same = None, 0
        elif differ_from is not None and st == differ_from:
            last, same = None, 0
        elif st == last:
            same += 1
            if same >= STABLE_READS - 1:
                return st, time.time() - t0
        else:
            last, same = st, 0
        time.sleep(POLL)
    raise TimeoutError('%s: no stable state within %ds (last %s)' % (what, secs, last and last[0]))


def step(key, what):
    """One key, then wait for the level to change and settle."""
    before = menu_state()
    ami.key(key)
    st, dt = wait_stable(STEP_SECS, differ_from=before, what=what)
    return st, dt


def theme_line(path):
    rc, out = c.run('Type %s' % path)
    if rc != 0:
        return None
    m = re.search(r'^\s*amarchy-gui-theme\s*=\s*(\S+)', out, re.M)
    return m.group(1) if m else ''


def stage_themes():
    rc, out = c.run('List %s LFORMAT=%%n' % SHIPPED)
    have = [l for l in out.splitlines() if l.strip().endswith('.theme')] if rc == 0 else []
    if len(have) >= NEED_THEMES:
        print('[INFO] %s holds %d themes' % (SHIPPED, len(have)))
        return
    c.run('MakeDir >NIL: UP-Term:Amarchy %s' % SHIPPED)
    files = sorted(THEME_DIR.glob('*.theme'))
    for f in files:
        ami.put(str(f), '%s/%s' % (SHIPPED, f.name))
    print('[INFO] staged %d themes from %s into %s' % (len(files), THEME_DIR, SHIPPED))


def strangers(allowed):
    """Requesters and windows on any screen that are neither the menu nor in `allowed` (titles) -> list of text."""
    out = [ami.describe_requester(r) for r in ami.requesters()]
    for s in g.screens():
        for w in s['windows']:
            t = w['title'] or ''
            if t != g.TITLE and t not in allowed:
                out.append('window "%s" on screen "%s"' % (t, s['title']))
    return out


def env(name):
    rc, out = c.run('GetEnv %s' % name)
    return out.strip() if rc == 0 else ''


def theme_accent(name):
    """The accent of the shipped theme `name` as an (r, g, b) tuple, from dist/amarchy-themes."""
    t = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'dist', 'amarchy-themes', name + '.theme')
    with open(t, encoding='latin-1') as f:
        m = re.search(r'^accent\s*=\s*"#([0-9a-fA-F]{6})"', f.read(), re.M)
    v = int(m.group(1), 16)
    return v >> 16, (v >> 8) & 255, v & 255


def header_accent_pixels(rgb):
    """Pixels of the header text drawn in colour `rgb`: in the open menu, outside the rows that are mostly `rgb`
    (the top and bottom border and the selected row's box), between the side borders. The text is anti-aliased,
    so a thin stroke seldom reaches full coverage: a pixel counts when it lies on the blend from the menu
    background (the commonest colour) to `rgb` at half coverage or more. Grey pens nearest the theme are off
    that line, so they do not count."""
    s, w = g.where(g.TITLE)
    left, top, ww, hh = g.box_of(s, w)
    _, _, rows = ami.grab((left, top, ww, hh))
    x0, x1 = ww * 5 // 100, ww * 95 // 100
    seen = {}
    for r in rows:
        for x in range(x0, x1):
            k = bytes(r[3 * x:3 * x + 3])
            seen[k] = seen.get(k, 0) + 1
    bg = max((k for k in seen if k != bytes(rgb)), key=seen.get)
    d = [rgb[i] - bg[i] for i in range(3)]
    dd = sum(v * v for v in d) or 1
    n = 0
    for r in rows:
        px = [bytes(r[3 * x:3 * x + 3]) for x in range(x0, x1)]
        if px.count(bytes(rgb)) * 2 >= x1 - x0:
            continue
        for p in px:
            t = sum((p[i] - bg[i]) * d[i] for i in range(3)) / dd
            if t >= 0.5 and all(abs(p[i] - bg[i] - t * d[i]) <= 12 for i in range(3)):
                n += 1
    return n


def one_select(n, allowed):
    """Returns (ok, detail, open_wait_seconds). The pick switches live (owner, 2026-10-28): the menu stays open on
    the theme list, AMARCHY_THEME (set where gui.c pens_get takes the colours) names the picked theme, the picked
    row reads "(current)" and the cursor stays on it."""
    before = {p: theme_line(p) for p in PREFS}
    ami.key(g.SPACE, g.LAMIGA)
    try:
        st, open_wait = wait_stable(OPEN_SECS, what='open menu')
        for _ in range(STYLE_DOWNS):
            st, _ = step(g.DOWN, 'Down to Style')
        step(g.RETURN, 'enter Style')
        step(g.RETURN, 'enter Amarchy theme')
        for _ in range(n):
            st, _ = step(g.DOWN, 'Down in theme list')
        rows_before, sel_before = env('AMARCHY_ROWS').split('|'), env('AMARCHY_SEL')
        ami.key(g.RETURN)
        wait_stable(STEP_SECS, differ_from=st, what='recoloured theme list')
    except TimeoutError as e:
        if not g.where(g.TITLE)[1]:
            return False, 'the menu closed on the pick (it must stay open and recolour)', 0.0
        g.close_menu()
        return False, str(e), 0.0
    if not g.where(g.TITLE)[1]:
        return False, 'the menu closed on the pick (it must stay open and recolour)', open_wait
    after = {}
    for _ in range(10):
        after = {p: theme_line(p) for p in PREFS}
        if all(after[p] != before[p] for p in PREFS):
            break
        time.sleep(1)
    drew, rows_after, sel_after = env('AMARCHY_THEME'), env('AMARCHY_ROWS').split('|'), env('AMARCHY_SEL')
    head = header_accent_pixels(theme_accent(drew)) if drew else 0
    g.close_menu()
    bad = strangers(allowed)
    if bad:
        return False, 'unexpected: ' + '; '.join(bad), open_wait
    picked = {after[p] for p in PREFS}
    if len(picked) != 1 or None in picked or '' in picked:
        return False, 'ENV/ENVARC differ or unreadable: %s' % after, open_wait
    unchanged = [p for p in PREFS if after[p] == before[p]]
    if unchanged:
        return False, 'theme line unchanged in %s (before %s, after %s)' % (unchanged, before, after), open_wait
    name = picked.pop()
    if drew != name:
        return False, 'the open menu drew theme %r after picking %r (AMARCHY_THEME)' % (drew, name), open_wait
    want = name + ' (current)'
    at = [i for i, r in enumerate(rows_before) if r.split(' (')[0] == name]
    if want not in rows_after or not at or rows_after.index(want) != at[0]:
        return False, 'rows after the pick do not mark %r: %s' % (want, rows_after), open_wait
    if sel_after != sel_before:
        return False, 'cursor moved from row %s to %s' % (sel_before, sel_after), open_wait
    if head < 10:
        return False, 'the header is not drawn in %s\'s accent (%d accent pixels outside border and selection)' % (
            name, head), open_wait
    return True, 'theme %s -> %s live (ENV, ENVARC, AMARCHY_THEME, rows, header accent %d px)' % (before['ENV:Amarchy/prefs'], name, head), open_wait


def main():
    lo, hi = 1, 5
    if '--selects' in sys.argv:
        lo, hi = map(int, sys.argv[sys.argv.index('--selects') + 1].split('-'))
    stage_themes()
    if not g.process_running('Amarchy'):
        print('[FAIL] the Amarchy commodity is not running')
        return 1
    g.close_menu()
    allowed = {w['title'] or '' for s in g.screens() for w in s['windows']}
    results, waits = [], []
    for n in range(lo, hi + 1):
        ok, detail, wt = one_select(n, allowed)
        results.append(ok)
        waits.append(wt)
        print('%s select %d: %s (stable-box wait %.1f s)' % ('[OK]' if ok else '[FAIL]', n, detail, wt), flush=True)
    good = sum(results)
    print('%s %d of %d selects; stable-box waits (s): %s' % ('[OK]' if good == len(results) else '[FAIL]', good,
                                                          len(results), ' '.join('%.1f' % w for w in waits)))
    return 0 if good == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
