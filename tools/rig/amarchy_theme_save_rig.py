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
test. Run: UPTERM_RIG=7 python3 tools/rig/amarchy_theme_save_rig.py [--selects 1-5] [--preview <downs>] [--no-build]
--preview walks the theme list without picking (one_preview) instead of the selects.
Exit status 1 on any [FAIL]."""
import os, re, struct, sys, time, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_gui_rig as g

STYLE_DOWNS = 3          # fallback only: Style is the 4th entry of the 3.1 top level; style_downs() reads the live menu env
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


def shipped_themes():
    """Themes the rig boot copied from VTC:amarchy-themes (rig.py stages them at install)."""
    rc, out = c.run('List %s LFORMAT=%%n' % SHIPPED)
    return [l for l in out.splitlines() if l.strip().endswith('.theme')] if rc == 0 else []


def stem(title):
    """A window title without its live numbers: a Directory Opus lister retitles itself with free memory
    ("Ram Disk,  1,023.3M free, ..."), so only the part before the first comma names the window."""
    return title.split(',')[0].strip()


def strangers(allowed):
    """Requesters and windows on any screen that are neither the menu nor in `allowed` (titles) -> list of text."""
    out = [ami.describe_requester(r) for r in ami.requesters()]
    for s in g.screens():
        for w in s['windows']:
            t = w['title'] or ''
            if t != g.TITLE and stem(t) not in {stem(a) for a in allowed}:
                out.append('window "%s" on screen "%s"' % (t, s['title']))
    return out


def style_downs():
    """Downs from the cursor to the Style row, read from the menu env (the top level differs per rig); STYLE_DOWNS if unreadable."""
    top = env('AMARCHY_ROWS').split('|')
    if 'Style' not in top:
        return STYLE_DOWNS
    return top.index('Style') - int(env('AMARCHY_SEL') or 0)


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


FILE_KEY = {'bg': 'background', 'fg': 'foreground', 'dim': 'dark_foreground', 'accent': 'accent'}


def theme_hex(name, key):
    """The colour `key` (bg, fg, dim, accent) of the shipped theme `name` as 'rrggbb'."""
    t = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'dist', 'amarchy-themes', name + '.theme')
    with open(t, encoding='latin-1') as f:
        m = re.search(r'^%s\s*=\s*"#([0-9a-fA-F]{6})"' % FILE_KEY[key], f.read(), re.M)
    # gthemes_core.c: a file without dark_foreground gets the built-in dim
    return m.group(1).lower() if m else '565f89'


def workbench_cols():
    """Palette entries 0-3 of the Workbench screen (VTCX:Apps/bin/dripens 'cols:' line, put there by hand), as 'rrggbb' strings."""
    for l in c.run('VTCX:Apps/bin/dripens Workbench')[1].splitlines():
        if l.startswith('cols:'):
            return [x.lower() for x in l.split()[1:]]
    return []


def dopus_reply(cmd):
    """The ARexx reply of one Directory Opus command, or None when the port is absent."""
    b = ami.req(0x12, ("OPTIONS RESULTS; ADDRESS DOPUS.1 '%s'; RETURN result" % cmd).encode('latin-1'))
    res = b[4:].decode('latin-1').strip()
    return res if struct.unpack('>I', b[:4])[0] == 0 and res != 'RESULT' else None


def dopus_pens_reset():
    """Set every lister pen to a value no theme sends, so a later read-back proves the theme switch wrote them."""
    for key in ('files', 'dirs', 'selfiles', 'seldirs'):
        dopus_reply('dopus set pens %s 2 2' % key)


def wb_pens_check(name):
    """The pen apply after the pick (D14 + Workbench pens, owner 2026-10-29): the sentinel counted it, Workbench
    colours 0-3 are the theme's bg/fg/dim/accent, and the Directory Opus listers read back the theme pens.
    Returns a failure text or None."""
    got, refused = env('AMARCHY_WBPENS'), env('AMARCHY_WBREFUSED')
    print('  [INFO] %s: AMARCHY_WBPENS=%s AMARCHY_WBREFUSED=%s' % (name, got or '-', refused or '-'))
    if not got or int(got) < 1:
        return 'AMARCHY_WBPENS did not rise after picking %s (refused=%s)' % (name, refused or '-')
    want = [theme_hex(name, k) for k in ('bg', 'fg', 'dim', 'accent')]
    cols = workbench_cols()
    print('  [INFO] %s: Workbench colours 0-3 %s, theme %s' % (name, cols, want))
    if cols != want:
        return 'Workbench colours 0-3 %s are not the theme\'s %s' % (cols, want)
    for key, pens in (('files', '1 0'), ('dirs', '3 0'), ('selfiles', '0 3'), ('seldirs', '0 3')):
        r = dopus_reply('dopus query pens %s' % key)
        print('  [INFO] %s: dopus query pens %s = %r' % (name, key, r))
        if r is None or r.split() != pens.split():
            return 'dopus query pens %s reads %r, want %r' % (key, r, pens)
    return None


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


def walk_key(rows, at, steps):
    """The list opens with the cursor on the current theme (row `at` of `rows`), which may sit near the end: walk
    towards the side with room for every step. Returns (key, word)."""
    return (g.DOWN, 'Down') if at + steps < len(rows) else (g.UP, 'Up')


def open_menu():
    """The hotkey with the desktop active: over a full-width terminal (the condev UP-Term window) the commodity
    opens the text menu inside that terminal instead (cxdesc.c cx_menu_style), so a click on the empty desktop
    below it comes first, as the demo recording does."""
    ami.click_px(400, 560)
    time.sleep(1)
    ami.key(g.SPACE, g.LAMIGA)


def one_select(n, allowed):
    """Returns (ok, detail, open_wait_seconds). The pick switches live (owner, 2026-10-28): the menu stays open on
    the theme list, AMARCHY_THEME (set where gui.c pens_get takes the colours) names the picked theme, the picked
    row reads "(current)" and the cursor stays on it."""
    before = {p: theme_line(p) for p in PREFS}
    dopus_pens_reset()
    open_menu()
    try:
        st, open_wait = wait_stable(OPEN_SECS, what='open menu')
        for _ in range(style_downs()):
            st, _ = step(g.DOWN, 'Down to Style')
        step(g.RETURN, 'enter Style')
        step(g.RETURN, 'enter Amarchy theme')
        rows = env('AMARCHY_ROWS').split('|')
        key, word = walk_key(rows, next((i for i, r in enumerate(rows) if r.endswith(' (current)')), 0), n)
        for _ in range(n):
            st, _ = step(key, '%s in theme list' % word)
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
    bad = wb_pens_check(name)
    if bad:
        return False, bad, open_wait
    return True, 'theme %s -> %s live (ENV, ENVARC, AMARCHY_THEME, rows, header accent %d px)' % (before['ENV:Amarchy/prefs'], name, head), open_wait


def one_preview(steps, allowed):
    """Preview on cursor move (owner, 2026-10-28): each cursor move in Style > Amarchy theme recolours the open menu in
    the theme under the cursor (AMARCHY_THEME names it, the header is in its accent) without saving it (both prefs
    lines unchanged); Esc out of the list draws the saved theme again. Returns (ok, detail)."""
    before = {p: theme_line(p) for p in PREFS}
    open_menu()
    seen = []
    try:
        st, _ = wait_stable(OPEN_SECS, what='open menu')
        for _ in range(style_downs()):
            st, _ = step(g.DOWN, 'Down to Style')
        step(g.RETURN, 'enter Style')
        st, _ = step(g.RETURN, 'enter Amarchy theme')
        saved = env('AMARCHY_THEME')
        rows = env('AMARCHY_ROWS').split('|')
        cur = [i for i, r in enumerate(rows) if r.endswith(' (current)')]
        if not cur or rows[cur[0]].split(' (')[0] != saved:
            g.close_menu()
            return False, 'the theme list does not mark the drawn theme %r (current): %s' % (saved, rows)
        shift = int(env('AMARCHY_SEL')) - cur[0]
        key, word = walk_key(rows, cur[0], steps)
        for _ in range(steps):
            st, _ = step(key, '%s in theme list' % word)
            drew, sel = env('AMARCHY_THEME'), int(env('AMARCHY_SEL'))
            under = rows[sel - shift].split(' (')[0] if 0 <= sel - shift < len(rows) else ''
            seen.append((under, drew, header_accent_pixels(theme_accent(drew)) if drew else 0))
        st, _ = step(g.ESC, 'Esc out of the theme list')
        back, back_head = env('AMARCHY_THEME'), header_accent_pixels(theme_accent(saved))
    except TimeoutError as e:
        g.close_menu()
        return False, str(e)
    g.close_menu()
    after = {p: theme_line(p) for p in PREFS}
    bad = strangers(allowed)
    if bad:
        return False, 'unexpected: ' + '; '.join(bad)
    for under, drew, head in seen:
        if drew != under:
            return False, 'cursor on %r but the menu drew %r (AMARCHY_THEME); walk %s' % (under, drew, seen)
        if head < 10:
            return False, 'previewing %r: header has %d accent pixels' % (drew, head)
    if after != before:
        return False, 'the preview saved a theme: before %s, after %s' % (before, after)
    if back != saved or back_head < 10:
        return False, 'Esc drew %r (header %d px), want the saved %r' % (back, back_head, saved)
    return True, 'walked %s, Esc restored %s (header %d px), prefs unchanged' % (
        ' '.join('%s/%dpx' % (d, h) for _, d, h in seen), saved, back_head)


def main():
    lo, hi = 1, 5
    if '--selects' in sys.argv:
        lo, hi = map(int, sys.argv[sys.argv.index('--selects') + 1].split('-'))
    have = shipped_themes()
    if len(have) < NEED_THEMES:
        print('[FAIL] %s holds %d themes, need %d: rebuild the rig kit (rig.py stages '
              'dist/amarchy-themes at install, the boot copies them)' % (SHIPPED, len(have), NEED_THEMES))
        return 1
    print('[INFO] %s holds %d themes' % (SHIPPED, len(have)))
    if not g.process_running('Amarchy'):
        print('[FAIL] the Amarchy commodity is not running')
        return 1
    g.close_menu()
    allowed = {w['title'] or '' for s in g.screens() for w in s['windows']}
    if '--preview' in sys.argv:
        ok, detail = one_preview(int(sys.argv[sys.argv.index('--preview') + 1]), allowed)
        print('%s preview: %s' % ('[OK]' if ok else '[FAIL]', detail), flush=True)
        return 0 if ok else 1
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
