#!/usr/bin/env python3
"""Pixel-grid rasteriser for small SVG icons (plan 2026-11-08-dopus-toolbar-size-blend.md DT-8).

The font path (gen_menu_icons.raster: outline fitted to its bbox with an EDGE margin, then area-averaged) puts a
stroke of 2/24 of the design between pixels at 16 and 32 px. This one draws from the SVG source instead:

- the design grid (the viewBox) maps to the whole target width and height, no bbox fit, no margin;
- the stroke is a round pen of a whole number of target pixels (the caller's `stroke_px`, not the design width), so
  straight strokes are whole pixels wide; on a tall-pixel target (height < width) the pen is that many columns wide
  and half as many rows high (at least 1 row), the same physical thickness both ways;
- every segment end and every curve extreme (the points where a curve runs horizontal or vertical) snaps per axis to
  the pixel grid: to pixel centres when the pen is an odd number of pixels along that axis, to pixel edges when even
  (a fill edge: see _fill_hints, DT-12). A straight horizontal or vertical stroke then covers whole pixels only; the points between
  snapped ends move by the interpolated snap offset, so curves keep their shape and their partial pixels (the 33/66%
  levels of the blend);
- coverage is SUB x SUB samples a pixel: a sample is inked when within the pen's radius of the path (round caps and
  round joins, as Lucide's stroke-linecap/linejoin) or inside a filled element (nonzero winding).

Elements: path (M L H V C S Q T A Z, absolute and relative), line, polyline, polygon, rect (rx, ry), circle, ellipse.
No transforms (none in lucide-static). stroke / fill inherit from the <svg> root; "none" switches either off."""
import bisect
import math
import re
import xml.etree.ElementTree as ET

SUB = 8          # samples a pixel, each way
FLAT = 24        # line pieces per curve piece


def _num_scanner(d):
    """Tokens of path data: command letters and numbers; arc flags are read one character at a time (`a2 2 0 0022 17`)."""
    i, n = 0, len(d)
    num = re.compile(r"[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?")

    def skip():
        nonlocal i
        while i < n and (d[i].isspace() or d[i] == ","):
            i += 1

    def peek():
        skip()
        return d[i] if i < n else ""

    def number():
        nonlocal i
        skip()
        m = num.match(d, i)
        if not m:
            raise ValueError("path data: number expected at %d: %r" % (i, d[i:i + 12]))
        i = m.end()
        return float(m.group())

    def flag():
        nonlocal i
        skip()
        if i >= n or d[i] not in "01":
            raise ValueError("path data: arc flag expected at %d" % i)
        i += 1
        return d[i - 1] == "1"

    def command():
        nonlocal i
        skip()
        i += 1
        return d[i - 1]

    return peek, number, flag, command


# A piece is (kind, args): ("L", p0, p1), ("C", p0, c1, c2, p1), ("A", cx, cy, rx, ry, a0, a1). point(piece, t) evaluates
# it in design units; pieces are split at their extremes before snapping, so the snapped points are the ones the eye
# reads as a stem edge.

def _point(pc, t):
    k = pc[0]
    if k == "L":
        (x0, y0), (x1, y1) = pc[1], pc[2]
        return x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
    if k == "C":
        p0, c1, c2, p1 = pc[1:]
        u = 1 - t
        return (u ** 3 * p0[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t ** 3 * p1[0],
                u ** 3 * p0[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t ** 3 * p1[1])
    cx, cy, rx, ry, a0, a1 = pc[1:]
    a = a0 + (a1 - a0) * t
    return cx + rx * math.cos(a), cy + ry * math.sin(a)


def _split(pc):
    """The piece cut at the parameters where its x or y runs flat (curve extremes), as [(t0, t1)]."""
    ts = []
    if pc[0] == "C":
        p0, c1, c2, p1 = pc[1:]
        for ax in (0, 1):
            # derivative of the cubic / 3: a t^2 + b t + c
            a = -p0[ax] + 3 * c1[ax] - 3 * c2[ax] + p1[ax]
            b = 2 * (p0[ax] - 2 * c1[ax] + c2[ax])
            c = c1[ax] - p0[ax]
            if abs(a) < 1e-9:
                if abs(b) > 1e-9:
                    ts.append(-c / b)
            else:
                disc = b * b - 4 * a * c
                if disc >= 0:
                    r = math.sqrt(disc)
                    ts += [(-b + r) / (2 * a), (-b - r) / (2 * a)]
    elif pc[0] == "A":
        a0, a1 = pc[5], pc[6]
        lo, hi = min(a0, a1), max(a0, a1)
        q = math.ceil(lo / (math.pi / 2) - 1e-9)
        while q * math.pi / 2 < hi - 1e-9:
            ts.append((q * math.pi / 2 - a0) / (a1 - a0))
            q += 1
    cuts = sorted(t for t in ts if 1e-6 < t < 1 - 1e-6)
    edges = [0.0] + cuts + [1.0]
    return list(zip(edges, edges[1:]))


def _arc(p0, rx, ry, rot, large, sweep, p1):
    """SVG endpoint arc -> centre form pieces (rotation 0 only; lucide-static has none other)."""
    if rot % 360:
        raise ValueError("rotated arcs are not supported")
    if p0 == p1:
        return []
    rx, ry = abs(rx), abs(ry)
    if rx == 0 or ry == 0:
        return [("L", p0, p1)]
    x1p, y1p = (p0[0] - p1[0]) / 2, (p0[1] - p1[1]) / 2
    lam = (x1p / rx) ** 2 + (y1p / ry) ** 2
    if lam > 1:
        s = math.sqrt(lam)
        rx, ry = rx * s, ry * s
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    co = math.sqrt(max(0.0, num / den)) * (-1 if large == sweep else 1)
    cxp, cyp = co * rx * y1p / ry, -co * ry * x1p / rx
    cx, cy = cxp + (p0[0] + p1[0]) / 2, cyp + (p0[1] + p1[1]) / 2
    a0 = math.atan2((y1p - cyp) / ry, (x1p - cxp) / rx)
    a1 = math.atan2((-y1p - cyp) / ry, (-x1p - cxp) / rx)
    da = a1 - a0
    if sweep and da < 0:
        da += 2 * math.pi
    elif not sweep and da > 0:
        da -= 2 * math.pi
    return [("A", cx, cy, rx, ry, a0, a0 + da)]


def path_pieces(d):
    """[subpath], each (closed, [piece]) of SVG path data."""
    peek, number, flag, command = _num_scanner(d)
    subs, cur, closed = [], [], False
    pos = start = (0.0, 0.0)
    last_c = last_q = None
    cmd = None
    while True:
        c = peek()
        if not c:
            break
        if c.isalpha():
            cmd = command()
        elif cmd is None:
            raise ValueError("path data starts without a command")
        rel = cmd.islower()
        k = cmd.upper()
        ox, oy = pos if rel else (0.0, 0.0)
        prev_c, prev_q = last_c, last_q
        last_c = last_q = None
        if k == "M":
            if cur:
                subs.append((closed, cur))
            cur, closed = [], False
            pos = start = (ox + number(), oy + number())
            cmd = "l" if rel else "L"
        elif k in "LHV":
            if k == "L":
                p = (ox + number(), oy + number())
            elif k == "H":
                p = ((pos[0] if rel else 0.0) + number(), pos[1])
            else:
                p = (pos[0], (pos[1] if rel else 0.0) + number())
            cur.append(("L", pos, p))
            pos = p
        elif k in "CS":
            if k == "C":
                c1 = (ox + number(), oy + number())
            else:
                c1 = (2 * pos[0] - prev_c[0], 2 * pos[1] - prev_c[1]) if prev_c else pos
            c2 = (ox + number(), oy + number())
            p = (ox + number(), oy + number())
            cur.append(("C", pos, c1, c2, p))
            last_c, pos = c2, p
        elif k in "QT":
            if k == "Q":
                q = (ox + number(), oy + number())
            else:
                q = (2 * pos[0] - prev_q[0], 2 * pos[1] - prev_q[1]) if prev_q else pos
            p = (ox + number(), oy + number())
            c1 = (pos[0] + 2 / 3 * (q[0] - pos[0]), pos[1] + 2 / 3 * (q[1] - pos[1]))
            c2 = (p[0] + 2 / 3 * (q[0] - p[0]), p[1] + 2 / 3 * (q[1] - p[1]))
            cur.append(("C", pos, c1, c2, p))
            last_q, pos = q, p
        elif k == "A":
            rx, ry, rot = number(), number(), number()
            large, sweep = flag(), flag()
            p = (ox + number(), oy + number())
            cur += _arc(pos, rx, ry, rot, large, sweep, p)
            pos = p
        elif k == "Z":
            if pos != start:
                cur.append(("L", pos, start))
            pos, closed = start, True
            subs.append((closed, cur))
            cur, closed = [], False
            cmd = None
        else:
            raise ValueError("path command %r is not supported" % cmd)
    if cur:
        subs.append((closed, cur))
    return subs


def _ellipse(cx, cy, rx, ry):
    return [(True, [("A", cx, cy, rx, ry, 0.0, 2 * math.pi)])]


def _rect(x, y, w, h, rx, ry):
    if rx is None and ry is None:
        rx = ry = 0.0
    rx = ry if rx is None else rx
    ry = rx if ry is None else ry
    rx, ry = min(rx, w / 2), min(ry, h / 2)
    if rx == 0 or ry == 0:
        return path_pieces("M%r %r h%r v%r h%r z" % (x, y, w, h, -w))
    return path_pieces("M%r %r H%r A%r %r 0 0 1 %r %r V%r A%r %r 0 0 1 %r %r H%r A%r %r 0 0 1 %r %r V%r A%r %r 0 0 1 %r %r z"
                       % (x + rx, y, x + w - rx, rx, ry, x + w, y + ry, y + h - ry, rx, ry, x + w - rx, y + h,
                          x + rx, rx, ry, x, y + h - ry, y + ry, rx, ry, x + rx, y))


def _pts(s):
    v = [float(t) for t in re.findall(r"[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?", s)]
    return list(zip(v[0::2], v[1::2]))


def parse(svg_text):
    """(viewBox (x, y, w, h), [(stroked, filled, [subpath])]) of an SVG document."""
    root = ET.fromstring(svg_text)
    ns = lambda tag: tag.rsplit("}", 1)[-1]
    vb = [float(v) for v in root.get("viewBox", "0 0 24 24").replace(",", " ").split()]
    out = []

    def walk(node, stroke, fill):
        stroke = node.get("stroke", stroke)
        fill = node.get("fill", fill)
        tag = ns(node.tag)
        g = lambda a, dflt=0.0: float(node.get(a, dflt))
        subs = None
        if tag == "path":
            subs = path_pieces(node.get("d", ""))
        elif tag == "line":
            subs = [(False, [("L", (g("x1"), g("y1")), (g("x2"), g("y2")))])]
        elif tag in ("polyline", "polygon"):
            p = _pts(node.get("points", ""))
            pcs = [("L", a, b) for a, b in zip(p, p[1:])]
            if tag == "polygon" and len(p) > 2:
                pcs.append(("L", p[-1], p[0]))
            subs = [(tag == "polygon", pcs)]
        elif tag == "rect":
            rx, ry = node.get("rx"), node.get("ry")
            subs = _rect(g("x"), g("y"), g("width"), g("height"), None if rx is None else float(rx), None if ry is None else float(ry))
        elif tag == "circle":
            subs = _ellipse(g("cx"), g("cy"), g("r"), g("r"))
        elif tag == "ellipse":
            subs = _ellipse(g("cx"), g("cy"), g("rx"), g("ry"))
        if subs is not None:
            out.append((stroke not in (None, "none"), fill not in (None, "none"), subs))
        for ch in node:
            walk(ch, stroke, fill)

    walk(root, None, "black")
    return tuple(vb), out


def _snap(v, odd):
    return math.floor(v) + 0.5 if odd else float(round(v))


def _fill_hints(subs, to_px, unit_x):
    """Per-axis maps (x, y) from target pixels to hinted target pixels for a filled-only element.

    A fill edge snaps to a pixel edge. A stem (the ink or the gap between two facing straight edges, horizontal or
    vertical, whose spans overlap) keeps a width of a whole number of units, at least one: a unit is one row in y and
    w / h columns in x on a tall-pixel grid (one column on a square one). So a bar and a gap of one physical thickness
    come out the same thickness both ways, and a thin cut-out (a + in a folder, a tick) stays open. Every other
    segment end and curve extreme rounds to the nearest pixel edge; the points between move piecewise-linearly with
    their neighbours, so curves keep their shape and their partial pixels."""
    keys = ([], [])
    edges = ([], [])          # per axis: (coordinate, span lo, span hi, direction)
    for closed, pieces in subs:
        for pc in pieces:
            for t0, t1 in _split(pc):
                a, b = to_px(_point(pc, t0)), to_px(_point(pc, t1))
                for ax in (0, 1):
                    keys[ax].extend((a[ax], b[ax]))
                    o = 1 - ax
                    if pc[0] == "L" and abs(a[ax] - b[ax]) < 1e-6 and abs(a[o] - b[o]) > 1e-6:
                        edges[ax].append((a[ax], min(a[o], b[o]), max(a[o], b[o]), 1 if b[o] > a[o] else -1))
    return tuple(_axis_map(keys[ax], edges[ax], (unit_x, 1.0)[ax]) for ax in (0, 1))


def _axis_map(keys, edges, unit):
    pairs = []
    for e in edges:
        best = None
        for f in edges:
            d = f[0] - e[0]
            if d > 1e-6 and min(e[2], f[2]) - max(e[1], f[1]) > 1e-6 and (best is None or d < best[0]):
                best = (d, f)
        if best and best[1][3] != e[3]:
            pairs.append((best[0], e[0], best[1][0]))
    k = lambda v: round(v, 6)
    tgt = {}
    for d, a, b in sorted(pairs):
        width = unit * max(1, round(d / unit))
        if k(a) in tgt and k(b) in tgt:
            continue
        if k(a) in tgt:
            tgt[k(b)] = tgt[k(a)] + width
        elif k(b) in tgt:
            tgt[k(a)] = tgt[k(b)] - width
        else:
            tgt[k(a)] = float(round((a + b) / 2 - width / 2))
            tgt[k(b)] = tgt[k(a)] + width
    for v in keys:
        tgt.setdefault(k(v), float(round(v)))
    src = sorted(tgt)
    dst = [tgt[v] for v in src]
    for i in range(1, len(dst)):        # keep the map monotonic: hinting never folds the outline over
        dst[i] = max(dst[i], dst[i - 1])

    def m(v):
        i = bisect.bisect_left(src, v)
        if i == 0:
            return v + dst[0] - src[0]
        if i == len(src):
            return v + dst[-1] - src[-1]
        s0, s1 = src[i - 1], src[i]
        return dst[i - 1] + (dst[i] - dst[i - 1]) * (v - s0) / (s1 - s0) if s1 > s0 else dst[i]
    return m


def coverage(svg_text, w, h, stroke_px):
    """h rows of w coverages 0..1 of the SVG drawn on a w x h pixel grid (see the module doc). stroke_px: the pen's
    width in target columns; on a tall-pixel grid (h < w) its height is stroke_px * h / w rows, at least 1."""
    (vx, vy, vw, vh), shapes = parse(svg_text)
    sx, sy = w / vw, h / vh
    pen_x = stroke_px
    pen_y = max(1, round(stroke_px * h / w)) if h != w else stroke_px
    # the pen is round in a space where y is stretched by pen_x / pen_y: distances are measured there
    k = pen_x / pen_y
    radius = pen_x / 2.0
    n_x, n_y = w * SUB, h * SUB
    ink = bytearray(n_x * n_y)

    def to_px(p):
        return (p[0] - vx) * sx, (p[1] - vy) * sy

    for stroked, filled, subs in shapes:
        odd_x = stroked and pen_x % 2 == 1
        odd_y = stroked and pen_y % 2 == 1
        snap = lambda p: (_snap(p[0], odd_x), _snap(p[1], odd_y))
        polys = []
        if filled and not stroked:
            mx, my = _fill_hints(subs, to_px, w / h if h < w else 1.0)
            for closed, pieces in subs:
                pts = []
                for pc in pieces:
                    for t0, t1 in _split(pc):
                        for i in range(0 if not pts else 1, (1 if pc[0] == "L" else FLAT) + 1):
                            p = to_px(_point(pc, t0 + (t1 - t0) * i / (1 if pc[0] == "L" else FLAT)))
                            q = (mx(p[0]), my(p[1]))
                            if not pts or q != pts[-1]:
                                pts.append(q)
                if pts:
                    polys.append((closed, pts))
            _fill(ink, n_x, n_y, [p for c, p in polys])
            continue
        for closed, pieces in subs:
            pts = []
            for pc in pieces:
                for t0, t1 in _split(pc):
                    a, b = to_px(_point(pc, t0)), to_px(_point(pc, t1))
                    sa, sb = snap(a), snap(b)
                    da, db = (sa[0] - a[0], sa[1] - a[1]), (sb[0] - b[0], sb[1] - b[1])
                    steps = 1 if pc[0] == "L" else FLAT
                    for i in range(0 if not pts else 1, steps + 1):
                        u = i / steps
                        p = to_px(_point(pc, t0 + (t1 - t0) * u))
                        q = (p[0] + da[0] + (db[0] - da[0]) * u, p[1] + da[1] + (db[1] - da[1]) * u)
                        if not pts or q != pts[-1]:
                            pts.append(q)
            if pts:
                polys.append((closed, pts))
        if filled:
            _fill(ink, n_x, n_y, [p for c, p in polys])
        if stroked:
            for closed, pts in polys:
                segs = list(zip(pts, pts[1:] + (pts[:1] if closed else [])))
                if not segs:
                    segs = [(pts[0], pts[0])]
                for a, b in segs:
                    _stroke(ink, n_x, n_y, a, b, radius, k)
    full = SUB * SUB
    out = [[0.0] * w for _ in range(h)]
    for y in range(n_y):
        row = out[y // SUB]
        base = y * n_x
        for x in range(n_x):
            if ink[base + x]:
                row[x // SUB] += 1.0 / full
    return out


def _stroke(ink, n_x, n_y, a, b, r, k):
    """Ink every sample within r of segment a-b, distances measured with y scaled by k (the round pen)."""
    ax, ay, bx, by = a[0], a[1] * k, b[0], b[1] * k
    dx, dy = bx - ax, by - ay
    ll = dx * dx + dy * dy
    r2 = r * r
    x_lo = max(0, int((min(ax, bx) - r) * SUB))
    x_hi = min(n_x - 1, int((max(ax, bx) + r) * SUB) + 1)
    y_lo = max(0, int((min(ay, by) - r) / k * SUB))
    y_hi = min(n_y - 1, int((max(ay, by) + r) / k * SUB) + 1)
    for syi in range(y_lo, y_hi + 1):
        py = (syi + 0.5) / SUB * k
        base = syi * n_x
        for sxi in range(x_lo, x_hi + 1):
            px = (sxi + 0.5) / SUB
            if ll:
                t = ((px - ax) * dx + (py - ay) * dy) / ll
                t = 0.0 if t < 0 else 1.0 if t > 1 else t
                ex, ey = px - (ax + t * dx), py - (ay + t * dy)
            else:
                ex, ey = px - ax, py - ay
            if ex * ex + ey * ey <= r2:
                ink[base + sxi] = 1


def _fill(ink, n_x, n_y, polys):
    """Ink the samples inside the polygons, nonzero winding (all subpaths of one element together)."""
    edges = [(a, b) for poly in polys for a, b in zip(poly, poly[1:] + poly[:1]) if a[1] != b[1]]
    for syi in range(n_y):
        y = (syi + 0.5) / SUB
        xs = sorted((a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]), 1 if b[1] > a[1] else -1)
                    for a, b in edges if (a[1] <= y < b[1]) or (b[1] <= y < a[1]))
        wind = 0
        for i, (x, d) in enumerate(xs[:-1]):
            wind += d
            if wind:
                for sxi in range(max(0, math.ceil(x * SUB - 0.5)), min(n_x, math.ceil(xs[i + 1][0] * SUB - 0.5))):
                    ink[syi * n_x + sxi] = 1
