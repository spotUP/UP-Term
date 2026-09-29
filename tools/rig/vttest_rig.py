#!/usr/bin/env python3
"""vttest on the rig (ledger A6): every captured vttest screen
(tests/streams/vttest-*.bin, the bytes vttest wrote, replayed from its
start) is shown in an 80x24 XCON window on the running rig, screenshotted,
and compared cell by cell with the engine's grid for it (build/vtdump):
the glyph in each cell must be the font's glyph for the expected
character, drawn in the expected colour, with bold, underline and inverse
applied. Cells the renderer draws itself (DEC line graphics, anything
above ASCII) and double-size rows are counted, not compared; a blinking
cell may be caught in either phase.

  vttest_rig.py [--out DIR] [name ...]   (default: every vttest stream)

Needs the rig up (rig.py start) and build/vtdump. Results: one line per
screen, and the screenshots in DIR (default the scratch folder)."""
import pathlib, struct, subprocess, sys, time, zlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG = ROOT / "build/rig"
sys.path.insert(0, str(ROOT / "tools/rig"))
import ami

W, H = 662, 212          # an XCON window of exactly 80x24 topaz 8 cells (CSI 18t)
TOP = 12                 # the window's top edge on the screen
CW, CH = 8, 8
FG, BG = (0xC0, 0xC0, 0xC0), (0, 0, 0)   # XCON's default colours
ANSI16 = [0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
          0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00, 0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF]


def rgb(v):
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def palette(i):
    if i < 16:
        return rgb(ANSI16[i])
    if i < 232:
        i -= 16
        lv = [0, 0x5F, 0x87, 0xAF, 0xD7, 0xFF]
        return (lv[i // 36], lv[i // 6 % 6], lv[i % 6])
    g = 8 + (i - 232) * 10
    return (g, g, g)


def colour(c, default):
    if c == 0x100 or c == 0x101:
        return default
    if c & 0x1000000:
        return rgb(c & 0xFFFFFF)
    return palette(c & 255)


def read_png(path):
    b = open(path, "rb").read()
    i, idat, w, h = 8, b"", 0, 0
    while i < len(b):
        n = struct.unpack(">I", b[i:i + 4])[0]
        t, d = b[i + 4:i + 8], b[i + 8:i + 8 + n]
        if t == b"IHDR":
            w, h = struct.unpack(">II", d[:8])
        elif t == b"IDAT":
            idat += d
        i += 12 + n
    raw = zlib.decompress(idat)
    stride = 1 + w * 3
    return w, h, lambda x, y: tuple(raw[y * stride + 1 + x * 3:y * stride + 4 + x * 3])


def run(cmd, secs=10):
    return ami.req(0x02, secs.to_bytes(2, "big") + cmd.encode("latin-1"))


def show(path_on_amiga, shot):
    """Write the file to the window with vtshow (it swallows the replies the
    stream's queries draw and holds the prompt back), screenshot until two
    shots in a row are the same -- the handler draws frame by frame, and a
    long replay takes seconds -- then let vtshow go."""
    ami.req(0x08, bytes([4]) + ("VTC:vtshow %s 120\r" % path_on_amiga).encode())
    seen = []
    for _ in range(60):
        time.sleep(1.5)
        subprocess.run([sys.executable, str(ROOT / "tools/rig/ami.py"), "shot", str(shot),
                        "0", str(TOP), str(W), str(H)], check=True, capture_output=True)
        cur = shot.read_bytes()
        if cur in seen[-2:]:
            break              # unchanged, or back to the shot before: a blink
        seen.append(cur)
    run("Echo >RAM:vtshow.stop")
    for _ in range(50):          # vtshow deletes the file as it leaves
        time.sleep(0.3)
        if b"vtshow.stop" not in run("List RAM:vtshow.stop QUICK NOHEAD", 5)[4:]:
            break
    time.sleep(0.5)


def origin_and_glyphs(scratch):
    """The text area's top left, and topaz 8's glyph masks for ASCII."""
    page = b"\x1b[H\x1b[2J\x1b[7m \x1b[0m"
    for row, (a, b) in enumerate([(0x21, 0x40), (0x40, 0x60), (0x60, 0x7F)]):
        page += b"\x1b[%d;1H" % (row + 2) + bytes(range(a, b))
    (RIG / "vtc/vt/charset.txt").write_bytes(page)
    shot = scratch / "charset.png"
    show("VTC:vt/charset.txt", shot)
    w, h, px = read_png(shot)
    ox = oy = None
    for y in range(h):
        for x in range(w):
            if px(x, y) == FG and all(px(x + dx, y + dy) == FG for dx in range(CW) for dy in range(CH)):
                ox, oy = x, y
                break
        if ox is not None:
            break
    if ox is None:
        raise SystemExit("[ERROR] no inverse block: is the window up?")
    glyphs = {0x20: tuple([0] * CH)}
    for row, (a, b) in enumerate([(0x21, 0x40), (0x40, 0x60), (0x60, 0x7F)]):
        for k, c in enumerate(range(a, b)):
            glyphs[c] = tuple(sum(1 << (7 - dx) for dx in range(CW)
                                  if px(ox + k * CW + dx, oy + (row + 1) * CH + dy) == FG)
                              for dy in range(CH))
    return ox, oy, glyphs


def replay_bytes(stream):
    """The stream as both sides see it: from a reset, and without ?40 --
    vttest's 132-column screens need a 132-column window, which the rig's
    screen cannot give (DECCOLM itself is proven by the host tests)."""
    return b"\x1bc" + stream.read_bytes().replace(b"\x1b[?40h", b"")


def expected(stream):
    out = subprocess.run([str(ROOT / "build/vtdump"), "80", "24", "xterm", "sizes"],
                         input=replay_bytes(stream), capture_output=True).stdout.decode("utf-8", "replace")
    lines = out.split("\n")
    cells = [[tuple(int(v) for v in c.split(",")) for c in l.split(" ")] for l in lines[24:48]]
    sizes = [int(v) for v in next(l for l in lines if l.startswith("sizes")).split()[1:]]
    cur = tuple(int(v) for v in next(l for l in lines if l.startswith("@"))[1:].split(","))
    return cells, sizes, cur


def compare(shot, cells, sizes, cur, ox, oy, glyphs):
    w, h, px = read_png(shot)
    ok = bad = skipped = 0
    worst = []
    for y in range(24):
        if sizes[y]:
            skipped += 80
            continue
        for x in range(80):
            fg, bg, attr, ch = cells[y][x]
            if (x, y) == cur:
                skipped += 1               # the cursor: drawn inverted on purpose
                continue
            if ch > 0x7E or ch < 0x20:
                skipped += 1
                continue
            f, b = colour(fg, FG), colour(bg, BG)
            if attr & 0x20:                          # inverse
                f, b = b, f
            if attr & 0x40:                          # conceal
                f = b
            want = list(glyphs[ch])
            if attr & 0x01:                          # bold: the soft style smears 1 px right
                want = [v | (v >> 1) for v in want]
            if attr & 0x08:                          # underline: the row under the baseline
                want[CH - 1] = 0xFF
            got = [sum(1 << (7 - dx) for dx in range(CW)
                       if px(ox + x * CW + dx, oy + y * CH + dy) == f) for dy in range(CH)]
            if f == b:
                got, want = [0] * CH, [0] * CH
            if got == want or ((attr & 0x10) and got == [0] * CH):   # blink: either phase
                ok += 1
            else:
                bad += 1
                if len(worst) < 3:
                    worst.append("(%d,%d) %r" % (x, y, chr(ch)))
    return ok, bad, skipped, worst


def main(args):
    out = pathlib.Path(args[args.index("--out") + 1]) if "--out" in args else \
        pathlib.Path("/private/tmp") / "vttest_rig"
    names = [a for a in args if not a.startswith("--") and pathlib.Path(a).name != out.name] or \
        sorted(p.name for p in (ROOT / "tests/streams").glob("vttest-*.bin"))
    out.mkdir(parents=True, exist_ok=True)
    (RIG / "vtc/vt").mkdir(exist_ok=True)
    (RIG / "vtc/vtshow").write_bytes((ROOT / "build/amiga/vtshow").read_bytes())
    run('Run >NIL: NewShell "XCON:0/%d/%d/%d/vttest/XTERM"' % (TOP, W, H))
    time.sleep(4)
    ox, oy, glyphs = origin_and_glyphs(out)
    total_bad = 0
    for name in names:
        stream = ROOT / "tests/streams" / name
        (RIG / "vtc/vt" / name).write_bytes(replay_bytes(stream))
        shot = out / (name[:-4] + ".png")
        show("VTC:vt/" + name, shot)
        cells, sizes, cur = expected(stream)
        ok, bad, skipped, worst = compare(shot, cells, sizes, cur, ox, oy, glyphs)
        total_bad += bad
        print("%-28s %s  %4d match  %3d differ  %4d not compared  %s" %
              (name[:-4], "PASS" if not bad else "FAIL", ok, bad, skipped, " ".join(worst)), flush=True)
    print("%d screens, %d cells differ; screenshots in %s" % (len(names), total_bad, out))
    return 1 if total_bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
