#!/usr/bin/env python3
"""own_prefs_rig.py -- Amarchy's own settings file on the rig (plan 2026-10-25-amarchy-own-prefs.md, rows G2 G3 G4;
R1 R2 R4 M1). The settings window (phase U) does not exist yet: the cases write ENV:Amarchy/prefs and
ENVARC:Amarchy/prefs the way its Save will, and prove the commodity and amarchy-gui read them.

  own_prefs_migrate  M1/G4: only the old UP-Term file holds Amarchy keys (ENVARC:up-term/up-term, default profile),
                     no ENVARC:Amarchy/prefs. The first Amarchy start writes it with those values; the second start
                     (after QUIT) leaves it byte for byte and in date alone.
  own_prefs_style    R1/R4/G2: menu-style changed in both files while Amarchy runs; the notify reloads it and
                     Left Amiga + Space opens the other menu program (amarchy-gui for graphical, amarchy-menu for text).
  own_prefs_font     R2/G3: amarchy-menu-font changed; the drawn menu opens in a window of another size
                     (the cell is the font's), back at the default it returns to the first size.

Self-checking, rig up (rig.py start --max). The script stages the build (stage_rig.py). UPTERM_RIG=<n>.
Usage: python3 tools/rig/own_prefs_rig.py [--no-build] [migrate] [style] [font]"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c, stage_rig

LAMIGA, SPACE, ESC = 0x40, 0x40, 0x45
NEW, NEWARC = 'ENV:Amarchy/prefs', 'ENVARC:Amarchy/prefs'
OLD, OLDARC = 'ENV:up-term/up-term', 'ENVARC:up-term/up-term'
FILES = (NEW, NEWARC, OLD, OLDARC)
bad = 0


def check(ok, what):
    global bad
    bad += not ok
    print('%s %s' % ('ok' if ok else 'FAIL', what), flush=True)
    return ok


def arexx(cmd):
    m = ami._m()
    try:
        with ami._conn() as conn:
            return m.arexx(conn, 'AMARCHY', cmd)[2].strip()
    except m.WasabiError:
        return ''


def running(name):
    return any(l.rstrip().split('/')[-1].split(':')[-1] == name for l in c.run('Status')[1].splitlines())


def write(path, *lines):
    """The file holds the lines (an empty list: absent)."""
    c.run('Delete >NIL: %s QUIET' % path)
    if lines:
        c.run('MakeDir >NIL: %s' % path.rsplit('/', 1)[0])
        for i, l in enumerate(lines):
            c.run('Echo %s %s "%s"' % ('>' if i == 0 else '>>', path, l))


def text(path):
    b = ami.read_file(path)
    return None if b is None else b.decode('latin-1')


def stat(path):
    """size and date/time line of the file as List shows it, None when absent"""
    rc, out = c.run('List %s' % path)
    return out.strip() if rc == 0 else None


def start_amarchy():
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    end = time.time() + 15
    while time.time() < end:
        if arexx('STATUS'):
            return True
        time.sleep(0.5)
    return False


def quit_amarchy():
    arexx('QUIT')
    end = time.time() + 10
    while time.time() < end and arexx('STATUS'):
        time.sleep(0.5)
    time.sleep(5)    # quiet before anything else touches the disk
    return not arexx('STATUS')


def close_menus():
    for _ in range(3):
        if not (running('amarchy-gui') or running('amarchy-menu')):
            return True
        ami.key(ESC)
        time.sleep(2)
    return False


def hotkey():
    """the menu program the hotkey starts: 'amarchy-gui', 'amarchy-menu' or None"""
    ami.key(SPACE, LAMIGA)
    end = time.time() + 15
    while time.time() < end:
        for p in ('amarchy-gui', 'amarchy-menu'):
            if running(p):
                time.sleep(2)
                return p
        time.sleep(0.5)
    return None


def window_box():
    for t in ('Amarchy',):
        w = ami.window(t)
        if w:
            return tuple(w['box'])
    return None


def case_migrate():
    print('-- own_prefs_migrate')
    quit_amarchy()
    write(NEW)
    write(NEWARC)
    write(OLD)
    write(OLDARC, '[profile default]', 'menu-style = text', 'amarchy-menu-font = topaz:8',
          'amarchy-app-icons = none', 'amarchy-hide-drawers = Work:Hidden')
    check(text(NEWARC) is None, 'before: no ENVARC:Amarchy/prefs')
    check(start_amarchy(), 'first start: the commodity answers on AMARCHY')
    time.sleep(3)
    t1 = text(NEWARC)
    check(t1 is not None, 'first start wrote ENVARC:Amarchy/prefs')
    t1 = t1 or ''
    for want in ('menu-style = text', 'amarchy-menu-font = topaz:8', 'amarchy-app-icons = none',
                 'amarchy-hide-drawers = Work:Hidden'):
        check(want.replace(' = ', '=').replace(' ', '') in t1.replace(' = ', '=').replace(' ', ''),
              'ENVARC:Amarchy/prefs holds "%s"' % want)
    print('info file:\n' + t1)
    s1 = stat(NEWARC)
    check(quit_amarchy(), 'QUIT, then 5 s quiet')
    time.sleep(62)   # a rewrite would change the minute of List's date as well as the content
    check(start_amarchy(), 'second start')
    time.sleep(3)
    t2, s2 = text(NEWARC), stat(NEWARC)
    check(t2 == t1, 'second start left the content alone')
    check(s2 == s1, 'second start left size and date alone (%r vs %r)' % (s1, s2))


def case_style():
    print('-- own_prefs_style')
    quit_amarchy()
    write(OLD)
    write(OLDARC)
    write(NEWARC, '[profile default]', 'menu-style = graphical')
    write(NEW, '[profile default]', 'menu-style = graphical')
    check(start_amarchy(), 'start with menu-style = graphical')
    time.sleep(3)
    got = hotkey()
    check(got == 'amarchy-gui', 'graphical: the hotkey opened %s' % got)
    close_menus()
    for style, want in (('text', 'amarchy-menu'), ('minimal', 'amarchy-gui'), ('graphical', 'amarchy-gui'), ('text', 'amarchy-menu')):
        for f in (NEWARC, NEW):
            write(f, '[profile default]', 'menu-style = %s' % style)
        time.sleep(4)    # notify: the file change reaches the commodity
        got = hotkey()
        check(got == want, 'menu-style = %s (ENV and ENVARC written while running): the hotkey opened %s, want %s' % (style, got, want))
        close_menus()
    # a change of the OLD file alone no longer reaches Amarchy (R4: the watch moved to Amarchy/prefs)
    write(OLDARC, '[profile default]', 'menu-style = graphical')
    write(OLD, '[profile default]', 'menu-style = graphical')
    time.sleep(4)
    got = hotkey()
    check(got == 'amarchy-menu', 'the UP-Term file changed to graphical: still the text menu (%s), the new file wins' % got)
    close_menus()
    write(OLDARC)
    write(OLD)


def case_font():
    print('-- own_prefs_font')
    quit_amarchy()
    write(OLD)
    write(OLDARC)
    for f in (NEWARC, NEW):
        write(f, '[profile default]', 'menu-style = graphical')
    check(start_amarchy(), 'start')
    time.sleep(3)
    sizes = {}
    for label, line in (('default', None), ('topaz8', 'amarchy-menu-font = topaz:8'), ('topaz9', 'amarchy-menu-font = topaz:9'),
                        ('default again', None)):
        for f in (NEWARC, NEW):
            write(f, *(['[profile default]', 'menu-style = graphical'] + ([line] if line else [])))
        time.sleep(4)
        got = hotkey()
        box = window_box()
        sizes[label] = box
        print('info %s: %s window %s' % (label, got, box))
        check(got == 'amarchy-gui' and box is not None, '%s: amarchy-gui open, box %s' % (label, box))
        close_menus()
    d, t8, t9, d2 = (sizes[k] for k in ('default', 'topaz8', 'topaz9', 'default again'))
    if all((d, t8, t9, d2)):
        # on a tall-pixel screen the built-in menu font is topaz 8 (render/fontpair.c), so topaz:8 may equal it
        check(t9[2:] != d[2:], 'topaz:9 gives a window size %s other than the default %s' % (t9[2:], d[2:]))
        check(t9[2:] != t8[2:], 'topaz:9 gives a size %s other than topaz:8 %s' % (t9[2:], t8[2:]))
        check(d2[2:] == d[2:], 'font key removed: back at the default size %s' % (d2[2:],))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    cases = args or ['migrate', 'style', 'font']
    stage_rig.report(stage_rig.stage_build(build='--no-build' not in sys.argv))
    saved = {f: ami.read_file(f) for f in FILES}
    try:
        for n in cases:
            globals()['case_' + n]()
    finally:
        quit_amarchy()
        for f, b in saved.items():
            ami.restore_file(f, b)
    print('FAIL' if bad else 'PASS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
