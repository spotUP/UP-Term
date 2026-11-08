#!/usr/bin/env python3
"""amarchy_dopus_toolbar_rig.py -- B7 / DT-6: the theme apply switches a Directory Opus lister that uses upstream's default
toolbar to the flat bank of the set that fits the DOpus screen (DOpus5:Buttons/Amarchy-toolbar-<set>), recolours that set's
brushes to the applied theme, and never touches a lister with a toolbar of its own.

Steps: the generated sets (build/dopus-toolbar, `make dopus-toolbar`) go into DOpus5: the way dist/addons/dopus/install.dos
does (DOpus5:Images/Amarchy/<set>/, DOpus5:Buttons/Amarchy-toolbar-<set>, 16 24 32 each square and tall "t"); Amarchy runs;
two listers open, lister B keeps the default toolbar, lister A gets a copy under another name (a custom bank). Then the real
entry point (menu Style > Amarchy theme -> dopus_theme_send) runs twice, with two themes. After each apply:
  (a) `lister query <B> toolbar` is the bank dopustb_expect (dt_pick_set on this screen's font and pixel aspect) names;
      lister A reports its custom path;
  (b) the CMAP of one brush of that set, read back from the rig disk, equals dt_cmap of the applied theme (host helper
      tools/rig/dopustb_expect.c, the same functions); after the second theme the bytes differ from the first;
  (c) one screenshot per apply; the colours drawn in the toolbar strip of lister B are counted (PIL, nothing is looked at),
      so a DOpus that caches the brush shows the same toolbar colours after the second theme as after the first.
Prints [OK] / [FAIL] per sentinel; exit 1 on any [FAIL]. Screenshots go to the directory in argv[1] (default
build/rig-reports/rig<N>/dopus-toolbar); the screenshots are kept only with --keep-shots.
Usage: UPTERM_RIG=<n> python3 tools/rig/amarchy_dopus_toolbar_rig.py [outdir] [--keep-shots]   (rig up with DOpus 5.104
(`rig.py dopus`), `rig.py install` after `make amiga`; `make dopus-toolbar`)"""
import collections, os, re, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, condev_rig as c
import amarchy_gui_rig as g
import amarchy_desk_rig as d

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TB = os.path.join(ROOT, 'build', 'dopus-toolbar')
CUSTOM = 'DOpus5:Buttons/Custom-bank'
SETS = ['%s-%s' % (f, z) for f in ('filled', 'outline') for z in ('16', '16t', '24', '24t', '32', '32t')]   # DT-9: family-size
BRUSH = 'Copy.small'      # one brush of every set: its CMAP is read back
FAILS = []


def rx(s):
    return c.run('SYS:Rexxc/rx "OPTIONS RESULTS; ADDRESS DOPUS.1; %s; SAY RESULT"' % s)[1].strip()


def rxn(s):
    """A verb that returns no result (`lister set`): no SAY RESULT."""
    return c.run('SYS:Rexxc/rx "ADDRESS DOPUS.1; %s"' % s)[1].strip()


def handles():
    return rx('lister query all').split()


def toolbar(h):
    return rx('lister query %s toolbar' % h)


def verdict(ok, what, seen=''):
    print('[%s] %s%s' % ('OK' if ok else 'FAIL', what, (' | ' + seen) if seen else ''))
    if not ok:
        FAILS.append(what)


def install_sets():
    """The way dist/addons/dopus/install.dos does: every set's drawer, every set's bank, the older banks and drawers gone."""
    for s in SETS:
        c.run('MakeDir >NIL: DOpus5:Images/Amarchy')
        c.run('MakeDir >NIL: DOpus5:Images/Amarchy/%s' % s)
        for n in sorted(os.listdir(os.path.join(TB, 'Images', s))):
            ami.put(os.path.join(TB, 'Images', s, n), 'DOpus5:Images/Amarchy/%s/%s' % (s, n))
        ami.put(os.path.join(TB, 'Buttons', 'Amarchy-toolbar-' + s), 'DOpus5:Buttons/Amarchy-toolbar-' + s)
    c.run('Delete >NIL: DOpus5:Buttons/Amarchy-toolbar QUIET')
    c.run('Delete >NIL: DOpus5:Buttons/Amarchy-toolbar-(16|24|32)(t|%) QUIET')
    c.run('Delete >NIL: DOpus5:Images/Amarchy/(16|24|32)(t|%) ALL QUIET FORCE')
    print('installed sets:', c.run('List DOpus5:Images/Amarchy LFORMAT %N')[1].split(), '| banks:',
          [l.split()[0] for l in c.run('List DOpus5:Buttons')[1].splitlines() if l.startswith('Amarchy-toolbar')])


def expect_exe():
    exe = os.path.join(tempfile.gettempdir(), 'dopustb_expect_rig')
    srcs = ['tools/rig/dopustb_expect.c', 'amarchy/dopustb_core.c', 'amarchy/iconpen_core.c', 'amarchy/iconfile_core.c',
            'amarchy/gthemes_core.c', 'amarchy/desk_core.c', 'handler/be32.c', 'config/upconf.c', 'config/upconf_rows.c',
            'prefs/settings_def.c', 'prefs/prefs_core.c', 'amarchy/amconf_core.c', 'amarchy/amconf_keys.c', 'handler/slash.c',
            'handler/kcmode.c']
    paths = [os.path.join(ROOT, s) for s in srcs]
    if not os.path.exists(exe) or os.path.getmtime(exe) < max(os.path.getmtime(p) for p in paths):
        subprocess.check_call(['cc', '-std=c89', '-I', os.path.join(ROOT, 'amarchy'), '-o', exe] + paths)
    return exe


def expect(theme, font_h, rx_, ry_):
    out = subprocess.check_output([expect_exe(), os.path.join(d.THEMES, theme + '.theme'), str(font_h), str(rx_), str(ry_)])
    kv = dict(l.split(' ', 1) for l in out.decode().splitlines())
    return kv['set'], kv['cmap']


def screen_facts():
    """(font height, pixel rx, ry, 'WxH') of the DOpus screen. The font is the screen font: the FONT chunk of
    ENV:Sys/font.prefs with fp_Type FP_SCREENFONT (2), its ta_YSize; topaz 8 when the file or that chunk is absent.
    The icon and system font chunks do not count (the stock 3.2 ENVARC prefs carry an icon font bhelv 11, s64 rig 64).
    The aspect is the picture's row doubling (a 640x256 screen has pixels twice as high as wide, ami.py list_windows
    row_scale)."""
    w, h = [int(x) for x in rx('dopus screen').split()[1:3]]
    s = g.screens()[0]
    ry = int(s['row_scale'])
    font_h = 8
    if c.run('Type ENV:Sys/font.prefs')[0] == 0:
        hexdump = c.run('Type ENV:Sys/font.prefs HEX')[1]
        raw = bytes.fromhex(''.join(m.group(1).replace(' ', '') for m in re.finditer(r'^[0-9A-F]{4}: ((?:[0-9A-F]{8} ?){1,4})', hexdump, re.M)))
        # struct FontPrefs after the chunk header: fp_Type at +14, fp_TextAttr.ta_YSize at +24 (FONT tag +0x16, +0x20)
        screen = [int.from_bytes(raw[i + 0x20:i + 0x22], 'big') for i in range(len(raw))
                  if raw[i:i + 4] == b'FONT' and int.from_bytes(raw[i + 0x16:i + 0x18], 'big') == 2]
        if screen:
            font_h = screen[-1]
    return font_h, 1, ry, '%dx%d' % (w, h)


def cmap_of(path):
    """The 24 CMAP bytes of an ILBM read back from the rig's disk, as hex."""
    with tempfile.NamedTemporaryFile(delete=False) as f:
        local = f.name
    try:
        ami.main(['get', path, local])
        b = open(local, 'rb').read()
    finally:
        os.unlink(local)
    i = b.find(b'CMAP')
    return b[i + 8:i + 8 + int.from_bytes(b[i + 4:i + 8], 'big')].hex()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    keep = '--keep-shots' in sys.argv
    out = args[0] if args else os.path.join(ROOT, 'build', 'rig-reports', 'rig%s' % os.environ.get('UPTERM_RIG', ''), 'dopus-toolbar')
    os.makedirs(out, exist_ok=True)
    install_sets()
    c.run('Copy DOpus5:Buttons/Toolbar %s' % CUSTOM)
    for m in re.finditer(r'Process\s+(\d+):.*RAM:Amarchy\b', c.run('Status')[1]):
        c.run('Break %s C' % m.group(1))
        time.sleep(3)
    if not g.process_running('Amarchy'):
        c.run('Run >NIL: VTC:Amarchy')
        time.sleep(8)
    print('process Amarchy running:', g.process_running('Amarchy'))
    font_h, prx, pry, geom = screen_facts()
    print('dopus screen:', rx('dopus screen'), '| font height', font_h, '| pixel', '%d:%d' % (prx, pry))
    before = set(handles())
    for _ in range(2):
        c.run('SYS:Rexxc/rx "ADDRESS DOPUS.1; command DeviceList NEW"')
        time.sleep(4)
    new = [h for h in handles() if h not in before] or handles()[-2:]
    a, b = new[0], new[-1]
    print('handles:', handles(), '| A =', a, 'B =', b)
    print('toolbar A,B (upstream default):', toolbar(a), '|', toolbar(b))
    print('set A custom:', rxn("lister set %s toolbar '%s'" % (a, CUSTOM)) or 'ok')
    time.sleep(2)
    cmaps = []
    for n in range(2):
        g.close_menu()
        name, up = d.pick()   # away from the cursor, which sits on the current theme
        time.sleep(5)
        want_set, want_cmap = expect(name, font_h, prx, pry)
        want_bank = 'DOpus5:Buttons/Amarchy-toolbar-' + want_set
        got = toolbar(b)
        verdict(got.lower().replace('dopus5:', '') == want_bank.lower().replace('dopus5:', ''),
                '(a) theme %d "%s" on %s: lister B toolbar is the bank of set %s' % (n + 1, name, geom, want_set), got)
        verdict(toolbar(a) == CUSTOM, '(a) lister A keeps its custom bank', toolbar(a))
        got_cmap = cmap_of('DOpus5:Images/Amarchy/%s/%s' % (want_set, BRUSH))
        verdict(got_cmap == want_cmap, '(b) theme %d: CMAP of %s/%s equals dt_cmap' % (n + 1, want_set, BRUSH), got_cmap)
        cmaps.append(got_cmap)
        rxn('dopus front')
        time.sleep(3)
        shot = os.path.join(out, 'toolbar-theme%d.png' % (n + 1))
        ami.main(['shot', shot])
        print('shot:', shot)
    verdict(cmaps[0] != cmaps[1], '(b) the second theme changed the CMAP bytes', '%s -> %s' % tuple(cmaps))
    if not keep:
        for n in range(2):
            os.unlink(os.path.join(out, 'toolbar-theme%d.png' % (n + 1)))
    print('RESULT:', 'FAIL (%d)' % len(FAILS) if FAILS else 'PASS')
    sys.exit(1 if FAILS else 0)


if __name__ == '__main__':
    main()
