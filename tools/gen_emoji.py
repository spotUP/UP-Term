#!/usr/bin/env python3
"""Twemoji's 72 px PNGs -> UP-Term's colour emoji pages (ledger U4,
thoughts/shared/plans/2026-10-05-emoji-colour.md).

Input: a Twemoji release archive (the GitHub tag's .tar.gz; its
assets/72x72/<hex>.png are read straight from it). Taken: every file named
by ONE code point whose width in the engine's table (engine/vtwidth.h) is 2
-- the renderer draws colour only over two cells, so a narrow one (U+263A,
U+2764 without VS16) would never be drawn. Sequences (ZWJ, skin tones,
flags, "-fe0f") are left out: the first pass draws the base code point.

Each emoji is area-averaged (premultiplied alpha, exact for 72 -> 16 and
72 -> 8) to 16x16 and to 16x8 (the variant for 8-pixel rows), then both
sizes get ONE palette: up to 16 colours (4-bit indices) when every pixel
stays within MAX_ERR of the art blended over black and over white, else
the exact colours (8-bit indices, at most 255).

One file per 256-code-point page that has an emoji, named as Unifont's
pages are ("1F6", "26"); render/emoji.c reads them. Layout, big-endian:

  0   4  "UCE1"
  4   2  page (the code point >> 8)
  6   2  emoji in the page
  8   4  the file's length
  12 32  present bits: code point lo is bit 0x80 >> (lo & 7) of byte lo >> 3
  44 32  8 x u32: the offset of the first glyph at or after each
         32-code-point group (lo & ~31), from the start of the file
  76     the glyphs in code-point order, each:
           0  1  bits an index, 4 or 8
           1  1  palette entries - 1 (1 to 16 at 4 bits, 1 to 255 at 8)
           2  4n the palette, R G B A (A 0 transparent .. 255 opaque, colour
                 not premultiplied)
           .     16x16 indices, rows top down, at 4 bits the left pixel in
                 the high nibble
           .     16x8 indices, the same way

Output is deterministic: the same archive gives the same bytes.

usage: gen_emoji.py <twemoji.tar.gz> <out-dir>
"""
import pathlib
import re
import struct
import sys
import tarfile
import zlib

MAGIC = b"UCE1"
HEADER = 76
SIZES = ((16, 16), (16, 8))
MAX_ERR = 48          # the most a 4-bit glyph's pixel may move, of 255, over black or white
PAGE_MAX = 0x40000    # render/emoji.h CE_PAGE_MAX
ROOT = pathlib.Path(__file__).resolve().parent.parent


# ---- the engine's widths ------------------------------------------------------

def _c_array(src, name):
    m = re.search(r"%s\[[^\]]*\](?:\[[^\]]*\])? = \{(.*?)\n\};" % name, src, re.S)
    if not m:
        raise SystemExit("vtwidth.h: no %s" % name)
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    return [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]


def width_fn(path=ROOT / "engine/vtwidth.h"):
    """vt_char_width (engine/vtwidth.h) for code points, from the header itself."""
    src = pathlib.Path(path).read_text()
    page = _c_array(src, "vt_width_page")
    bits = _c_array(src, "vt_width_bits")

    def width(c):
        if c < 0x300:
            return 1
        if c >= 0x40000:
            return 0 if 0xE0000 <= c <= 0xE0FFF else 1
        p = page[c >> 8]
        if p < 3:
            return p
        return (bits[(p - 3) * 64 + ((c & 0xFF) >> 2)] >> ((c & 3) << 1)) & 3
    return width


# ---- PNG ------------------------------------------------------------------------

def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(data):
    """(w, h, [(r, g, b, a)] row by row) of a non-interlaced PNG: palette
    (1-8 bits, tRNS), grey, RGB or RGBA at 8 bits."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat, plte, trns = 8, b"", None, b""
    w = h = depth = ctype = None
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", chunk)
            if inter:
                raise ValueError("interlaced PNG")
        elif kind == b"PLTE":
            plte = chunk
        elif kind == b"tRNS":
            trns = chunk
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
    chans = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    if ctype != 3 and depth != 8:
        raise ValueError("%d-bit colour type %d" % (depth, ctype))
    bpp = max(1, chans * depth // 8)
    stride = (w * chans * depth + 7) // 8
    raw = zlib.decompress(idat)
    prev = bytearray(stride)
    out = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 0xFF
            elif f == 2:
                line[i] = (line[i] + b) & 0xFF
            elif f == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif f == 4:
                line[i] = (line[i] + _paeth(a, b, c)) & 0xFF
        prev = line
        for x in range(w):
            if ctype == 3:
                per = 8 // depth
                v = (line[x // per] >> ((per - 1 - x % per) * depth)) & ((1 << depth) - 1)
                al = trns[v] if v < len(trns) else 255
                out.append((plte[3 * v], plte[3 * v + 1], plte[3 * v + 2], al))
            elif ctype == 6:
                out.append(tuple(line[4 * x:4 * x + 4]))
            elif ctype == 2:
                out.append(tuple(line[3 * x:3 * x + 3]) + (255,))
            elif ctype == 4:
                out.append((line[2 * x],) * 3 + (line[2 * x + 1],))
            else:
                out.append((line[x],) * 3 + (255,))
    return w, h, out


# ---- scaling, palette ----------------------------------------------------------------

def area(w, h, px, W, H):
    """px (w x h RGBA) averaged over each of W x H boxes, premultiplied:
    straight-alpha RGBA, a fully transparent pixel (0, 0, 0, 0)."""
    out = []
    for oy in range(H):
        y0, y1 = oy * h / H, (oy + 1) * h / H
        for ox in range(W):
            x0, x1 = ox * w / W, (ox + 1) * w / W
            r = g = b = a = tot = 0.0
            for sy in range(int(y0), min(h, int(y1 + 0.999999))):
                fy = min(y1, sy + 1) - max(y0, sy)
                row = sy * w
                for sx in range(int(x0), min(w, int(x1 + 0.999999))):
                    f = (min(x1, sx + 1) - max(x0, sx)) * fy
                    pr, pg, pb, pa = px[row + sx]
                    pa *= f
                    r += pr * pa
                    g += pg * pa
                    b += pb * pa
                    a += pa
                    tot += f
            al = int(a / tot + 0.5)
            if al == 0:
                out.append((0, 0, 0, 0))
            else:
                out.append((int(r / a + 0.5), int(g / a + 0.5), int(b / a + 0.5), al))
    return out


def over(p, bg):
    """p over the background bg (RGB): the RGB shown, as render/emoji.c's ce_blend."""
    a = p[3]
    return tuple((p[i] * a + bg[i] * (255 - a) + 127) // 255 for i in range(3))


def error(p, q):
    """The most p and q differ, blended over black and over white."""
    return max(abs(x - y) for bg in ((0, 0, 0), (255, 255, 255)) for x, y in zip(over(p, bg), over(q, bg)))


def kmeans(colours, weight, k, iters=10):
    """k colours for the given ones (weights: their pixel counts), premultiplied
    k-means; {colour: its palette colour}. Transparent stays transparent."""
    def pm(c):
        return (c[0] * c[3] / 255, c[1] * c[3] / 255, c[2] * c[3] / 255, float(c[3]))

    pts = [(pm(c), weight[c], c) for c in colours if c[3]]
    k -= 1 if len(pts) < len(colours) else 0  # an entry for transparent
    # the most used first, then each time the one worst served (weighted)
    cent = [max(pts, key=lambda q: (q[1], q[2]))[0]]
    dist = [sum((v[i] - cent[0][i]) ** 2 for i in range(4)) for v, _, _ in pts]
    while len(cent) < k:
        j = max(range(len(pts)), key=lambda i: (dist[i] * pts[i][1], pts[i][2]))
        c = pts[j][0]
        cent.append(c)
        dist = [min(d, sum((v[i] - c[i]) ** 2 for i in range(4))) for d, (v, _, _) in zip(dist, pts)]
    near = [0] * len(pts)
    for _ in range(iters):
        for n, (v, _, _) in enumerate(pts):
            near[n] = min(range(len(cent)), key=lambda j: sum((v[i] - cent[j][i]) ** 2 for i in range(4)))
        acc = [[0.0] * 5 for _ in cent]
        for n, (v, w, _) in enumerate(pts):
            a = acc[near[n]]
            for i in range(4):
                a[i] += v[i] * w
            a[4] += w
        cent = [tuple(a[i] / a[4] for i in range(4)) if a[4] else cent[j] for j, a in enumerate(acc)]
    for n, (v, _, _) in enumerate(pts):
        near[n] = min(range(len(cent)), key=lambda j: sum((v[i] - cent[j][i]) ** 2 for i in range(4)))

    def straight(c):
        a = int(c[3] + 0.5)
        if a == 0:
            return (0, 0, 0, 0)
        return tuple(min(255, int(c[i] * 255 / c[3] + 0.5)) for i in range(3)) + (a,)

    m = {c: c for c in colours if not c[3]}
    for n, (_, _, c) in enumerate(pts):
        m[c] = straight(cent[near[n]])
    return m


def palette_of(images):
    """(bits, palette, [indices per image]) for images that share one palette."""
    px = [p for im in images for p in im]
    weight = {}
    for p in px:
        weight[p] = weight.get(p, 0) + 1
    colours = sorted(weight)
    if len(colours) <= 16:
        m, bits = {c: c for c in colours}, 4
    else:
        m = kmeans(colours, weight, 16)
        bits = 4
        if max(error(p, m[p]) for p in colours) > MAX_ERR:
            bits = 8
            m = {c: c for c in colours} if len(colours) <= 255 else kmeans(colours, weight, 255)
    pal = sorted(set(m.values()))
    index = {c: i for i, c in enumerate(pal)}
    return bits, pal, [[index[m[p]] for p in im] for im in images]


def glyph_bytes(w, h, px):
    """One emoji's glyph record (the layout above) from its 72 px RGBA."""
    images = [area(w, h, px, W, H) for W, H in SIZES]
    bits, pal, idx = palette_of(images)
    out = bytearray([bits, len(pal) - 1])
    for c in pal:
        out += bytes(c)
    for im in idx:
        if bits == 8:
            out += bytes(im)
        else:
            out += bytes((im[i] << 4) | im[i + 1] for i in range(0, len(im), 2))
    return bytes(out)


def decode_glyph(g):
    """(bits, palette, [RGBA per image]) of a glyph record: the converter's
    inverse, for the tests."""
    bits, n = g[0], g[1] + 1
    pal = [tuple(g[2 + 4 * i:6 + 4 * i]) for i in range(n)]
    pos = 2 + 4 * n
    images = []
    for W, H in SIZES:
        cnt = W * H
        if bits == 8:
            ix = list(g[pos:pos + cnt])
            pos += cnt
        else:
            ix = [v for b in g[pos:pos + cnt // 2] for v in (b >> 4, b & 15)]
            pos += cnt // 2
        images.append([pal[i] for i in ix])
    return bits, pal, images


def glyph_size(g, off=0):
    bits, n = g[off], g[off + 1] + 1
    return 2 + 4 * n + sum(W * H for W, H in SIZES) * bits // 8


# ---- pages ------------------------------------------------------------------------------

def page_name(page):
    """The page's file name: "26" in the BMP, "1F6" in plane 1 (as Unifont's)."""
    return "%02X" % page if page < 0x100 else "%03X" % page


def page_bytes(page, glyphs):
    """The page file for high byte `page`, or None when it has no emoji."""
    present = bytearray(32)
    groups = []
    body = bytearray()
    count = 0
    for lo in range(256):
        if lo % 32 == 0:
            groups.append(HEADER + len(body))
        g = glyphs.get(page << 8 | lo)
        if g is None:
            continue
        present[lo >> 3] |= 0x80 >> (lo & 7)
        body += g
        count += 1
    if not count:
        return None
    total = HEADER + len(body)
    if total > PAGE_MAX:
        raise SystemExit("page %s: %d bytes, more than %d" % (page_name(page), total, PAGE_MAX))
    head = MAGIC + struct.pack(">HHI", page, count, total) + bytes(present) + struct.pack(">8I", *groups)
    assert len(head) == HEADER
    return head + bytes(body)


def read_archive(path, width):
    """{code point: glyph record} for the archive's single-code-point,
    two-cell emoji."""
    glyphs = {}
    name_re = re.compile(r"(?:^|/)assets/72x72/([0-9a-f]{2,6})\.png$")
    with tarfile.open(path) as tar:
        for m in tar:
            mm = name_re.search(m.name)
            if not mm or not m.isfile():
                continue
            cp = int(mm.group(1), 16)
            if width(cp) != 2:
                continue
            w, h, px = read_png(tar.extractfile(m).read())
            glyphs[cp] = glyph_bytes(w, h, px)
    return glyphs


def convert(archive, out_dir, width=None):
    glyphs = read_archive(archive, width or width_fn())
    out = pathlib.Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for pat in ("[0-9A-F][0-9A-F]", "[0-9A-F][0-9A-F][0-9A-F]"):
        for old in out.glob(pat):
            old.unlink()  # a page the new archive no longer has must not linger
    pages = {}
    for page in sorted({cp >> 8 for cp in glyphs}):
        b = page_bytes(page, glyphs)
        (out / page_name(page)).write_bytes(b)
        pages[page] = len(b)
    return pages, glyphs


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pages, glyphs = convert(sys.argv[1], sys.argv[2])
    four = sum(1 for g in glyphs.values() if g[0] == 4)
    print("[OK] %d emoji in %d pages, %d bytes (largest %d; %d at 4 bits, %d at 8) in %s" %
          (len(glyphs), len(pages), sum(pages.values()), max(pages.values()), four, len(glyphs) - four,
           sys.argv[2]))


if __name__ == "__main__":
    main()
