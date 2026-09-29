#!/usr/bin/env python3
"""Diff the xterm personality against two reference emulators.

Every stream in tests/streams/ (<name>.<cols>x<rows>.bin, default 80x24) is
fed to build/vtdump (ours), build/vterm_dump (libvterm, neovim's terminal)
and pyte. Text, cursor, colours and bold/inverse/underline are compared.

The engine must match libvterm. pyte is the second opinion: where the two
references disagree the case is listed in DISAGREE with what xterm's
ctlseqs says and which one we follow -- never silently skipped.
"""
import pathlib, re, subprocess, sys
import pyte

ROOT = pathlib.Path(__file__).resolve().parent.parent
# stream name -> reason; the engine follows libvterm on these, pyte differs.
# Each reason names the pyte gap, so a new entry must be a gap too.
PYTE_GAP_ALT = "pyte has no alternate screen (?47/?1047/?1049)"
PYTE_GAP_BCE = "pyte erases with the default background, not the current one (no BCE)"
DISAGREE = {
    "quirk-alt-1047-clears-on-exit.10x4.bin": PYTE_GAP_ALT,
    "quirk-alt-1049-roundtrip.10x4.bin": PYTE_GAP_ALT,
    "quirk-bce-erase-colour.10x3.bin": PYTE_GAP_BCE,
    "quirk-bce-il-colour.10x4.bin": PYTE_GAP_BCE,
    "quirk-bce-scroll-colour.10x3.bin": PYTE_GAP_BCE,
    "quirk-can-in-osc.10x3.bin": "pyte does not abort an OSC string on CAN",
    "quirk-cha-hpa-vpa.10x4.bin": "pyte has no HPA (CSI `)",
    "quirk-cht-cbt.30x3.bin": "pyte has no CHT (CSI I)",
    "quirk-dec-graphics-box.12x4.bin": "pyte has no DEC special graphics",
    "quirk-so-si-g1.12x3.bin": "pyte has no G1 / SO / SI",
    "quirk-decstr-soft-reset.10x4.bin": "pyte has no DECSTR (CSI ! p)",
    "quirk-esc-restarts-csi.10x3.bin": "pyte does not restart on ESC inside a CSI",
    "quirk-nel.10x3.bin": "pyte's NEL keeps the column",
    "quirk-origin-mode-cup.10x6.bin": "pyte does not clamp CUP to the region under DECOM",
    "quirk-region-cup-outside.10x5.bin": "pyte scrolls the region on LF below it",
    "quirk-wrap-below-region.10x5.bin": "pyte scrolls the region on a wrap below it",
    "quirk-region-ri-above.10x5.bin": "pyte scrolls the region on RI above it",
    "quirk-region-su-sd.10x6.bin": "pyte's SU/SD ignore the region",
    "quirk-sgr-256-and-rgb.12x3.bin": "pyte has no colon SGR sub-parameters",
    "quirk-st-forms.10x3.bin": "pyte prints DCS payloads",
    "quirk-wide-at-last-col.10x3.bin": "pyte puts a wide glyph in the last column",
    "quirk-wrap-then-lf.10x4.bin": "pyte's LF cancels the column of a pending wrap",
    "vttest-m2-s04.80x24.bin": "pyte obeys DECCOLM and goes 132 wide; xterm without allowColumns, libvterm and we keep the width",
    "vttest-m8-s07.80x24.bin": "pyte obeys DECCOLM and goes 132 wide; xterm without allowColumns, libvterm and we keep the width",
    "vttest-m1-s00.80x24.bin": "pyte draws vttest's cursor-movement frame wrong (IND/RI/NEL frame)",
    "vttest-m1-s01.80x24.bin": "pyte obeys DECCOLM (132 columns)",
    "vttest-m1-s02.80x24.bin": "pyte obeys DECCOLM (132 columns) and loses the autowrap frame",
    "vttest-m1-s03.80x24.bin": "pyte obeys DECCOLM (132 columns) and loses the autowrap frame",
    "vttest-m2-s02.80x24.bin": "pyte obeys DECCOLM (132 columns)",
    "vttest-m2-s12.80x24.bin": "pyte has no DECSCNM (?5 reverse-video screen)",
    "vttest-m2-s13.80x24.bin": "pyte has no DECSCNM (?5 reverse-video screen)",
    "vttest-m8-s03.80x24.bin": "pyte's insert mode leaves a stale glyph",
    "vttest-m8-s08.80x24.bin": "pyte obeys DECCOLM (132 columns)",
    "vttest-m8-s09.80x24.bin": "pyte obeys DECCOLM (132 columns)",
    "vttest-m8-s10.80x24.bin": "pyte obeys DECCOLM (132 columns)",
    "vttest-m8-s11.80x24.bin": "pyte obeys DECCOLM (132 columns)",
    "chatsim.80x24.bin": "pyte's SU/SD ignore the region (ncurses scrolls the log with them)",
    "vim-small.40x12.bin": PYTE_GAP_ALT + " (vim has quit back to the primary screen)",
}

# Sequences programs send whatever the terminfo says (capability probes and
# optional features); a terminal that lacks them ignores them, and the
# programs fall back. Only these may stay unhandled in a TERM=vtcon capture.
PROBES = {
    "C 14t": "XTWINOPS pixel-size query (tmux); no pixel size is reported",
    "O 10": "OSC 10 foreground-colour query (tmux)",
    "O 11": "OSC 11 background-colour query (tmux)",
    "C >q": "XTVERSION query (tmux)",
    "C ?996n": "colour-scheme DSR query (tmux)",
    "M 2031": "colour-scheme change notifications (tmux)",
    "M 1005": "UTF-8 mouse encoding (tmux asks for it; SGR ?1006 is what we do)",
    "M 7727": "tmux application-escape mode, tmux-specific",
    "C 0%m": "vim: an XTQMODKEYS-style query",
    "D 0": "a DCS request (vim: XTGETTCAP/DECRQSS); no reply",
}

# stream name -> reason: the engine follows xterm here and libvterm differs.
# These compare against a reviewed golden grid (tests/golden/<stream>.txt,
# written by `make golden`), so a change to them is a visible diff.
XTERM_NOT_LIBVTERM = {
    "quirk-wrap-then-el.10x4.bin": "EL resets the pending wrap in xterm; libvterm keeps it",
    "quirk-wrap-then-ech.10x4.bin": "ECH resets the pending wrap in xterm; libvterm keeps it",
    "quirk-wrap-then-ich.10x4.bin": "ICH resets the pending wrap in xterm; libvterm keeps it",
    "quirk-wrap-then-dch.10x4.bin": "DCH resets the pending wrap in xterm; libvterm keeps it",
    "quirk-wrap-then-tab.10x4.bin": "HT at the last column stays there in xterm; libvterm wraps first",
    "quirk-rep-after-wrap.10x3.bin": "REP prints like the glyph itself (wraps); libvterm drops it",
    "quirk-region-cuu-stops.10x6.bin": "CUU stops at the top margin inside the region (VT100)",
    "quirk-region-cud-stops.10x6.bin": "CUD stops at the bottom margin inside the region (VT100)",
    "quirk-region-il-inside.10x6.bin": "IL moves the cursor to the left margin (VT102, xterm)",
    "quirk-region-dl-inside.10x6.bin": "DL moves the cursor to the left margin (VT102, xterm)",
    "quirk-region-one-line.10x5.bin": "DECSTBM needs top < bottom; a one-line region is ignored",
    "quirk-wide-overwrite-left.10x3.bin": "writing over half of a wide glyph erases all of it",
    "quirk-wide-overwrite-right.10x3.bin": "writing over half of a wide glyph erases all of it",
    "quirk-wide-ich-split.10x3.bin": "ICH inside a wide glyph erases the glyph",
    "quirk-wide-dch-split.10x3.bin": "DCH inside a wide glyph erases the glyph",
    "vttest-m2-s14.80x24.bin": "DECRC restores the character sets DECSC saved (VT100); libvterm does not",
    "quirk-decrc-without-save.10x4.bin": "libvterm resets the pen to RGB black, not default colours",
}
# stream name -> reason: a reference crashes or hangs on it; the other one decides.
REF_BROKEN = {
    "quirk-rep-with-nothing.10x3.bin": "libvterm loops forever on REP before any glyph",
    "quirk-wide-dch-split.10x3.bin": "pyte IndexError on a wide glyph edit",
    "quirk-wide-ich-split.10x3.bin": "pyte IndexError on a wide glyph edit",
    "quirk-wide-overwrite-left.10x3.bin": "pyte IndexError on a wide glyph edit",
    "tmux-chatsim.80x24.bin": "pyte crashes on tmux's output",
    "tmux-split.80x24.bin": "pyte crashes on tmux's output",
    "tmux-vim.80x24.bin": "pyte crashes on tmux's output",
}

class RefScreen(pyte.Screen):
    """pyte with the one crash it has on real traffic removed: private SGR
    (CSI > 4 ; 2 m, xterm modifyOtherKeys, which vim sends) is not SGR."""
    def select_graphic_rendition(self, *attrs, private=False):
        if private:
            return
        super().select_graphic_rendition(*attrs)

def dump(tool, data, cols, rows):
    """A tool's grid, or None when the reference hangs (libvterm has one)."""
    try:
        out = subprocess.run([str(ROOT / "build" / tool), str(cols), str(rows)], input=data,
                             capture_output=True, check=True, timeout=5).stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        if tool == "vtdump":
            raise
        return None
    lines = out.split("\n")
    text = lines[:rows]
    attr = []
    for y in range(rows):
        row = []
        for cell in lines[rows + y].split(" "):
            fg, bg, a, ch = (int(v) for v in cell.split(","))
            row.append(visible(fg, bg, bool(a & 1), bool(a & 0x20), bool(a & 8), ch))
        attr.append(row)
    unhandled = lines[2 * rows + 1] if len(lines) > 2 * rows + 1 else ""
    return text, lines[2 * rows], attr, unhandled

def visible(fg, bg, bold, inverse, underline, ch):
    """What a cell shows. A blank with no underline paints only its
    background (swapped under inverse): its foreground and bold are
    invisible, and the references disagree about what an erase leaves
    there, so they are not compared."""
    if ch == 32 and not underline:
        return (None, fg if inverse else bg, False, False, False)
    return (fg, bg, bold, inverse, underline)

def compare(a, b, cols, rows):
    """Differences between two dumps: (rows with other text, cursor pair, cells).
    Dumps are (text, cursor, attr[, unhandled]); only the first three count."""
    bad = [y for y in range(rows) if a[0][y] != b[0][y]]
    cells = [(x, y, a[2][y][x], b[2][y][x]) for y in range(rows) for x in range(cols)
             if a[2][y][x] != b[2][y][x]]
    return bad, (a[1], b[1]) if a[1] != b[1] else None, cells

def pyte_dump(data, cols, rows):
    try:
        return pyte_dump_(data, cols, rows)
    except Exception:  # pyte crashes on some wide-glyph edits
        return None

def pyte_dump_(data, cols, rows):
    scr = RefScreen(cols, rows)
    st = pyte.ByteStream(scr)
    st.feed(data)
    text = [scr.display[y] for y in range(rows)]
    cur = "@%d,%d" % (min(scr.cursor.x, cols - 1), scr.cursor.y)
    attr = [[visible(colour(c.fg), colour(c.bg), bool(c.bold), bool(c.reverse), bool(c.underscore),
                     ord(c.data[0]) if c.data else 32)
             for c in (scr.buffer[y][x] for x in range(cols))] for y in range(rows)]
    return text, cur, attr

NAMES = ["black", "red", "green", "brown", "blue", "magenta", "cyan", "white"]
DEFAULT, RGB = 0x100, 0x1000000

def colour(v):
    """pyte's colour value in the engine's encoding."""
    if v == "default":
        return DEFAULT
    if v in NAMES:
        return NAMES.index(v)
    if v.startswith("bright") and v[6:] in NAMES:
        return 8 + NAMES.index(v[6:])
    if v.startswith("bright") and v[6:] == "yellow":
        return 11
    r, g, b = int(v[0:2], 16), int(v[2:4], 16), int(v[4:6], 16)
    hexv = v.lower()
    if hexv in PALETTE:
        return PALETTE.index(hexv)
    return RGB | (r << 16) | (g << 8) | b

import pyte.graphics
PALETTE = [c.lower() for c in pyte.graphics.FG_BG_256]

def report(label, diff, ours, ref):
    bad, cur, cells = diff
    for y in bad[:6]:
        print("  row %2d ours    [%s]\n         %-7s [%s]" % (y, ours[0][y].rstrip(), label, ref[0][y].rstrip()))
    if cur:
        print("  cursor ours %s %s %s" % (cur[0], label, cur[1]))
    for x, y, got, want in cells[:4]:
        print("  cell %d,%d ours %s %s %s  (fg, bg, bold, inverse, underline)" % (x, y, got, label, want))
    if len(cells) > 4:
        print("  ... %d cells differ in colour or attributes" % len(cells))

WRITE_GOLDEN = False

def entry(table, name):
    """A table entry for a stream, or for its xterm-256color twin: a ti-
    capture is the same program under TERM=vtcon, so the same reference gap
    applies to it."""
    if name in table:
        return table[name]
    if name.startswith("ti-") and name[3:] in table:
        return table[name[3:]]
    return None

def main():
    global WRITE_GOLDEN
    args = sys.argv[1:]
    if args and args[0] == "--write-golden":
        WRITE_GOLDEN = True
        args = args[1:]
    only = args[0] if args else None
    fails = ran = 0
    for p in sorted((ROOT / "tests/streams").glob("*.bin")):
        if only and only not in p.name:
            continue
        ran += 1
        m = re.search(r"\.(\d+)x(\d+)\.bin$", p.name)
        cols, rows = (int(m.group(1)), int(m.group(2))) if m else (80, 24)
        data = p.read_bytes()
        ours = dump("vtdump", data, cols, rows)
        if p.name.startswith("ti-"):
            # A TERM=vtcon capture: terminfo promised only what the engine does,
            # so anything unhandled must be a probe programs send regardless.
            kinds = re.findall(r"\[([^\]]+)\]x(\d+)", ours[3])
            bad = [k for k, _ in kinds if k not in PROBES]
            if bad or ("unhandled 0" != ours[3].split(" [")[0] and not kinds):
                fails += 1
                print("[FAIL] %s: TERM=vtcon left unhandled sequences that are not known probes: %s"
                      % (p.name, ", ".join(bad) or ours[3]))
                continue
        if p.name in XTERM_NOT_LIBVTERM:
            g = ROOT / "tests/golden" / (p.name[:-4] + ".txt")
            raw = subprocess.run([str(ROOT / "build/vtdump"), str(cols), str(rows)], input=data,
                                 capture_output=True, check=True).stdout.decode("utf-8")
            # the grid only: the unhandled-sequence line is a diagnostic
            raw = "\n".join(l for l in raw.split("\n") if not l.startswith("unhandled "))
            if WRITE_GOLDEN:
                g.write_text(raw)
            if not g.exists():
                fails += 1
                print("[FAIL] %s: no golden grid (make golden, then review it)" % p.name)
            elif g.read_text() != raw:
                fails += 1
                print("[FAIL] %s: differs from its golden grid (%s)" % (p.name, XTERM_NOT_LIBVTERM[p.name]))
            else:
                print("[OK] %s (golden: %s)" % (p.name, XTERM_NOT_LIBVTERM[p.name]))
            continue
        vt = dump("vterm_dump", data, cols, rows)
        py = pyte_dump(data, cols, rows)
        if vt is None or py is None:
            if entry(REF_BROKEN, p.name) is None:
                fails += 1
                print("[FAIL] %s: %s failed on it and REF_BROKEN has no entry"
                      % (p.name, "libvterm" if vt is None else "pyte"))
                continue
            ref, label = (py, "pyte") if vt is None else (vt, "libvterm")
            if ref is None:
                fails += 1
                print("[FAIL] %s: both references failed" % p.name)
                continue
            d = compare(ours, ref, cols, rows)
            if d[0] or d[1] or d[2]:
                fails += 1
                print("[FAIL] %s: differs from %s (the other reference: %s)" % (p.name, label, entry(REF_BROKEN, p.name)))
                report(label, d, ours, ref)
            else:
                print("[OK] %s (%s only: %s)" % (p.name, label, entry(REF_BROKEN, p.name)))
            continue
        dv = compare(ours, vt, cols, rows)
        dp = compare(ours, py, cols, rows)
        vt_ok = not (dv[0] or dv[1] or dv[2])
        py_ok = not (dp[0] or dp[1] or dp[2])
        if vt_ok and py_ok:
            print("[OK] %s" % p.name)
        elif vt_ok:
            if entry(DISAGREE, p.name):
                print("[OK] %s (pyte differs: %s)" % (p.name, entry(DISAGREE, p.name)))
            else:
                fails += 1
                print("[FAIL] %s: matches libvterm, pyte differs and DISAGREE has no entry" % p.name)
                report("pyte", dp, ours, py)
        else:
            fails += 1
            print("[FAIL] %s: differs from libvterm%s" % (p.name, "" if not py_ok else " (pyte agrees with us)"))
            report("libvterm", dv, ours, vt)
    print("%d streams, %d failed" % (ran, fails))
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
