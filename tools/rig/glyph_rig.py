#!/usr/bin/env python3
"""glyph_rig.py -- a glyph above U+00FF reaches the screen as itself (UT1), and a selection ends where output
overwrites it (UT5).

The windows read their Unifont and emoji pages through UP-Term:unifont/ and UP-Term:emoji/. A rig with
no kit had no UP-Term: assign, so every such glyph drew the replacement. This prints "?" and then, in
a second run, the braille cell U+280B after the same "?", into an XCON: window, grabs the first text
row both times (ami.grab, a pixel compare, never a screenshot read by eye) and requires:
  - the assign UP-Term: exists and VTC:Apps/unifont/28 is there,
  - the row with "?" has ink in its first cell, and
  - the row with U+280B differs from the one with a second "?" (a replacement would draw "?"), and
  - a selection does not survive output over it (UT5): Select all, type an Echo, Copy finds nothing new.

  python3 tools/rig/glyph_rig.py        (rig up, handler installed, `make amiga build/amiga/snipprobe`, `rig.py install`; UPTERM_RIG=<n> as for the other scripts)
"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import condev_rig as c
import typewin as tw

TITLE = 'glyph check'
VTC = paths.RIG / 'vtc'
CLEAR = b'\x1b[2J\x1b[H'
NOTE = b"glyph check: reference ?? first, then ? and a glyph; ?? on the second line of tests means FAIL\n"


def first_row(name, text):
    """The pixels of the window's first text row after Type of a file that clears the window and prints text."""
    (VTC / name).write_bytes(CLEAR + text.encode('utf-8') + b"\n" + NOTE)
    tw.typ('Type VTC:' + name)
    tw.key(tw.RET, wait=3)
    w = ami.window(TITLE)
    if not w:
        raise SystemExit('[ERROR] window %s is gone' % TITLE)
    x, y, ww, hh = w['box']
    # the first text row: under the title bar (about 12 px) and the border, 16 px of rows, 48 px of columns
    _, _, rows = ami.grab((x, y, min(ww, 80), min(hh, 40)))
    return [r[: 3 * 80] for r in rows]


def clip_text(out):
    c.run('Delete %s QUIET' % out)
    c.run('VTC:snipprobe CLIP %s' % out)
    return c.run('Type %s' % out)[1]


def selection_check():
    """UT5: output over a selection drops it (xterm). Select all, let the shell echo a new word, Copy: with the
    selection gone Copy does nothing and the clipboard still holds the earlier text."""
    tw.typ('Echo AAAAAAAA')
    tw.key(tw.RET, wait=2)
    tw.key(0x20, tw.RAMIGA, wait=1)     # Select all
    tw.key(0x33, tw.RAMIGA, wait=1.5)   # Copy
    before = clip_text('RAM:glyph-clip1.txt')
    tw.click_in(TITLE)                   # a click drops the selection
    tw.key(0x20, tw.RAMIGA, wait=1)     # Select all again
    tw.typ('Echo BBBBBBBB')              # the echo and the output write over selected cells
    tw.key(tw.RET, wait=2)
    tw.key(0x33, tw.RAMIGA, wait=1.5)   # Copy
    after = clip_text('RAM:glyph-clip2.txt')
    check('the clipboard held the screen with AAAAAAAA', 'AAAAAAAA' in before, repr(before[-200:]))
    check('output over the selection dropped it (Copy found nothing new)', 'BBBBBBBB' not in after, repr(after[-200:]))
    c.run('Delete RAM:glyph-clip1.txt RAM:glyph-clip2.txt QUIET')


fails = 0


def check(name, ok, detail=''):
    global fails
    print('[%s] %s %s' % ('OK' if ok else 'ERROR', name, '' if ok else detail), flush=True)
    fails += 0 if ok else 1


def main():
    out = c.run('Assign >NIL: UP-Term: EXISTS')[0]
    check('UP-Term: is assigned', out == 0, 'rig.py setup, then a reboot, writes the assign into boot/go')
    check('VTC:Apps/unifont/28 is there (rig.py install copies it)', (VTC / paths.UPTERM_DRAWER / 'unifont' / '28').is_file())
    tw.open_xcon(TITLE)
    try:
        a = first_row('glyph_qq.txt', '??')
        b = first_row('glyph_qb.txt', '?⠋')
        blank = a[-1]
        ink = sum(1 for r in a if r != blank)
        check('the "??" row has ink', ink > 0, 'ink rows %d' % ink)
        diff = sum(1 for ra, rb in zip(a, b) if ra != rb)
        check('"?" and U+280B draw differently (not the replacement)', diff > 0, 'differing pixel rows %d' % diff)
        d1 = first_row('glyph_d1.txt', '-')
        d2 = first_row('glyph_d2.txt', '\u2014')
        check('the em dash draws Unifont\'s glyph, not the stand-in "-" (UT6)', sum(1 for x, y in zip(d1, d2) if x != y) > 0)
        lg = first_row('glyph_l1.txt', '\u2580\u2580\u2588 \u2588\u2580')    # pi's logo cells
        qq = first_row('glyph_l2.txt', '????? ??')
        check('the logo blocks draw as blocks, not "?"', sum(1 for x, y in zip(lg, qq) if x != y) > 0)
        selection_check()
    finally:
        for _ in range(3):      # a window left open takes the keys of the next script
            if not ami.window(TITLE):
                break
            tw.click_in(TITLE)
            tw.typ('endcli')
            tw.key(tw.RET, wait=3)
        check('the check window is closed', not ami.window(TITLE))
        for n in ('glyph_qq.txt', 'glyph_qb.txt', 'glyph_d1.txt', 'glyph_d2.txt', 'glyph_l1.txt', 'glyph_l2.txt'):
            (VTC / n).unlink(missing_ok=True)
    print('glyph_rig: %s' % ('PASS' if not fails else 'FAIL (%d)' % fails))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
