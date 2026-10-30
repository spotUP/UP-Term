#!/usr/bin/env python3
"""test_amarchy_gui_rebind.py -- a theme pick that resets the Workbench screen leaves amarchy-gui nothing on the old one.

  python3 -I tests/test_amarchy_gui_rebind.py

FLAT-P1-2: desk_dos_apply hides the menu window (gui_hide), IPrefs closes the Workbench screen and opens a new one,
then gui_show reopens the window. gui_show relocked the new screen but kept the menu font (g->mt, its m->scr), so the
first draw read the freed screen's ColorMap and ObtainBestPen waited forever on garbage: the bar never came back.
amarchy/gui.c is Amiga-only code that no host binary links, so this walks the structure that prevents it: every
screen-bound resource (lock, colour map, pens, menu font, icon bitmaps, text lengths) is taken in screen_bind and
dropped in screen_unbind, and open, hide, show and cleanup go through that pair and nothing else.
Exit 1 on a missing link or a screen resource taken outside the pair."""
import pathlib, re, sys

SRC = (pathlib.Path(__file__).resolve().parents[1] / 'amarchy' / 'gui.c').read_text()


def body(name):
    """The text of function `name`: from its definition line to the closing brace at column 0."""
    m = re.search(r'^[A-Za-z][^\n;]*\b' + name + r'\([^;{]*\)\s*\n?\{.*?^\}', SRC, re.S | re.M)
    if not m:
        sys.exit('FAIL: amarchy/gui.c has no function %s' % name)
    return m.group(0)


NEED = [
    ('gui_open', r'\bscreen_bind\(g\)'),
    ('gui_show', r'\bscreen_bind\(g\)'),
    ('gui_show', r'\bmg_layout\('),          # the window box comes from the new screen's font and size
    ('gui_hide', r'\bscreen_unbind\(g\)'),
    ('cleanup', r'\bscreen_unbind\(g\)'),
    ('screen_bind', r'\bpick_font\(g\)'),
    ('screen_bind', r'\bpens_get\(g, 0\)'),
    ('screen_bind', r'\big_open\(g->scr'),
    ('screen_bind', r'g->chev_w\s*='),
    ('screen_unbind', r'\bmt_close\(g->mt\)'),
    ('screen_unbind', r'\bpens_release\(g\)'),
    ('screen_unbind', r'\big_close\(\)'),
    ('screen_unbind', r'\bUnlockPubScreen\(0, g->scr\)'),
]
# taken or dropped anywhere else, a resource can outlive the screen it was made for
ONLY_IN_PAIR = [r'\bpick_font\(g\)', r'\bmt_close\(', r'\big_open\(', r'\big_close\(', r'\bUnlockPubScreen\(0, g->scr\)',
                r'g->scr\s*=\s*lock_front_screen\(']

bad = 0
for fn, pat in NEED:
    if not re.search(pat, body(fn)):
        print('FAIL: gui.c %s() no longer matches %s' % (fn, pat))
        bad = 1
outside = SRC.replace(body('screen_bind'), '').replace(body('screen_unbind'), '')
for pat in ONLY_IN_PAIR:
    for m in re.finditer(pat, outside):
        print('FAIL: gui.c line %d: %s outside screen_bind/screen_unbind' % (SRC[:SRC.find(m.group(0))].count('\n') + 1, m.group(0)))
        bad = 1
# the bug's order: the font's pens go before the font, the screen goes last
u = body('screen_unbind')
if not (u.find('pens_release') < u.find('mt_close') < u.find('UnlockPubScreen')):
    print('FAIL: screen_unbind must release pens, then close the font, then unlock the screen')
    bad = 1
print('FAIL' if bad else 'OK: amarchy-gui binds every screen resource in screen_bind/screen_unbind')
sys.exit(bad)
