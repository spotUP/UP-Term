#!/usr/bin/env python3
"""outline_rig.py -- ledger F1 on the rig: glyphs the bitmap font lacks come
from an outline font (plan thoughts/shared/plans/2026-10-03-outline-fonts.md).

  1. ttf.library 0.8.5 (Aminet util/libs/ttflib68020) and Symbols Nerd Font
     Mono (nerd-fonts, MIT) fetched into build/third_party once; on the rig
     they are used from VTC: (LIBS: and FONTS: assigned with ADD -- the rig's
     system disk is not touched), the font installed by ttf.library's own
     ttfinstall into RAM:.
  2. The same UTF-8 line typed out in an XCON:/XTERM window twice: with a
     profile naming no fallback font, then with font-fallback naming it.
     The two screenshots differ exactly in the icon cells: eight Nerd Font
     icons drawn as glyphs where the first run drew '?'; a CJK character
     the font lacks and an em dash (Latin-1 stand-in '-') do not change, nor
     does the ASCII around them.
  3. The time from the command to the drawn line, with the font (the
     worker's first glyphs: engine open, one rasterisation per icon).

The profile lives in ENV:up-term/up-term only; the file there before is put
back at the end. Run with the rig up and the handler installed:
  python3 tools/rig/outline_rig.py
"""
import pathlib, shutil, struct, subprocess, sys, time, urllib.request, zipfile, io
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / 'build/rig/vtc'
TP = ROOT / 'build/third_party/f1'
TTFLIB = 'https://aminet.net/util/libs/ttflib68020.lha'
NERD = 'https://github.com/ryanoasis/nerd-fonts/releases/latest/download/NerdFontsSymbolsOnly.zip'
ICONS = [0xE0A0, 0xF07B, 0xF121, 0xE7A8, 0xF015, 0xE712, 0xF09B, 0xF1D3]
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
    """ttf.library, ttfinstall and the font, once"""
    TP.mkdir(parents=True, exist_ok=True)
    lib, inst, ttf = TP / 'ttf.library', TP / 'ttfinstall', TP / 'SymNerdMono.ttf'
    if not lib.exists() or not inst.exists():
        outer = TP / 'ttflib68020.lha'
        outer.write_bytes(urllib.request.urlopen(TTFLIB).read())
        subprocess.run(['lha', 'xqw=' + str(TP), str(outer)], check=True)
        inner = next(TP.glob('ttflib68020_*.lha'))
        subprocess.run(['lha', 'xqw=' + str(TP), str(inner)], check=True)
        shutil.copyfile(TP / 'ttflib68020/ttf.library', lib)
        shutil.copyfile(TP / 'ttflib68020/ttfinstall', inst)
    if not ttf.exists():
        z = zipfile.ZipFile(io.BytesIO(urllib.request.urlopen(NERD).read()))
        ttf.write_bytes(z.read('SymbolsNerdFontMono-Regular.ttf'))
    for f in (lib, inst, ttf):
        shutil.copyfile(f, VTC / f.name)


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


def run_line(profile):
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
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)
    return w, h, px, secs


def main():
    fetch()
    # one line: ASCII, the icons (a blank between each), CJK, em dash, ASCII
    text = 'A ' + ' '.join(chr(i) for i in ICONS) + ' ' + chr(CJK) + ' ' + chr(EMDASH) + ' Z\n'
    (VTC / 'outline-line.txt').write_bytes(text.encode('utf-8'))
    had = c.run('Copy ENV:up-term/up-term RAM:outline-rig.bak QUIET')[0] == 0
    c.run('MakeDir ENV:up-term QUIET')
    try:
        c.run('Assign LIBS: VTC: ADD')
        c.run('MakeDir RAM:f1fonts QUIET')
        c.run('Assign FONTS: RAM:f1fonts ADD')
        rc, out = c.run('VTC:ttfinstall VTC:SymNerdMono.ttf RAM:f1fonts', 120)
        names = [l.strip() for l in c.run('List RAM:f1fonts PAT #?.otag LFORMAT "%n"')[1].splitlines() if l.strip()]
        check(rc == 0 and names, 'ttfinstall made an .otag for the font', (rc, out, names))
        if not names:
            return 1
        face = names[0][:-5]
        print('  face:', face)
        w, h, plain, _ = run_line('[profile default]\n')
        w, h, fancy, secs = run_line('[profile default]\nfont-fallback = %s\n' % face)
        # the window's text area: 4 pixels in, under an 11-pixel title bar
        # (topaz 8 Workbench). The shell's number differs between the two
        # windows (its banner and prompt): the typed line is the row that
        # changed most.
        rows = diff_cells(plain, fancy, w, WX + 4, WY + 11, WX + WW - 18, WY + WH - 2)
        line = max(rows, key=lambda k: len(rows[k])) if rows else None
        cols = rows.get(line, [])
        want = [2 + 2 * i for i in range(len(ICONS))]
        check(cols == want, 'the eight icons are drawn from the outline font, nothing else on their line moves',
              'changed columns %s, icons at %s (all rows: %s)' % (cols, want, rows))
        check(2 + 2 * len(ICONS) not in cols, 'the CJK character the font lacks keeps its \'?\'', cols)
        check(4 + 2 * len(ICONS) not in cols, 'the em dash keeps its stand-in', cols)
        print('  line with the font drawn within %.1f s of Return (6 s wait)' % secs)
        ami.png(str(ROOT / 'build/rig/outline_plain.png'), w, h,
                [plain[r * w * 3:(r + 1) * w * 3] for r in range(h)])
        ami.png(str(ROOT / 'build/rig/outline_fancy.png'), w, h,
                [fancy[r * w * 3:(r + 1) * w * 3] for r in range(h)])
    finally:
        if had:
            c.run('Copy RAM:outline-rig.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:outline-rig.bak QUIET')
        c.run('Assign FONTS: RAM:f1fonts REMOVE')
        c.run('Assign LIBS: VTC: REMOVE')
        (VTC / 'outline-rig.conf').unlink(missing_ok=True)
    print('outline_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
