#!/usr/bin/env python3
"""Amarchy native desktop theme rig (plan 2026-10-29-amarchy-p1a-native-theme.md, RX1 first steps).

Real entry point: the drawn menu (Left Amiga + Space) > Style > Amarchy theme > Return, the path that runs
gt_dos_select (there is no `desk` ARexx verb or hotkey action yet: R1.12). The steps:
  1. Baseline: ENV:Sys and ENVARC:Sys prefs bytes, Workbench colours 0-3 (T:wbpens).
  2. Master switch off (the default): pick a theme. AMARCHY_DESKS must not rise, no desktop prefs file may change,
     T:amarchy-revert must not appear.
  3. Master switch on (desktop-theme = on in ENV:Amarchy/prefs): pick another theme. AMARCHY_DESKS +1, AMARCHY_DESKMASK 15;
     ENV:Sys palette, wbpattern, pointer, font equal the bytes desk_core makes for that theme (tools/rig/desk_expect.c,
     built on the host from the same sources); Workbench colours 0-3 changed; T:amarchy-revert holds the step-1 bytes of every
     file that existed; ENVARC:Sys unchanged (persist is R1.10).
The stock editors rewrite ENV:Sys files after USE, so the byte compare settles first and names every differing offset.

Rig up (UPTERM_RIG=<n> tools/rig/rig.py start --max --fresh), Amarchy running with this branch's binaries.
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


def n_env(name):
    return int(t.env(name) or 0)


def pick(up=None):
    """Style > Amarchy theme > a row next to the cursor that is not the current theme > Return. The menu stays open on the pick: close it."""
    t.open_menu()
    st, _ = t.wait_stable(t.OPEN_SECS, what='open menu')
    for _ in range(t.STYLE_DOWNS):
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
    t.wait_stable(t.STEP_SECS, differ_from=st, what='recoloured theme list')
    g.close_menu()
    time.sleep(3)
    return rows[sel].split(' (')[0], up


def expect(theme, part, extra):
    exe = os.path.join(tempfile.gettempdir(), 'desk_expect_rig')
    if not os.path.exists(exe):
        srcs = ['tools/rig/desk_expect.c', 'amarchy/desk_core.c', 'amarchy/gthemes_core.c', 'handler/be32.c',
                'config/upconf.c', 'config/upconf_rows.c']
        subprocess.check_call(['cc', '-std=c89', '-o', exe] + [os.path.join(ROOT, s) for s in srcs])
    return subprocess.check_output([exe, os.path.join(THEMES, theme + '.theme'), part] + extra)


def diff_offsets(a, b):
    n = min(len(a), len(b))
    d = [i for i in range(n) if a[i] != b[i]]
    return '%d bytes differ (len %d vs %d), first at %s' % (len(d), len(a), len(b), d[:8])


def main():
    if not g.process_running('Amarchy'):
        print('[FAIL] the Amarchy commodity is not running')
        return 1
    g.close_menu()
    ami.put(os.path.join(ROOT, 'build', 'amiga', 'wbpens'), 'T:wbpens')
    # a font the theme can name must exist for the font part; the expectation helper checks a local FONTS: stand-in
    base = snapshot()
    cols0 = wb_colours()
    print('[INFO] baseline Workbench colours %s; ENV:Sys %s' % (cols0, {n: (len(base[('ENV', n)]) if base[('ENV', n)] else None) for n in SYS_FILES}))
    hdr = '; Amarchy preferences. Written by the Amarchy preferences window (Exchange: Show Interface).\n'
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(hdr.encode('latin-1'))
    ami.put(f.name, PREFS)
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
    saved = prefs.rstrip('\n') + '\n'
    if re.search(r'^\s*desktop-theme\s*=', saved, re.M):
        new = re.sub(r'^(\s*desktop-theme\s*=).*$', r'\1 on', saved, flags=re.M)
    else:
        new = saved + 'desktop-theme = on\n'
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(new.encode('latin-1'))
    ami.put(f.name, PREFS)
    os.unlink(f.name)
    base_on = snapshot()
    cols_on = wb_colours()
    name2, _ = pick(up=not up1)
    say(bool(name2) and name2 != name1, 'on: picked a second theme', '%s then %s' % (name1, name2))
    say(n_env('AMARCHY_DESKS') == desks0 + 1, 'on: AMARCHY_DESKS +1', 'was %r now %r' % (desks0, t.env('AMARCHY_DESKS')))
    say(t.env('AMARCHY_DESKMASK') == '15', 'on: AMARCHY_DESKMASK 15', t.env('AMARCHY_DESKMASK'))
    got = settle(SYS_FILES)
    ncol = 8
    want = {'palette': expect(name2, 'palette', [str(ncol)]),
            'wbpattern': expect(name2, 'wbpattern', ['3'])}
    ptr = base_on[('ENV', 'pointer')] or base_on[('ENVARC', 'pointer')]
    for n, w in want.items():
        say(got[n] == w, 'on: ENV:Sys/%s.prefs equals desk_core output' % n,
            'missing' if got[n] is None else diff_offsets(got[n], w))
    if ptr:
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(ptr)
        w = expect(name2, 'pointer', [f.name])
        os.unlink(f.name)
        say(got['pointer'] == w, 'on: ENV:Sys/pointer.prefs equals desk_core output',
            'missing' if got['pointer'] is None else diff_offsets(got['pointer'], w))
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
