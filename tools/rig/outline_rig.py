#!/usr/bin/env python3
"""outline_rig.py -- ledger F1 on the rig: glyphs the bitmap font lacks come
from an outline font (plan thoughts/shared/plans/2026-10-03-outline-fonts.md).

  1. Symbols Nerd Font Mono, the kit's pinned copy (`make symbolsnerd`),
     copied as a bare SymbolsNerdFontMono.ttf into RAM:f1fonts, which is
     assigned to FONTS: with ADD (the rig's system disk is not touched).
     No font library, no ttfinstall, no .otag: the handler finds
     FONTS:<name>.ttf itself (plan R5.1).
  2. The same UTF-8 line typed out in an XCON:/XTERM window twice: with a
     profile naming no fallback font, then with font-fallback naming it.
     The two screenshots differ exactly in the icon cells: eight Nerd Font
     icons and two plane-15/private-use codes (OWN) drawn as glyphs where
     the first run drew '?'; a CJK character the font lacks and an em dash
     (Latin-1 stand-in '-') do not change, nor does the ASCII around them.
     UP-Term's own TrueType engine (render/ttf.c) reads the .ttf, so
     32-bit codes draw too. `UPTerm state outline-own` in
     the window counts the glyphs that engine drew: 0 without the
     fallback, at least len(ICONS)+len(OWN) with it.
  3. Antialiasing (plan phase 4, R4.5): with the fallback font and the
     default `outline-antialias = on`, most icon cells hold at least one
     colour that is neither the window's foreground nor its background
     (fg = the 'A' glyph's pixels, bg = the commonest colour of the text
     area, both read from the plain shot). A third run with
     `outline-antialias = off` draws icon cells of foreground and
     background only, still differing from the plain run. `UPTerm state
     outline-aa` (glyphs drawn antialiased) is at least len(ICONS)+len(OWN)
     with it on and 0 with it off. On an AGA screen without a free shade
     pen the 1-bit path draws, and the intermediate-colour check fails:
     read that as the finding, not as a handler fault. Run it on RTG and
     on AGA 256 colours (`rig.py rtg` / `rig.py aga256`, each reboots).
  4. The time from the command to the drawn line, with the font (the
     worker's first glyphs: engine open, one rasterisation per icon).

The profile lives in ENV:up-term/up-term only; the file there before is put
back at the end. Run with the rig up and the handler installed:
  python3 tools/rig/outline_rig.py
"""
import os, pathlib, shutil, struct, subprocess, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
TTF = ROOT / 'build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf'
FACE = 'SymbolsNerdFontMono'
ICONS = [0xE0A0, 0xF07B, 0xF121, 0xE7A8, 0xF015, 0xE712, 0xF09B, 0xF1D3]
# codes above U+FFFF and in private use (render/ttf.c reads 32-bit codes
# and cmap format 12): they change like the icons. Both
# lists can be given as hex: OUTLINE_ICONS=EBCF,F011 OUTLINE_OWN=F003B,E615
OWN = [0xF003B, 0xE615]
if os.environ.get('OUTLINE_ICONS'):
    ICONS = [int(x, 16) for x in os.environ['OUTLINE_ICONS'].split(',')]
if os.environ.get('OUTLINE_OWN'):
    OWN = [int(x, 16) for x in os.environ['OUTLINE_OWN'].split(',') if x]
CJK, EMDASH = 0x4E2D, 0x2014
WX, WY, WW, WH = 0, 20, 640, 200
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def fetch():
    """the kit's pinned font (the Makefile checks both SHA-256s), to VTC:"""
    subprocess.run(['make', '-s', 'symbolsnerd'], cwd=ROOT, check=True)
    shutil.copyfile(TTF, VTC / (FACE + '.ttf'))


def shot():
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    return w, h, b[8:]


def diff_cells(a, b, w, x0, y0, x1, y1, cw=8, ch=8):
    """{text row: [8-pixel columns]} whose pixels differ between two shots,
    counted from the box's top left"""
    rows = {}
    for y in range(y0, y1):
        for x in range(x0, x1):
            o = (y * w + x) * 3
            if a[o:o + 3] != b[o:o + 3]:
                rows.setdefault((y - y0) // ch, set()).add((x - x0) // cw)
    return {k: sorted(v) for k, v in rows.items()}


def run_line(profile, keys=()):
    """types the line in a fresh window; returns the shot, the seconds, and {key: int or None} of the
    `UPTerm state <key>` values read from that window"""
    shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')    # the rig's path has none
    (VTC / 'outline-rig.conf').write_text(profile)
    c.run('Copy VTC:outline-rig.conf ENV:up-term/up-term QUIET')
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/outline/CLOSE/XTERM"' % (WX, WY, WW, WH))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((WX + 300) * kx), int((WY + 150) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:outline-line.txt')
    time.sleep(0.5)
    t0 = time.time()
    ami.key(0x44)
    time.sleep(6)
    w, h, px = shot()
    secs = time.time() - t0
    states = {}
    for key in keys:
        # a /state counter of this window, written to the host-shared dir
        (VTC / 'own.txt').unlink(missing_ok=True)
        ami.req(0x08, bytes([4]) + b'VTC:UPTerm state %s >VTC:own.txt' % key.encode())
        time.sleep(0.5)
        ami.key(0x44)
        time.sleep(3)
        try:
            states[key] = int((VTC / 'own.txt').read_text().split()[0])
        except (OSError, ValueError, IndexError):
            states[key] = None
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)
    return w, h, px, secs, states


def changed_band(a, b, w, x0, y0, x1, y1):
    """(first, last) pixel row between y0 and y1 whose pixels differ between two shots: the icon line's
    own rows, whatever the screen's text row height is (the other text rows are the same in both)"""
    ys = [y for y in range(y0, y1) if any(a[(y * w + x) * 3:(y * w + x) * 3 + 3] != b[(y * w + x) * 3:(y * w + x) * 3 + 3]
                                          for x in range(x0, x1))]
    return (ys[0], ys[-1]) if ys else (y0, y0)


def cell_colours(px, w, x0, band, col, cw=8):
    """{RGB: pixel count} of one text cell: columns from the box's left x0, the pixel rows of `band`"""
    got = {}
    for y in range(band[0], band[1] + 1):
        for x in range(x0 + col * cw, x0 + (col + 1) * cw):
            o = (y * w + x) * 3
            k = bytes(px[o:o + 3])
            got[k] = got.get(k, 0) + 1
    return got


def main():
    fetch()
    # one line: ASCII, the icons (a blank between each), CJK, em dash, ASCII
    text = 'A ' + ' '.join(chr(i) for i in ICONS + OWN) + ' ' + chr(CJK) + ' ' + chr(EMDASH) + ' Z\n'
    (VTC / 'outline-line.txt').write_bytes(text.encode('utf-8'))
    had = c.run('Copy ENV:up-term/up-term RAM:outline-rig.bak QUIET')[0] == 0
    c.run('MakeDir ENV:up-term QUIET')
    try:
        c.run('MakeDir RAM:f1fonts QUIET')
        c.run('Delete RAM:f1fonts/#? QUIET')
        c.run('Assign FONTS: RAM:f1fonts ADD')
        rc, out = c.run('Copy VTC:%s.ttf RAM:f1fonts/' % FACE, 120)
        names = [l.strip() for l in c.run('List RAM:f1fonts LFORMAT "%n"')[1].splitlines() if l.strip()]
        check(rc == 0 and names == [FACE + '.ttf'], 'FONTS: holds the bare .ttf only (no .otag)', (rc, out, names))
        if rc:
            return 1
        face = FACE
        w, h, plain, _, st = run_line('[profile default]\n', ('outline-own', 'outline-aa'))
        own_plain, aa_plain = st['outline-own'], st['outline-aa']
        w, h, fancy, secs, st = run_line('[profile default]\nfont-fallback = %s\n' % face, ('outline-own', 'outline-aa'))
        own_fancy, aa_fancy = st['outline-own'], st['outline-aa']
        w, h, flat, _, st = run_line('[profile default]\nfont-fallback = %s\noutline-antialias = off\n' % face,
                                     ('outline-aa',))
        aa_off = st['outline-aa']
        # the window's text area: 4 pixels in, under an 11-pixel title bar
        # (topaz 8 Workbench). The shell's number differs between the two
        # windows (its banner and prompt): the typed line is the row that
        # changed most.
        rows = diff_cells(plain, fancy, w, WX + 4, WY + 11, WX + WW - 18, WY + WH - 2)
        line = max(rows, key=lambda k: len(rows[k])) if rows else None
        cols = rows.get(line, [])
        n = len(ICONS) + len(OWN)
        want = [2 + 2 * i for i in range(n)]
        check(cols == want, 'the icons and the own-engine codes are drawn from the outline font, nothing else on their line moves',
              'changed columns %s, icons at %s (all rows: %s)' % (cols, want, rows))
        check(own_plain == 0, 'outline-own is 0 without a fallback font', own_plain)
        check(own_fancy is not None and own_fancy >= n, 'outline-own counts at least %d glyphs with the font' % n, own_fancy)
        print('  outline-own: plain %s, fancy %s' % (own_plain, own_fancy))
        check(2 + 2 * n not in cols, 'the CJK character the font lacks keeps its \'?\'', cols)
        check(4 + 2 * n not in cols, 'the em dash keeps its stand-in', cols)
        # antialiasing: fg and bg from the plain shot, intermediate colours in the icon cells
        bx, by = WX + 4, WY + 11
        area = {}
        for y in range(by, WY + WH - 2):
            for x in range(bx, WX + WW - 18):
                k = bytes(plain[(y * w + x) * 3:(y * w + x) * 3 + 3])
                area[k] = area.get(k, 0) + 1
        bg = max(area, key=area.get)
        icon_cols = want
        band = changed_band(plain, fancy, w, bx + want[0] * 8, by, bx + (want[-1] + 1) * 8, WY + WH - 2)
        print('  icon rows %d..%d' % band)
        fgs = {k: v for k, v in cell_colours(plain, w, bx, band, 0).items() if k != bg}
        fg = max(fgs, key=fgs.get) if fgs else None
        print('  bg %s fg %s' % (bg.hex(), fg.hex() if fg else None))
        mid_aa = [len(set(cell_colours(fancy, w, bx, band, k)) - {fg, bg}) for k in icon_cols]
        mid_off = [len(set(cell_colours(flat, w, bx, band, k)) - {fg, bg}) for k in icon_cols]
        print('  intermediate colours per icon cell: on %s, off %s' % (mid_aa, mid_off))
        print('  off-run colours beyond fg/bg: %s' % sorted({k.hex() for col in icon_cols
              for k in set(cell_colours(flat, w, bx, band, col)) - {fg, bg}}))
        check(fg is not None and sum(1 for m in mid_aa if m) * 2 > n,
              'antialias on: most icon cells hold a colour that is neither foreground nor background', mid_aa)
        rows_off = diff_cells(plain, flat, w, bx, by, WX + WW - 18, WY + WH - 2)
        check(not any(mid_off) and rows_off.get(line) == want,
              'antialias off: icon cells hold only foreground and background, and still differ from the plain run',
              'intermediate %s, changed columns %s' % (mid_off, rows_off.get(line)))
        check(aa_plain == 0 and aa_off == 0 and aa_fancy is not None and aa_fancy >= n,
              'outline-aa is 0 plain and with antialias off, at least %d with it on' % n,
              (aa_plain, aa_fancy, aa_off))
        print('  outline-aa: plain %s, on %s, off %s' % (aa_plain, aa_fancy, aa_off))
        print('  line with the font drawn within %.1f s of Return (6 s wait)' % secs)
        ami.png(str(paths.RIG / 'outline_plain.png'), w, h,
                [plain[r * w * 3:(r + 1) * w * 3] for r in range(h)])
        ami.png(str(paths.RIG / 'outline_fancy.png'), w, h,
                [fancy[r * w * 3:(r + 1) * w * 3] for r in range(h)])
        ami.png(str(paths.RIG / 'outline_flat.png'), w, h,
                [flat[r * w * 3:(r + 1) * w * 3] for r in range(h)])
    finally:
        if had:
            c.run('Copy RAM:outline-rig.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:outline-rig.bak QUIET')
        c.run('Assign FONTS: RAM:f1fonts REMOVE')
        (VTC / 'outline-rig.conf').unlink(missing_ok=True)
    print('outline_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
