#!/usr/bin/env python3
"""gridfit_rig.py -- s48 gridfit: do the TTF_FILL glyphs (box drawing, block, powerline) cover whole pixels on a tall-pixel screen?

Rig on `rig.py start --max` (AGA hires PAL, pixel 1:2, so rx != ry), handler installed (`rig.py install`, then a reboot:
a mounted XCON: keeps the old handler). An XCON window prints the glyphs in white on a flat black background, the
screenshot is read as raw RGB (never looked at) and every glyph cell is printed as a pixel map: `#` the glyph's pen,
`.` the background pen, `o` any other pen (an intermediate shade). Per row and per column of each cell the counts follow.
The first glyph of a run is the anchor that gives the cell origin, width and height: a full block (U+2588) or, for a
font without one, the full-height powerline triangle (U+E0B0). Cells are a blank cell apart (pitch = 2 cells).

  UPTERM_RIG=<n> python3 tools/rig/gridfit_rig.py <jbmono|symbols|icons|stress> <out-prefix> [again [x0,y0,cw,ch]]   (again: re-read <out-prefix>.rgb; the cell given: 20,34,8,8 on this screen)
    jbmono  : U+2588 U+2500 U+2502 U+253C U+E0B0 from JetBrains Mono (build/jbmono)
    symbols : U+E0B0 U+E0B4 from the Symbols Nerd Font (build/symbolsnerd)
    icons   : the anchor U+E0B0 and 8 straight-edged icons (U+F07B F15B F0C9 F00C E0A0 F013 F023 F1C0): pixel maps and stem verdicts
    stress  : 25 rows of 30 icons in a 230-row window, then Version, the window and serial.log are checked (Guru, hang)
Exit 1 when a straight stem has an intermediate pen beside it or a full block is not all one pen.
"""
import collections, pathlib, shutil, struct, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
WX, WY, WW, WH = 0, 20, 400, 100
WHITE = bytes.fromhex('ffffff')   # SGR 97 on the 16-pen palette
RUNS = {
    'jbmono': ('build/jbmono/JetBrainsMono-Regular.ttf', 'GridfitJB', [0x2588, 0x2500, 0x2502, 0x253C, 0xE0B0]),
    'symbols': ('build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf', 'GridfitSym', [0xE0B0, 0xE0B4]),
    # icon mode: the anchor U+E0B0 first, then straight-edged Nerd Font icons (folder, file, bars, check, branch, cog, lock, database)
    'icons': ('build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf', 'GridfitSym',
              [0xE0B0, 0xF07B, 0xF15B, 0xF0C9, 0xF00C, 0xE0A0, 0xF013, 0xF023, 0xF1C0]),
    # stress: a screenful of the 8 icons, then the handler must still answer (no Guru, no stack overflow)
    'stress': ('build/symbolsnerd/SymbolsNerdFontMono-Regular.ttf', 'GridfitSym', [0xE0B0]),
}
ICONS = [0xF07B, 0xF15B, 0xF0C9, 0xF00C, 0xE0A0, 0xF013, 0xF023, 0xF1C0]


def shoot(run, out):
    src, face, cps = RUNS[run]
    src = ROOT / src if (ROOT / src).exists() else paths.VTCON / src
    shutil.copyfile(src, VTC / (face + '.ttf'))
    line = '\x1b[97;40m\x1b[2J\x1b[H  %s\x1b[m\n' % ' '.join(chr(i) for i in cps)
    if run == 'stress':   # anchor line, then 25 rows of 30 icons (all 8, cycling)
        rows = ['  ' + ' '.join(chr(ICONS[(r + k) % 8]) for k in range(30)) for r in range(25)]
        line = '\x1b[97;40m\x1b[2J\x1b[H  %s\n%s\x1b[m\n' % (chr(0xE0B0), '\n'.join(rows))
    (VTC / 'gf-line.txt').write_bytes(line.encode('utf-8'))
    (VTC / 'gf.conf').write_text('[profile default]\nfont-fallback = %s\n' % face)
    c.run('MakeDir ENV:up-term QUIET')
    c.run('MakeDir RAM:f1fonts QUIET')
    c.run('Assign FONTS: RAM:f1fonts ADD')
    c.run('Copy VTC:%s.ttf RAM:f1fonts/ QUIET' % face, 120)
    c.run('Copy VTC:gf.conf ENV:up-term/up-term QUIET')
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/gf%s/CLOSE/XTERM"' % ((WX, WY, WW, 230 if run == 'stress' else WH) + (run,)))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((WX + 200) * kx), int((WY + 60) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:gf-line.txt')
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(6)
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    px = bytes(b[8:])
    pathlib.Path(out + '.rgb').write_bytes(struct.pack('>HH', w, h) + px)
    if run == 'stress':
        alive(out)
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)
    return w, h, px


def alive(out):
    """After the icon flood: the handler answers (a command runs, the window is still there), no Guru in the serial log."""
    rc, txt = c.run('Version', 30)
    print('alive: Version rc=%d %r' % (rc, txt.strip()[:60]))
    win = ami.window('gfstress')
    print('alive: window gfstress', 'present %s' % (win,) if win else 'MISSING')
    log = (paths.RIG / 'serial.log').read_text(errors='replace') if (paths.RIG / 'serial.log').exists() else ''
    hits = [l for l in log.splitlines() if 'guru' in l.lower() or 'software failure' in l.lower()]
    print('alive: guru lines in serial.log: %d %s' % (len(hits), hits[:3]))
    ok = rc == 0 and win is not None and not hits
    print('STRESS %s' % ('PASS' if ok else 'FAIL'))
    pathlib.Path(out + '.alive').write_text('PASS' if ok else 'FAIL')


def longest_ink_run(line):
    """Length of the longest run of non-background pens ('#' or 'o') in a row or column string."""
    best = cur = 0
    for c in line:
        cur = cur + 1 if c != '.' else 0
        best = max(best, cur)
    return best


def stem_verdict(cls):
    """A stem is a row or column whose longest ink run (any non-background pen) covers at least 3/4 of the cell's
    extent. Judge only that run: the stem is SOFT when at least 3/4 of the run's pens are intermediate shades (an
    unsnapped stem sits on two pixels at half coverage, every pen soft). A notch in the font design (a broken line, a
    diagonal cut) softens a minority of the run and does not make the stem soft. Returns (stem rows, stem cols,
    intermediate pens on stem runs, verdict)."""
    ch, gw = len(cls), len(cls[0])
    cols = [''.join(cls[y][x] for y in range(ch)) for x in range(gw)]

    def runs(lines, extent):
        out = []
        for i, ln in enumerate(lines):
            if longest_ink_run(ln) * 4 >= extent * 3:
                # the longest run itself
                best, start, cur, s0 = 0, 0, 0, 0
                for k, c in enumerate(ln + '.'):
                    if c != '.':
                        if cur == 0:
                            s0 = k
                        cur += 1
                    else:
                        if cur > best:
                            best, start = cur, s0
                        cur = 0
                out.append((i, ln[start:start + best]))
        return out
    rr, cc = runs([''.join(r) for r in cls], gw), runs(cols, ch)
    srow, scol = [i for i, _ in rr], [i for i, _ in cc]
    o_stem = sum(r.count('o') for _, r in rr + cc)
    soft = any(r.count('o') * 4 >= len(r) * 3 for _, r in rr + cc)
    verdict = 'NO-STEM' if not (srow or scol) else 'SOFT' if soft else 'SNAPPED'
    return srow, scol, o_stem, verdict


def analyse(run, w, h, px, geom=None):
    cps = RUNS[run][2]
    if run == 'stress':
        n = sum(1 for i in range(w * h) if px[i * 3:i * 3 + 3] == WHITE)
        print('stress shot %dx%d white pixels %d' % (w, h, n))
        return 0 if n > 2000 else 1

    def at(x, y):
        o = (y * w + x) * 3
        return px[o:o + 3]
    # screenshot scale: the window is WW wide on the screen
    box = [(x, y) for y in range(WY + 13, WY + 40) for x in range(WX + 8, WX + 120)]
    bgc = collections.Counter(at(x, y) for x, y in box).most_common(1)[0][0]
    fgs = [(x, y) for x, y in box if at(x, y) == WHITE]   # the window's own text is grey
    if not fgs:
        print('no glyph pixels found; screenshot', w, h)
        return 1
    xs0 = min(x for x, y in fgs)
    y0 = min(y for x, y in fgs if x == xs0)
    y1 = y0
    while at(xs0, y1 + 1) != bgc:   # the anchor's left column is its full height
        y1 += 1
    fgc = collections.Counter(at(x, y) for x, y in fgs if y0 <= y <= y1).most_common(1)[0][0]
    if geom:   # the cell of an earlier jbmono run, for a font without a full-height anchor
        xs0, y0, cw, ch = geom
        y1 = y0 + ch - 1
    print('shot %dx%d bg=%s fg=%s anchor x0=%d rows %d..%d' % (w, h, bgc.hex(), fgc.hex(), xs0, y0, y1))
    # cell width: the anchor's extent along its widest row
    row = max(range(y0, y1 + 1), key=lambda y: sum(at(x, y) != bgc for x in range(xs0, xs0 + 24)))
    cw = 0
    while at(xs0 + cw, row) != bgc and cw < 24:
        cw += 1
    ch = y1 - y0 + 1
    if geom:
        cw, ch = geom[2], geom[3]
    print('cell %dx%d (anchor row %d is %d wide)' % (cw, ch, row, cw))
    bad = 0
    segs = []
    if run == 'icons':   # icons are wider than a cell: cut the line into glyphs at its empty columns instead of a fixed pitch
        x, xe = xs0, min(w, WX + WW)
        while x < xe:
            if any(at(x, y) != bgc for y in range(y0, y1 + 1)):
                x1 = x
                while x1 < xe and any(at(x1, y) != bgc for y in range(y0, y1 + 1)):
                    x1 += 1
                segs.append((x, x1 - x))
                x = x1
            else:
                x += 1
        segs = segs[:len(cps)]   # a stray window-edge column after the line
        print('glyph segments (x, width):', segs, '(expected %d)' % len(cps))
        if len(segs) != len(cps):
            bad += 1
    for i, cp in enumerate(cps):
        cx = xs0 + 2 * cw * i
        gw = cw
        if run == 'icons' and i < len(segs):
            cx, gw = segs[i]
        cell = [[at(cx + x, y0 + y) for x in range(gw)] for y in range(ch)]
        cls = [['#' if p == fgc else '.' if p == bgc else 'o' for p in r] for r in cell]
        print('U+%04X' % cp)
        for y, r in enumerate(cls):
            print('  %2d %s  fg%d bg%d o%d' % (y, ''.join(r), r.count('#'), r.count('.'), r.count('o')))
        cols = [''.join(cls[y][x] for y in range(ch)) for x in range(gw)]
        print('  cols fg/o:', ' '.join('%d/%d' % (k.count('#'), k.count('o')) for k in cols))
        n_o = sum(r.count('o') for r in cls)
        n_f = sum(r.count('#') for r in cls)
        cw_ = gw
        print('  total fg %d intermediate %d of %d' % (n_f, n_o, gw * ch))
        if cp == 0x2588 and n_f != cw * ch:
            bad += 1
        if cp in (0x2500, 0x2502, 0x253C) and n_o:
            bad += 1
        if run == 'icons' and cp != 0xE0B0:   # straight stems: rows/cols that are at least half fg; intermediate pens beside them
            srow, scol, o_stem, verdict = stem_verdict(cls)
            print('  stems: rows %s cols %s, intermediate on them %d -> %s' % (srow, scol, o_stem, verdict))
    return bad


def main():
    run, out = sys.argv[1], sys.argv[2]
    geom = tuple(int(v) for v in sys.argv[4].split(',')) if len(sys.argv) > 4 else None   # x0,y0,cw,ch
    if len(sys.argv) > 3 and sys.argv[3] == 'again':   # analyse an earlier shot, no rig
        d = pathlib.Path(out + '.rgb').read_bytes()
        w, h = struct.unpack('>HH', d[:4])
        px = d[4:]
    else:
        w, h, px = shoot(run, out)
    sys.exit(1 if analyse(run, w, h, px, geom) else 0)


if __name__ == '__main__':
    main()
