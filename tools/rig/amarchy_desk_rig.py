#!/usr/bin/env python3
"""Amarchy native desktop theme rig (plan 2026-10-29-amarchy-p1a-native-theme.md, RX1 first steps).

Real entry point: the drawn menu (Left Amiga + Space) > Style > Amarchy theme > Return, the path that runs
gt_dos_select (there is no `desk` ARexx verb or hotkey action yet: R1.12). The steps:
  1. Baseline: ENV:Sys and ENVARC:Sys prefs bytes, Workbench colours 0-3 (T:wbpens).
  2. Master switch off (the default): pick a theme. AMARCHY_DESKS must not rise, no desktop prefs file may change,
     T:amarchy-revert must not appear.
  3. Master switch on (desktop-theme = on in ENV:Amarchy/prefs): pick another theme. AMARCHY_DESKS +1, AMARCHY_DESKMASK 15;
     the THEMED FIELDS of ENV:Sys palette, wbpattern, pointer, font equal those of the bytes desk_core makes for that theme
     (tools/rig/desk_expect.c, built on the host from the same sources, prints the fields of both files); Workbench colours 0-3 changed; T:amarchy-revert holds the step-1 bytes of every
     file that existed; ENVARC:Sys unchanged (persist is R1.10).
The stock editors rewrite ENV:Sys files after USE in their own layout (the plan: USE writes ROOT, DRAWER and SCREEN always),
so the files settle first and only the themed fields are compared; byte exactness of desk_core stays with tests/test_desk.c.
A differing field prints its name, the expected value and the got value.

Rig up on AmigaOS 3.2 (owner decision 2026-10-29; 3.1 has no stock Pointer/Wbpattern editors the steps need):
  UPTERM_RIG=<n> tools/rig/rig.py start --max --os 32 --purpose "<session>: desk theme"   (tree build/rig/os-322)
then stage and start this branch's Amarchy (the 3.2 kit does not auto-start it; on 3.1 too):
  python3 tools/rig/amarchy_gui_rig.py --setup
Delete ENV:Sys/{palette,wbpattern,font,pointer}.prefs and ENV:Amarchy/prefs between runs for a clean baseline.
Run: UPTERM_RIG=3 python3 tools/rig/amarchy_desk_rig.py
Exit status 1 on any [FAIL]."""
import os, re, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
sys.path.insert(0, HERE)
import ami, condev_rig as c
import amarchy_gui_rig as g
import amarchy_theme_save_rig as t

SYS_FILES = ['palette', 'wbpattern', 'pointer', 'font']
PREFS = 'ENV:Amarchy/prefs'
THEMES = os.path.join(ROOT, 'dist', 'amarchy-themes')
failed = 0


def say(ok, what, detail=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('[OK]' if ok else '[FAIL]', what, '' if ok or not detail else ': ' + detail), flush=True)
    return ok


def sysfile(where, name):
    return ami.read_file('%s:Sys/%s.prefs' % (where, name))


def snapshot():
    return {(w, n): sysfile(w, n) for w in ('ENV', 'ENVARC') for n in SYS_FILES}


def wb_colours():
    """(idx -> 'rrggbb') of the Workbench screen colours 0-3 from build/amiga/wbpens on the rig (T:wbpens)."""
    rc, out = c.run('T:wbpens')
    cols = {}
    for l in out.splitlines():
        m = re.match(r'RGB (\d+) ([0-9a-f]+) ([0-9a-f]+) ([0-9a-f]+)', l.strip())
        if m and int(m.group(1)) < 4:
            cols[int(m.group(1))] = ''.join(x.zfill(2) for x in m.groups()[1:])
    return cols


def wb_depth():
    for l in c.run('T:wbpens')[1].splitlines():
        m = re.match(r'DEPTH (\d+)', l.strip())
        if m:
            return int(m.group(1))
    return 2


def settle(names):
    """The ENV:Sys files of `names` identical on two reads 2 s apart (the editors rewrite them after USE)."""
    last = None
    for _ in range(15):
        cur = {n: sysfile('ENV', n) for n in names}
        if cur == last:
            return cur
        last = cur
        time.sleep(2)
    return last


def pointer_editor_version():
    """Version number of SYS:Prefs/Pointer on the rig (0 when absent): the Pointer write needs 47+."""
    m = re.search(r'(\d+)\.\d+', c.run('Version SYS:Prefs/Pointer')[1])
    return int(m.group(1)) if m else 0


def n_env(name):
    return int(t.env(name) or 0)


def put_prefs(local):
    """Put the prefs file. wasabid's rename into place fails on ENV:Amarchy/prefs once Amarchy has saved it (Delete
    works), so the old file is deleted first."""
    c.run('Delete >NIL: %s QUIET' % PREFS)
    ami.put(local, PREFS)


def pick(up=None):
    """Style > Amarchy theme > a row next to the cursor that is not the current theme > Return. The menu stays open on the pick: close it."""
    t.open_menu()
    st, _ = t.wait_stable(t.OPEN_SECS, what='open menu')
    # the top level differs per rig (3.1: Style is the 4th entry; the 3.2 rig lists Apps, Settings, Style): read where it is
    for _ in range(t.style_downs()):
        st, _ = t.step(g.DOWN, 'Down to Style')
    t.step(g.RETURN, 'enter Style')
    t.step(g.RETURN, 'enter Amarchy theme')
    rows = t.env('AMARCHY_ROWS').split('|')
    sel = int(t.env('AMARCHY_SEL') or 0)
    print('[INFO] theme list %d rows, cursor on %d (%s)' % (len(rows), sel, rows[sel] if sel < len(rows) else '?'))
    if up is None:
        up = sel + 1 >= len(rows)
    key = g.UP if up else g.DOWN
    print('[INFO] direction %s' % ('Up' if up else 'Down'))
    for _ in range(2):
        st, _ = t.step(key, 'move in theme list')
        sel = int(t.env('AMARCHY_SEL') or 0)
        if not rows[sel].endswith(' (current)'):
            break
    print('[INFO] picking row %d (%s)' % (sel, rows[sel]))
    ami.key(g.RETURN)
    try:
        t.wait_stable(t.STEP_SECS, differ_from=st, what='recoloured theme list')
    except TimeoutError:
        if t.menu_state()[0] is not None:  # the pick may also close the menu; only a menu still showing the old state is a failure
            raise
    g.close_menu()
    time.sleep(3)
    return rows[sel].split(' (')[0], up


def desk_expect_exe():
    exe = os.path.join(tempfile.gettempdir(), 'desk_expect_rig')
    srcs = ['tools/rig/desk_expect.c', 'amarchy/desk_core.c', 'amarchy/gthemes_core.c', 'handler/be32.c',
            'config/upconf.c', 'config/upconf_rows.c']
    paths = [os.path.join(ROOT, s) for s in srcs]
    if not os.path.exists(exe) or os.path.getmtime(exe) < max(os.path.getmtime(p) for p in paths):
        subprocess.check_call(['cc', '-std=c89', '-o', exe] + paths)
    return exe


def expect(theme, part, extra):
    exe = desk_expect_exe()
    return subprocess.check_output([exe, os.path.join(THEMES, theme + '.theme'), part] + extra)


def fields(part, data, ncol=8):
    """{field: value} of the themed fields of a prefs file, parsed by desk_expect (the same parser for expected and got)."""
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(data)
    try:
        out = subprocess.run([desk_expect_exe(), '-', 'fields', part, f.name, str(ncol)], capture_output=True)
    finally:
        os.unlink(f.name)
    if out.returncode:
        return None
    return dict(l.split(' ', 1) for l in out.stdout.decode('latin-1').splitlines())


def compare(part, got, want, what, ncol=8):
    """One [OK]/[FAIL] for a part: every themed field of `want` against `got`; each difference names field, expected, got."""
    if got is None:
        return say(False, what, 'file missing')
    g, w = fields(part, got, ncol), fields(part, want, ncol)
    if g is None or w is None:
        return say(False, what, 'cannot parse the %s file (%s)' % (part, 'got' if g is None else 'expected'))
    if part == 'wbpattern':
        # an all-zero pattern is pen 0 on every plane at any depth: the stock 3.2 Wbpattern editor rewrites the flat
        # DRAWER chunk (pen 0) with depth 0 after USE (observed rig 4, 2026-10-29); the pixels are what is compared
        for k in list(w):
            if k.endswith('.depth') and w.get(k[:-6] + '.data', 'x').strip('0 ') == '' and g.get(k[:-6] + '.data') == w[k[:-6] + '.data']:
                g[k] = w[k]
    diffs = ['%s: expected %s got %s' % (k, w[k], g.get(k, '<absent>')) for k in w if g.get(k) != w[k]]
    print('[INFO] %s: %d themed fields compared' % (part, len(w)))
    return say(not diffs, what, '; '.join(diffs[:12]))


def main():
    if not g.process_running('Amarchy'):
        print('[FAIL] the Amarchy commodity is not running')
        return 1
    g.close_menu()
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'wbpens'), 'T:wbpens')
    # the rig image has no pointer.prefs: seed ENV:Sys with the OS 3.2.2 file (tests/data/desk, the host golden) as the baseline
    if sysfile('ENV', 'pointer') is None and sysfile('ENVARC', 'pointer') is None:
        ami.put(os.path.join(ROOT, 'tests', 'data', 'desk', 'pointer-os322.prefs'), 'ENV:Sys/pointer.prefs')
        print('[INFO] seeded ENV:Sys/pointer.prefs from tests/data/desk/pointer-os322.prefs')
    base = snapshot()
    cols0 = wb_colours()
    print('[INFO] baseline Workbench colours %s; ENV:Sys %s' % (cols0, {n: (len(base[('ENV', n)]) if base[('ENV', n)] else None) for n in SYS_FILES}))
    hdr = '; Amarchy preferences. Written by the Amarchy preferences window (Exchange: Show Interface).\n'
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(hdr.encode('latin-1'))
    c.run('MakeDir >NIL: ENV:Amarchy')    # a fresh rig has no ENV:Amarchy yet; the put cannot create it
    put_prefs(f.name)
    os.unlink(f.name)
    prefs = (ami.read_file(PREFS) or b'').decode('latin-1')
    print('[INFO] %s before:\n%s' % (PREFS, prefs))
    c.run('Delete T:amarchy-revert ALL QUIET')

    # step 2: master off
    desks0 = n_env('AMARCHY_DESKS')
    name1, up1 = pick()
    after = snapshot()
    say(n_env('AMARCHY_DESKS') == desks0, 'off: AMARCHY_DESKS did not rise', 'was %r now %r' % (desks0, t.env('AMARCHY_DESKS')))
    say(after == base, 'off: no desktop prefs file changed (theme %s)' % name1,
        ', '.join('%s:%s' % k for k in base if base[k] != after[k]))
    say(c.run('List T:amarchy-revert')[0] != 0, 'off: T:amarchy-revert does not exist')

    # step 3: master on
    # the file as the first pick left it (it holds the picked theme), not the step-1 copy: the cursor then starts on that theme
    saved = (ami.read_file(PREFS) or b'').decode('latin-1').rstrip('\n') + '\n'
    if re.search(r'^\s*desktop-theme\s*=', saved, re.M):
        new = re.sub(r'^(\s*desktop-theme\s*=).*$', r'\1 on', saved, flags=re.M)
    else:
        # the keys belong to "[profile default]" (amconf_core.h); a key before the header is ignored
        new = saved + ('' if re.search(r'^\[profile default\]', saved, re.M) else '[profile default]\n') + 'desktop-theme = on\n'
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(new.encode('latin-1'))
    put_prefs(f.name)
    os.unlink(f.name)
    base_on = snapshot()
    cols_on = wb_colours()
    name2, _ = pick(up=False)  # the list cursor starts on row 0 again; a current row is stepped over
    say(bool(name2) and name2 != name1, 'on: picked a second theme', '%s then %s' % (name1, name2))
    say(n_env('AMARCHY_DESKS') == desks0 + 1, 'on: AMARCHY_DESKS +1', 'was %r now %r' % (desks0, t.env('AMARCHY_DESKS')))
    say(t.env('AMARCHY_DESKMASK') == '15', 'on: AMARCHY_DESKMASK 15', t.env('AMARCHY_DESKMASK'))
    got = settle(SYS_FILES)
    depth = wb_depth()
    ncol = 8 if depth >= 3 else 4
    print('[INFO] Workbench depth %d: palette compared at %d colours' % (depth, ncol))
    compare('palette', got['palette'], expect(name2, 'palette', [str(ncol)]),
            'on: ENV:Sys/palette.prefs themed fields equal desk_core output', ncol)
    compare('wbpattern', got['wbpattern'], expect(name2, 'wbpattern', [str(min(depth, 3))]),
            'on: ENV:Sys/wbpattern.prefs ROOT and DRAWER fields equal desk_core output')
    ptr = base_on[('ENV', 'pointer')] or base_on[('ENVARC', 'pointer')]
    if pointer_editor_version() < 47:       # desk_pointer_editor_ok (b375c835): below 47 the Pointer write is skipped
        say(got['pointer'] == ptr, 'on: SYS:Prefs/Pointer < 47, ENV:Sys/pointer.prefs not written (equals its baseline)')
    else:
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(ptr)
        w = expect(name2, 'pointer', [f.name])
        os.unlink(f.name)
        compare('pointer', got['pointer'], w, 'on: ENV:Sys/pointer.prefs RGB tables equal desk_core output')
    fbase = base_on[('ENV', 'font')] or base_on[('ENVARC', 'font')]
    fdir = tempfile.mkdtemp()  # no theme names a font here; an empty FONTS: stand-in keeps the user's name and size
    if fbase:
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(fbase)
        w = expect(name2, 'font', [f.name, fdir])
        os.unlink(f.name)
    else:
        w = expect(name2, 'font', ['-', fdir])
    os.rmdir(fdir)
    compare('font', got['font'], w, 'on: ENV:Sys/font.prefs WBFONT pens, modes, name and size equal desk_core output')
    cols = wb_colours()
    print('[INFO] Workbench colours before %s after %s' % (cols_on, cols))
    say(cols != cols_on, 'on: Workbench colours 0-3 changed live')
    for p, b in base_on.items():
        if p[0] == 'ENVARC':
            say(sysfile(*p) == b, 'on: ENVARC:Sys/%s.prefs unchanged' % p[1])
    rev = c.run('List T:amarchy-revert')[1]
    print('[INFO] T:amarchy-revert:\n%s' % rev)
    for (w, n), b in base_on.items():
        cp = ami.read_file('T:amarchy-revert/%s-Sys-%s.prefs' % (w, n)) if True else None
        if b is None:
            say(cp is None, 'on: no revert copy for missing %s:Sys/%s.prefs' % (w, n))
        else:
            say(cp == b, 'on: T:amarchy-revert copy of %s:Sys/%s.prefs holds the old bytes' % (w, n))
    ami.put_back = None
    print('[%s] %d failed' % ('OK' if not failed else 'FAIL', failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
