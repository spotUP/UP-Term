#!/usr/bin/env python3
"""r6_rig.py -- outline plan R6.1/R6.2 on the rig: Nerd Font icons are two
cells wide and outline glyphs keep their shape on non-square pixels.

One window per mode, cleared, then three lines typed from a file:
  L0  FULL BLOCK, 'A', 'B', ICON (U+F07B, two cells), 'A', 'B'
  L1  FULL BLOCK, POWERLINE U+E0B0, FULL BLOCK
Cell geometry comes from the blocks (both at column 0): the cell width from
a run of identical blocks, the cell height from L1's top minus L0's top.
Per mode it checks:
  a. the icon's ink box in pixels (width:height printed; the caller compares
     the PAL hires and the RTG numbers: tall pixels give about twice the
     width for the height)
  b. the icon draws into both its cells, the 'A' and 'B' after it equal the
     first 'A' and 'B' (cursor advanced by two)
  c. U+E0B0: its first pixel column is ink on every row of the cell (meets
     the block with no gap)

  UPTERM_RIG=<n> python3 tools/rig/r6_rig.py hires|rtg
(rig up, handler installed; rtg: `rig.py rtg` first, hires: any Workbench.)
"""
import os, pathlib, shutil, struct, subprocess, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
TTF = ROOT / 'build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf'
FACE = 'SymbolsNerdFontMono'
ICON, BLOCK, PL = 0xF07B, 0x2588, 0xE0B0
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen else ''), flush=True)


def shot():
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    return w, h, b[8:]


def t(s, wt=2):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(wt)


def analyse(mode, w, h, px, box):
    bx, by, bw, bh = box
    pix = lambda x, y: bytes(px[(y * w + x) * 3:(y * w + x) * 3 + 3])
    x0, x1, y0, y1 = bx + 4, bx + bw - 18, by + 11, by + bh - 2
    bg = pix(x1 - 4, y1 - 4)   # the text area's bottom right corner: empty
    while pix(x1 - 4, y0) != bg and y0 < y1:   # under the title bar, whatever its height
        y0 += 1
    ink = lambda x, y: pix(x, y) != bg
    # first ink pixel (row-major) inside the text area: the block at L0 col 0
    top0 = ox = None
    for y in range(y0, y1):
        xs = [x for x in range(x0, x1) if ink(x, y)]
        if xs:
            top0, ox = y, min(xs)
            break
    if top0 is None:
        check(False, mode + ': any ink in the window')
        return
    fg = pix(ox, top0)
    # block cell width: run of fg along the top row from ox
    cw = 0
    while ink(ox + cw, top0) and pix(ox + cw, top0) == fg:
        cw += 1
    # block height down column ox
    bh0 = 0
    while ink(ox, top0 + bh0):
        bh0 += 1
    # the two block rows touch (L0, L1 at column 0) and a blank line follows: ch is half the column
    ch = bh0 // 2
    print('  %s: bg %s fg %s origin (%d,%d) cell %dx%s block ink height %d' % (mode, bg.hex(), fg.hex(), ox, top0, cw, ch, bh0))
    if not ch or cw < 4:
        check(False, mode + ': cell geometry read from the blocks', (cw, ch))
        return
    cell = lambda col, row: (ox + col * cw, top0 + row * ch)

    def box_of(x, y, nc):
        xs, ys = [], []
        for yy in range(y, y + ch):
            for xx in range(x, x + nc * cw):
                if ink(xx, yy):
                    xs.append(xx)
                    ys.append(yy)
        return (min(xs), max(xs), min(ys), max(ys)) if xs else None

    def same(a, b):
        (ax, ay), (bx2, by2) = a, b
        return all(pix(ax + i, ay + j) == pix(bx2 + i, by2 + j) for j in range(ch) for i in range(cw))

    ix, iy = cell(3, 0)
    bb = box_of(ix, iy, 2)
    if bb:
        wpx, hpx = bb[1] - bb[0] + 1, bb[3] - bb[2] + 1
        print('  %s: ICON ink box %d x %d px (ratio %.3f), x %d..%d of cell span %d..%d' %
              (mode, wpx, hpx, wpx / hpx, bb[0] - ix, bb[1] - ix, 0, 2 * cw - 1))
    check(bb is not None and bb[1] - ix >= cw, mode + ' 2b: the icon draws into its second cell', bb and (bb[1] - ix, cw))
    check(same(cell(5, 0), cell(1, 0)) and same(cell(6, 0), cell(2, 0)),
          mode + ' 2b: the cell after the icon is the next character (A B again; cursor advanced by 2)')
    # c: powerline cell at col 1 row 1
    px0, py0 = cell(1, 1)
    col0 = sum(1 for yy in range(py0, py0 + ch) if ink(px0, yy))
    pb = box_of(px0, py0, 1)
    check(col0 == ch, mode + ' 2c: U+E0B0 first pixel column is ink on all %d cell rows (no gap to the block)' % ch, col0)
    print('  %s: E0B0 ink box %s' % (mode, pb))
    blk = sum(1 for yy in range(py0, py0 + ch) if ink(px0 - 1, yy))
    check(blk == ch, mode + ' 2c: the block next to it fills all %d rows' % ch, blk)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else 'hires'
    subprocess.run(['make', '-s', 'symbolsnerd'], cwd=ROOT, check=True)
    shutil.copyfile(TTF, VTC / (FACE + '.ttf'))
    shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
    shutil.rmtree(VTC / 'fonts', ignore_errors=True)
    shutil.copytree(ROOT / 'dist/fonts', VTC / 'fonts')
    text = '\x1b[2J\x1b[H' + chr(BLOCK) + 'AB' + chr(ICON) + 'AB\n' + chr(BLOCK) + chr(PL) + chr(BLOCK) + '\n\n'
    (VTC / 'r6-line.txt').write_bytes(text.encode('utf-8'))
    (VTC / 'r6.conf').write_text('[profile default]\nfont-fallback = %s\n' % FACE)
    had = c.run('Copy ENV:up-term/up-term RAM:r6.bak QUIET')[0] == 0
    c.run('MakeDir ENV:up-term QUIET')
    try:
        c.run('MakeDir RAM:f1fonts QUIET')
        c.run('Delete RAM:f1fonts/#? QUIET')
        c.run('Assign FONTS: RAM:f1fonts ADD')
        c.run('Assign FONTS: VTC:fonts ADD')
        c.run('Copy VTC:%s.ttf RAM:f1fonts/' % FACE, 120)
        c.run('Copy VTC:r6.conf ENV:up-term/up-term QUIET')
        opts = 'CLOSE' if mode == 'rtg' else 'CLOSE/OWNSCREEN/SCREENMODE 0x29000'
        c.run('Run >NIL: NewShell "XCON:0/20/640/200/r6/%s"' % opts)
        time.sleep(6)
        box = ami.window('r6')['box']
        print('  window box', box)
        kx, ky = ami.pointer_scale()
        x, y, w, h = box
        ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
                   ('wait', 2), ('button', 0, 0), ('wait', 5))
        time.sleep(1)
        t('Type VTC:r6-line.txt', 6)
        sw, sh, px = shot()
        out = str(paths.RIG / ('r6_%s.png' % mode))
        ami.png(out, sw, sh, [px[r * sw * 3:(r + 1) * sw * 3] for r in range(sh)])
        print('  screenshot %dx%d -> %s' % (sw, sh, out))
        analyse(mode, sw, sh, px, box)
        t('EndCLI', 4)
    finally:
        if had:
            c.run('Copy RAM:r6.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:r6.bak QUIET')
        c.run('Assign FONTS: VTC:fonts REMOVE')
        c.run('Assign FONTS: RAM:f1fonts REMOVE')
        for f in ('r6.conf', 'r6-line.txt'):
            (VTC / f).unlink(missing_ok=True)
    print('r6_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
