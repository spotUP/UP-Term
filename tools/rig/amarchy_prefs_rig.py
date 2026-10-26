#!/usr/bin/env python3
"""amarchy_prefs_rig.py -- R1 of plan 2026-10-26-amarchy-prefs-inline.md: the settings written through the
Amarchy menu reach the files and the handler, in the drawn menu and in the text menu.

NOT RUN when written: the rows stay open until it has run on a rig.

Table driven and resumable like amarchy_menu_rig.py: a verdict per row in <RIG>/amarchy/prefs_<row>.verdict,
--only <row>, --failed, --fresh. Rig 7 only for the agreed run (UPTERM_RIG=7).

Rows
  drawn-blink-off / drawn-blink-on / text-blink-off / text-blink-on
      Super+Space, Settings > General > Cursor blinks, Return (drawn menu: menu-in-terminal = graphical; text
      menu: the default over the terminal). The row starts from the opposite state (the files are written to it
      first), then asserts: ENV:up-term/up-term AND ENVARC:up-term/up-term both change (md5) and both hold
      cursor-blink = <new value>; the handler window's cursor, sampled 12 x 0.25 s before (blinks iff the start
      state is on) and after (blinks iff the new state is on). The *-on row is the "select the row again and
      blinking resumes" half of the plan.
  m6-font
      Settings > General > Font in the drawn menu: the "Choose a font" requester opens on the SAME screen as the
      menu, not on Workbench. When the menu itself is on Workbench the check is trivially true and says so.
  m6-mode
      Settings > Advanced > Screen mode in the text menu: the row's label shows a mode name, it does not match
      0x[0-9A-F]+ (M6b fix lands separately; this row may fail until then).
  theme-name
      Style > Amarchy theme > <n-th theme> in the drawn menu: the theme line of BOTH ENV:Amarchy/prefs and
      ENVARC:Amarchy/prefs names the theme the picked row was labelled with (amarchy_theme_save_rig.py only
      asserts that both changed and agree).

Sentinels
  The reload of the handler has no counter (watch_worker only re-reads ENV:up-term/up-term on notify; a counter
  would be test-only product code). The sentinel is the effect: the cursor toggles between blinking and steady
  only if the handler reloaded the file the menu wrote, so before-blinking -> after-steady -> (next row)
  steady -> blinking again is the call proof. The file sentinel is the md5 of both files plus the key value.
  The drawn menu's rows come from ENV:AMARCHY_ROWS (gui.c sentinel_rows).

Menu path: never a hardcoded Down count. The expected positions are computed from dist/keymap ([menu] order:
top-level groups, then the Settings children) and prefs/settings_def.c (table order of a page's rows, the
Cursor blinks / Font / Screen mode row); the observed rows (sentinel or screen text) give the real position,
and a disagreement in order between the file and the screen is a failure that names both.

Run (rig up: UPTERM_RIG=7 tools/rig/rig.py start --max --purpose "vtcon-a1 Amarchy session: prefs R1 reachability",
Amarchy running, handler + amarchy-menu installed, rig.py install):
  UPTERM_RIG=7 python3 tools/rig/amarchy_prefs_rig.py [--only ROW] [--failed] [--fresh] [--no-build]
Also watch (plan R1, handoff "Watch"): M8 colour chips on a low-colour screen; look at the drawn menu by eye.
"""
import argparse, hashlib, os, pathlib, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami, condev_rig as c
import stage_rig, tabstate
import amarchy_gui_rig as g
import menus_rig as mr
from amarchy_theme_save_rig import wait_stable, step, strangers, stage_themes, theme_line, PREFS, OPEN_SECS
from amarchy_menu_rig import screen_text, menu_rows, x9_open_menu, x9_leave, RAW, KEYMAP

OUT = pathlib.Path(paths.RIG) / 'amarchy'
ROOT = pathlib.Path(__file__).resolve().parents[2]
SETTINGS_DEF = ROOT / 'prefs' / 'settings_def.c'
UPTERM_FILES = ('ENV:up-term/up-term', 'ENVARC:up-term/up-term')
HOST = 'prefshost'
SAMPLES = 12
SAMPLE_GAP = 0.25
FILE_SECS = 20


# ---- the menu path, computed from the files that build the menu -----------------------------------------------
def keymap_menu_ids():
    """The ids of the [menu] section of dist/keymap, in file order."""
    ids, sect = [], ''
    for l in KEYMAP.read_text().splitlines():
        l = l.strip()
        if l.startswith('['):
            sect = l
        elif sect == '[menu]' and l and not l.startswith(';') and '=' in l:
            ids.append(l.split('=')[0].strip())
    return ids


def cap(seg):
    return seg[:1].upper() + seg[1:]


def groups_under(prefix=''):
    """Labels of the groups directly under `prefix` ('' = the top level), in order of first appearance (the
    order mn_children_t lists them: a group appears where its first row is)."""
    out = []
    for i in keymap_menu_ids():
        if prefix:
            if not i.startswith(prefix + '.'):
                continue
            i = i[len(prefix) + 1:]
        seg = i.split('.')[0]
        if seg and cap(seg) not in out:
            out.append(cap(seg))
    return out


def page_rows(page):
    """[(sdi name, plain label)] of the settings table rows on PG_PAGE_<page>, in table order (setting_rows)."""
    text = SETTINGS_DEF.read_text()
    rows = []
    for m in re.finditer(r'\{\s*SDI_(\w+),\s*(?:"[^"]*"|0),\s*(?:"[^"]*"|0),\s*PG_PAGE_(\w+),\s*SD_\w+,\s*"([^"]*)"', text):
        if m.group(2) == page.upper():
            rows.append((m.group(1), m.group(3).replace('_', '')))
    return rows


def expect_order(observed, expected, what):
    """The observed rows that the file knows must come in the file's order (hidden rows, such as Dopus without
    Directory Opus, may be missing). Returns '' or the failure text."""
    known = [r for r in observed if r in expected]
    want = [r for r in expected if r in known]
    return '' if known == want else '%s: screen order %s, file order %s' % (what, known, want)


def find_row(rows, label):
    """Index of the row whose text is `label` or starts with / contains it (a setting row may carry its value)."""
    for i, r in enumerate(rows):
        if r.strip() == label:
            return i
    for i, r in enumerate(rows):
        if label in r:
            return i
    return -1


# ---- reading the menu ------------------------------------------------------------------------------------------
def drawn_rows():
    rc, out = c.run('GetEnv AMARCHY_ROWS')
    return out.strip().split('|') if rc == 0 and out.strip() else []


def drawn_open():
    """Opens the drawn menu and waits for it to settle; returns the root rows."""
    c.run('Delete >NIL: ENV:AMARCHY_ROWS QUIET')
    ami.click_px(100, 100)
    ami.key(g.SPACE, g.LAMIGA)
    wait_stable(OPEN_SECS, what='open drawn menu')
    return drawn_rows()


def drawn_down(rows, label, expected, what):
    """Down to `label` in the level whose rows are `rows`; raises on a file/screen disagreement."""
    bad = expect_order(rows, expected, what)
    if bad:
        raise RuntimeError(bad)
    ix = find_row(rows, label)
    if ix < 0:
        raise RuntimeError('%s: no row "%s" in %s' % (what, label, rows))
    for _ in range(ix):
        step(g.DOWN, 'Down to ' + label)
    return ix


def drawn_enter(what):
    step(g.RETURN, 'enter ' + what)
    return drawn_rows()


def text_open():
    if not x9_open_menu():
        raise RuntimeError('the text menu did not open (no Filter line)')
    return menu_rows(screen_text())


def text_down(rows, label, expected, what):
    bad = expect_order(rows, expected, what)
    if bad:
        raise RuntimeError(bad)
    ix = find_row(rows, label)
    if ix < 0:
        raise RuntimeError('%s: no row "%s" in %s' % (what, label, rows))
    for _ in range(ix):
        ami.key(RAW['down'])
        time.sleep(0.3)
    return ix


def text_enter():
    ami.key(RAW['return'])
    time.sleep(1)
    return menu_rows(screen_text())


def text_closed(secs=20):
    end = time.time() + secs
    while time.time() < end:
        if 'Filter' not in screen_text():
            return True
        time.sleep(0.5)
    return False


# ---- the files and the cursor -----------------------------------------------------------------------------------
def md5(path):
    b = ami.read_file(path)
    return None if b is None else hashlib.md5(b).hexdigest()


def blink_value(path):
    b = ami.read_file(path)
    if b is None:
        return None
    m = re.findall(r'^\s*cursor-blink\s*=\s*(\S+)', b.decode('latin-1'), re.M)
    return m[-1] if m else 'off'          # absent key: the default, off (settings_def.c)


def write_conf(blink, drawn):
    """Both stores start from the same text: the state under test and, for the drawn rows, the setting that
    makes Super+Space over the terminal open the drawn menu."""
    c.run('MakeDir >NIL: ENV:up-term ENVARC:up-term')
    for p in UPTERM_FILES:
        c.run('Echo >%s "[profile default]"' % p)
        c.run('Echo >>%s "cursor-blink = %s"' % (p, blink))
        if drawn:
            c.run('Echo >>%s "menu-in-terminal = graphical"' % p)
    time.sleep(3)                          # the handler reloads on the notify of ENV:up-term/up-term


def host_window():
    """Opens the handler's terminal window (the cursor under test) once; returns its title."""
    import wbrun_rig
    if not any(HOST in (w.get('title') or '') for w in wbrun_rig.windows()):
        tabstate.open_host(HOST)
    for w in wbrun_rig.windows():
        if HOST in (w.get('title') or ''):
            return w['title']
    raise RuntimeError('no window titled %s' % HOST)


def locate_cursor(title):
    mr.TITLE = title                       # menus_rig.box() reads this global
    ami.click_px(100, 100)
    for _ in range(8):
        cur = mr.find_cursor()
        if cur:
            return cur
        time.sleep(0.25)
    return None


def sample(cur):
    seen = set()
    for _ in range(SAMPLES):
        seen.add(mr.pixel(*cur))
        time.sleep(SAMPLE_GAP)
    return seen


def pick_blink(drawn):
    """Super+Space > Settings > General > Cursor blinks > Return. Raises RuntimeError on any path problem."""
    settings_page = groups_under('settings')
    pages = page_rows('GENERAL')
    row_label = dict(pages)['BLINK']
    if drawn:
        rows = drawn_open()
        drawn_down(rows, 'Settings', groups_under(), 'root')
        rows = drawn_enter('Settings')
        drawn_down(rows, 'General', settings_page, 'Settings')
        rows = drawn_enter('General')
        drawn_down(rows, row_label, [l for _, l in pages], 'General')
        ami.key(g.RETURN)
        end = time.time() + 30
        while time.time() < end and g.where(g.TITLE)[1]:
            time.sleep(0.5)
        if g.where(g.TITLE)[1]:
            g.close_menu()
            raise RuntimeError('drawn menu still open 30 s after the final Return')
    else:
        rows = text_open()
        text_down(rows, 'Settings', groups_under(), 'root')
        rows = text_enter()
        text_down(rows, 'General', settings_page, 'Settings')
        rows = text_enter()
        text_down(rows, row_label, [l for _, l in pages], 'General')
        ami.key(RAW['return'])
        if not text_closed():
            x9_leave()
            raise RuntimeError('text menu still open 20 s after the final Return')


def r_blink(drawn, new):
    """One blink row: start from the opposite state, pick the row once, assert files and cursor."""
    old = 'off' if new == 'on' else 'on'
    saved = {p: ami.read_file(p) for p in UPTERM_FILES}
    try:
        title = host_window()
        write_conf(old, drawn)
        before = {p: md5(p) for p in UPTERM_FILES}
        if any(blink_value(p) != old for p in UPTERM_FILES):
            return False, 'could not stage cursor-blink = %s in both files' % old
        cur = locate_cursor(title)
        if not cur:
            return False, 'the cursor of window "%s" was not found before the pick' % title
        s0 = sample(cur)
        if (len(s0) >= 2) != (old == 'on'):
            return False, 'before the pick (cursor-blink = %s) the cursor sampled %d colours (blinking expected: %s)' % (
                old, len(s0), old == 'on')
        allowed = {w['title'] or '' for s in g.screens() for w in s['windows']}
        try:
            pick_blink(drawn)
        except (RuntimeError, TimeoutError) as e:
            return False, str(e)
        end, after = time.time() + FILE_SECS, {}
        while time.time() < end:
            after = {p: md5(p) for p in UPTERM_FILES}
            if all(after[p] != before[p] for p in UPTERM_FILES):
                break
            time.sleep(1)
        changed = [p for p in UPTERM_FILES if after[p] == before[p]]
        if changed:
            return False, 'md5 unchanged in %s after the pick (the menu wrote no file)' % changed
        vals = {p: blink_value(p) for p in UPTERM_FILES}
        if set(vals.values()) != {new}:
            return False, 'cursor-blink in the files is %s, %s expected' % (vals, new)
        time.sleep(3)                      # the handler reloads on the notify
        bad = strangers(allowed)
        if bad:
            return False, 'unexpected windows or requesters: ' + '; '.join(bad)
        cur = locate_cursor(title) or cur
        s1 = sample(cur)
        if (len(s1) >= 2) != (new == 'on'):
            return False, ('files hold cursor-blink = %s but the handler cursor sampled %d colours after the pick '
                           '(blinking expected: %s): the handler did not reload' % (new, len(s1), new == 'on'))
        return True, '%s -> %s: ENV and ENVARC md5 changed, both hold %s; cursor %d colours before, %d after' % (
            old, new, new, len(s0), len(s1))
    finally:
        for p, b in saved.items():
            ami.restore_file(p, b)


def r_drawn_off():  return r_blink(True, 'off')
def r_drawn_on():   return r_blink(True, 'on')
def r_text_off():   return r_blink(False, 'off')
def r_text_on():    return r_blink(False, 'on')


# ---- M6: the font and screen mode requesters ----------------------------------------------------------------------
def r_m6_font():
    saved = {p: ami.read_file(p) for p in UPTERM_FILES}
    try:
        host_window()
        write_conf('off', True)
        pages = page_rows('GENERAL')
        font_label = dict(pages)['FONT']
        rows = drawn_open()
        ms, mw = g.where(g.TITLE)
        if not mw:
            return False, 'the drawn menu did not open'
        menu_screen = ms['title']
        try:
            drawn_down(rows, 'Settings', groups_under(), 'root')
            rows = drawn_enter('Settings')
            drawn_down(rows, 'General', groups_under('settings'), 'Settings')
            rows = drawn_enter('General')
            drawn_down(rows, font_label, [l for _, l in pages], 'General')
        except (RuntimeError, TimeoutError) as e:
            g.close_menu()
            return False, str(e)
        ami.key(g.RETURN)
        found, end = (None, None), time.time() + 30
        while time.time() < end and not found[1]:
            for s in g.screens():
                for w in s['windows']:
                    if (w['title'] or '') == 'Choose a font':
                        found = (s, w)
            time.sleep(0.5)
        if not found[1]:
            g.close_menu()
            return False, 'no "Choose a font" window within 30 s of the Return on %s' % font_label
        req_screen = found[0]['title']
        ami.key(RAW['esc'])
        time.sleep(1)
        if g.where('Choose a font')[1]:
            g.close_window('Choose a font')
        g.close_menu()
        note = ' (the menu is on %s itself, so this proves nothing about the screen choice; run with a public screen in front)' % menu_screen \
            if menu_screen.lower() == 'workbench screen' or menu_screen.lower() == 'workbench' else ''
        if req_screen != menu_screen:
            return False, 'requester on screen "%s", menu on screen "%s"' % (req_screen, menu_screen)
        return True, 'font requester on the menu screen "%s"%s' % (menu_screen, note)
    finally:
        for p, b in saved.items():
            ami.restore_file(p, b)


def r_m6_mode():
    saved = {p: ami.read_file(p) for p in UPTERM_FILES}
    try:
        host_window()
        write_conf('off', False)
        pages = page_rows('ADVANCED')
        mode_label = dict(pages)['SCREEN_MODE']
        try:
            rows = text_open()
            text_down(rows, 'Settings', groups_under(), 'root')
            rows = text_enter()
            text_down(rows, 'Advanced', groups_under('settings'), 'Settings')
            rows = text_enter()
        except (RuntimeError, TimeoutError) as e:
            x9_leave()
            return False, str(e)
        ix = find_row(rows, mode_label)
        text = screen_text()
        line = next((l for l in text.splitlines() if mode_label in l), '')
        x9_leave()
        if ix < 0:
            return False, 'no "%s" row on the Advanced page: %s' % (mode_label, rows)
        if re.search(r'0x[0-9A-Fa-f]+', line):
            return False, 'screen mode row shows a raw id: "%s"' % line.strip()
        return True, 'screen mode row reads "%s"' % line.strip()
    finally:
        for p, b in saved.items():
            ami.restore_file(p, b)


# ---- theme-name: the picked theme is the one both prefs name -----------------------------------------------------
THEME_PICK = 2        # the third theme of the list; the theme-save rig walks 1-5 Downs from the list's first row


def r_theme_name():
    saved = {p: ami.read_file(p) for p in PREFS}
    try:
        stage_themes()
        if not g.process_running('Amarchy'):
            return False, 'the Amarchy commodity is not running'
        g.close_menu()
        allowed = {w['title'] or '' for s in g.screens() for w in s['windows']}
        before = {p: theme_line(p) for p in PREFS}
        try:
            rows = drawn_open()
            ix = drawn_down(rows, 'Style', groups_under(), 'root')
            rows = drawn_enter('Style')
            if find_row(rows, 'Amarchy theme') != 0:
                g.close_menu()
                return False, 'Amarchy theme is not the first row of Style: %s' % rows
            names = drawn_enter('Amarchy theme')
            if len(names) <= THEME_PICK:
                g.close_menu()
                return False, 'the theme list has %d rows: %s' % (len(names), names)
            for _ in range(THEME_PICK):
                step(g.DOWN, 'Down in theme list')
            picked = names[THEME_PICK].strip()
            ami.key(g.RETURN)
        except (RuntimeError, TimeoutError) as e:
            g.close_menu()
            return False, str(e)
        end = time.time() + 30
        while time.time() < end and g.where(g.TITLE)[1]:
            time.sleep(0.5)
        if g.where(g.TITLE)[1]:
            g.close_menu()
            return False, 'menu still open 30 s after the final Return'
        after = {}
        for _ in range(10):
            time.sleep(1)
            after = {p: theme_line(p) for p in PREFS}
            if all(after[p] and after[p] != before[p] for p in PREFS):
                break
        bad = strangers(allowed)
        if bad:
            return False, 'unexpected: ' + '; '.join(bad)
        want = re.sub(r'\.theme$', '', picked).strip().lower().replace(' ', '-')
        wrong = [p for p in PREFS if not after[p] or after[p].lower() != want]
        if wrong:
            return False, 'picked row "%s" (theme %s) but %s' % (picked, want, {p: after[p] for p in PREFS})
        return True, 'picked "%s": ENV and ENVARC Amarchy/prefs both name %s (was %s)' % (picked, want, before['ENV:Amarchy/prefs'])
    finally:
        for p, b in saved.items():
            ami.restore_file(p, b)


ROWS = [
    ('drawn-blink-off', r_drawn_off),
    ('drawn-blink-on',  r_drawn_on),
    ('text-blink-off',  r_text_off),
    ('text-blink-on',   r_text_on),
    ('m6-font',         r_m6_font),
    ('m6-mode',         r_m6_mode),
    ('theme-name',      r_theme_name),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--only', choices=[n for n, _ in ROWS])
    ap.add_argument('--failed', action='store_true', help='re-run the rows that did not pass; keep the passed ones')
    ap.add_argument('--fresh', action='store_true', help='ignore recorded verdicts')
    ap.add_argument('--no-build', action='store_true', help='stage what build/amiga holds, do not run make')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    stage_rig.report(stage_rig.stage_build(build=not a.no_build))
    if not g.process_running('Amarchy'):      # the hotkey commodity; the rig boots without it (amarchy_gui_rig starts it the same way)
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        time.sleep(3)
    passed = total = 0
    for name, fn in ROWS:
        f = OUT / ('prefs_' + name + '.verdict')
        if a.only and a.only != name:
            continue
        if f.exists() and not a.fresh and not a.only:
            old = f.read_text()
            if old.startswith('ok') or not a.failed:
                total += 1
                passed += old.startswith('ok')
                continue
        try:
            ok, detail = fn()
        except Exception as e:             # a row that dies is a FAIL with its error, not a dead run
            ok, detail = False, 'exception %s: %s' % (type(e).__name__, e)
        f.write_text('%s %s\n' % ('ok' if ok else 'FAIL', detail))
        total += 1
        passed += ok
        print('%s %s: %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    print('amarchy_prefs_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
