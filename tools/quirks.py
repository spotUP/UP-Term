#!/usr/bin/env python3
"""Write the quirk streams: one tests/streams/quirk-<name>.<cols>x<rows>.bin per
edge case a Unix terminal meets (esctest / vttest territory). Each is diffed
against libvterm and pyte by refdiff.py. Every case ends by printing a marker
at the cursor, so the cursor position is visible in the grid as well.

Add a case: append to CASES, run `make quirks test-ref`.
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "tests/streams"

E = "\x1b"
CSI = E + "["

def fill(cols, rows):
    """Distinct text in every cell, so moved or erased cells show."""
    s = ""
    for y in range(rows):
        s += CSI + "%d;1H" % (y + 1) + "".join(chr(ord("a") + (x + y) % 26) for x in range(cols))
    return s

# name, cols, rows, stream
CASES = [
    # --- pending wrap (the last-column flag) --------------------------------
    ("wrap-then-cr", 10, 4, "0123456789\r*"),
    ("wrap-then-bs", 10, 4, "0123456789\b*"),
    ("wrap-then-lf", 10, 4, "0123456789\n*"),
    ("wrap-then-cup-same", 10, 4, "0123456789" + CSI + "1;10H*"),
    ("wrap-then-cuf", 10, 4, "0123456789" + CSI + "C*"),
    ("wrap-then-cub", 10, 4, "0123456789" + CSI + "D*"),
    ("wrap-then-el", 10, 4, "0123456789" + CSI + "K*"),
    ("wrap-then-ech", 10, 4, "0123456789" + CSI + "X*"),
    ("wrap-then-ich", 10, 4, "0123456789" + CSI + "@*"),
    ("wrap-then-dch", 10, 4, "0123456789" + CSI + "P*"),
    ("wrap-then-tab", 10, 4, "0123456789\t*"),
    ("wrap-then-sgr", 10, 4, "0123456789" + CSI + "31m*"),
    ("wrap-then-decsc-decrc", 10, 4, "0123456789" + E + "7" + CSI + "H" + E + "8*"),
    ("wrap-at-bottom-scrolls", 10, 3, fill(10, 3) + CSI + "3;10Hx*"),
    ("wrap-off-then-on", 10, 3, CSI + "?7l0123456789ab" + CSI + "?7h" + CSI + "2;1H0123456789cd"),
    ("wrap-in-region-bottom", 10, 5, fill(10, 5) + CSI + "2;4r" + CSI + "4;1H0123456789XY"),
    ("wrap-below-region", 10, 5, fill(10, 5) + CSI + "1;3r" + CSI + "5;1H0123456789XY"),
    # --- scroll regions ------------------------------------------------------
    ("region-one-line", 10, 5, fill(10, 5) + CSI + "3;3r" + CSI + "3;1H\n\nZ"),
    ("region-invalid-ignored", 10, 5, fill(10, 5) + CSI + "4;2r" + CSI + "5;1H\n*"),
    ("region-homes-cursor", 10, 5, CSI + "3;4H" + CSI + "2;4r*"),
    ("region-cup-outside", 10, 5, fill(10, 5) + CSI + "2;3r" + CSI + "5;1H\n*"),
    ("region-ri-at-top", 10, 5, fill(10, 5) + CSI + "2;4r" + CSI + "2;1H" + E + "M*"),
    ("region-ri-above", 10, 5, fill(10, 5) + CSI + "3;4r" + CSI + "1;1H" + E + "M*"),
    ("region-il-inside", 10, 6, fill(10, 6) + CSI + "2;5r" + CSI + "3;4H" + CSI + "2L*"),
    ("region-il-outside", 10, 6, fill(10, 6) + CSI + "2;4r" + CSI + "6;1H" + CSI + "L*"),
    ("region-dl-inside", 10, 6, fill(10, 6) + CSI + "2;5r" + CSI + "3;4H" + CSI + "9M*"),
    ("region-su-sd", 10, 6, fill(10, 6) + CSI + "2;5r" + CSI + "2S" + CSI + "T*"),
    ("region-cuu-stops", 10, 6, CSI + "3;5r" + CSI + "4;1H" + CSI + "9A*"),
    ("region-cud-stops", 10, 6, CSI + "2;4r" + CSI + "3;1H" + CSI + "9B*"),
    ("region-cuu-from-below", 10, 6, CSI + "2;3r" + CSI + "6;1H" + CSI + "2A*"),
    ("origin-mode-cup", 10, 6, CSI + "2;4r" + CSI + "?6h" + CSI + "9;9H*" + CSI + "H+"),
    ("origin-mode-off-homes", 10, 6, CSI + "2;4r" + CSI + "?6h" + CSI + "3;3H" + CSI + "?6l*"),
    ("origin-vpa", 10, 6, CSI + "3;5r" + CSI + "?6h" + CSI + "2d*"),
    ("decstbm-reset", 10, 5, fill(10, 5) + CSI + "2;3r" + CSI + "r" + CSI + "5;1H\n*"),
    # --- cursor motion -------------------------------------------------------
    ("cup-past-bounds", 10, 4, CSI + "99;99H*"),
    ("cup-zero-params", 10, 4, CSI + "3;3H" + CSI + "0;0H*"),
    ("cup-omitted-row", 10, 4, CSI + ";5H*"),
    ("cha-hpa-vpa", 10, 4, CSI + "5G*" + CSI + "8`+" + CSI + "3d-"),
    ("cnl-cpl", 10, 4, CSI + "2;5H" + CSI + "E*" + CSI + "2F+"),
    ("cub-past-left", 10, 4, CSI + "1;3H" + CSI + "9D*"),
    ("bs-at-col0", 10, 4, CSI + "2;1H\b\b*"),
    ("hpr-vpr", 10, 4, CSI + "a*" + CSI + "2e+"),
    # --- tabs ----------------------------------------------------------------
    ("tab-at-margin", 10, 3, "\t\t\t*"),
    ("tab-clear-all", 20, 3, CSI + "3g\t*"),
    ("tab-set-custom", 20, 3, CSI + "1;3H" + E + "H" + CSI + "1;7H" + E + "H\r\t*\t+"),
    ("cht-cbt", 30, 3, CSI + "3I*" + CSI + "2Z+"),
    ("cbt-at-col0", 30, 3, CSI + "Z*"),
    # --- erase and edit ------------------------------------------------------
    ("ed-below", 10, 4, fill(10, 4) + CSI + "2;5H" + CSI + "J*"),
    ("ed-above", 10, 4, fill(10, 4) + CSI + "3;5H" + CSI + "1J*"),
    ("ed-all-keeps-cursor", 10, 4, fill(10, 4) + CSI + "3;5H" + CSI + "2J*"),
    ("el-modes", 10, 4, fill(10, 4) + CSI + "1;5H" + CSI + "K" + CSI + "2;5H" + CSI + "1K"
     + CSI + "3;5H" + CSI + "2K*"),
    ("ech-past-end", 10, 3, fill(10, 3) + CSI + "2;7H" + CSI + "99X*"),
    ("ich-past-end", 10, 3, fill(10, 3) + CSI + "2;7H" + CSI + "99@*"),
    ("dch-past-end", 10, 3, fill(10, 3) + CSI + "2;7H" + CSI + "99P*"),
    ("irm-at-last-col", 10, 3, fill(10, 3) + CSI + "4h" + CSI + "2;10HXY" + CSI + "4l"),
    ("irm-shifts-off-edge", 10, 3, fill(10, 3) + CSI + "4h" + CSI + "2;3HXYZ" + CSI + "4l"),
    ("bce-erase-colour", 10, 3, CSI + "44m" + CSI + "2J" + CSI + "41m" + CSI + "2;3H" + CSI + "K"
     + CSI + "0m*"),
    ("bce-scroll-colour", 10, 3, CSI + "43m\n\n\n\n" + CSI + "0m*"),
    ("bce-il-colour", 10, 4, fill(10, 4) + CSI + "45m" + CSI + "2;1H" + CSI + "L" + CSI + "0m*"),
    ("rep-after-wrap", 10, 3, "012345678X" + CSI + "5b"),
    ("rep-with-nothing", 10, 3, CSI + "5b*"),
    # --- wide glyphs -----------------------------------------------------------
    ("wide-at-last-col", 10, 3, "012345678中*"),
    ("wide-overwrite-left", 10, 3, "中文" + CSI + "1;1Hx"),
    ("wide-overwrite-right", 10, 3, "中文" + CSI + "1;2Hx"),
    ("wide-ich-split", 10, 3, "中文" + CSI + "1;2H" + CSI + "@*"),
    ("wide-dch-split", 10, 3, "中文" + CSI + "1;2H" + CSI + "P*"),
    ("wide-el-split", 10, 3, "中文" + CSI + "1;2H" + CSI + "1K*"),
    # --- character sets and SGR -----------------------------------------------
    ("dec-graphics-box", 12, 4, E + "(0lqqqqk\r\nx    x\r\nmqqqqj" + E + "(B done"),
    ("so-si-g1", 12, 3, E + ")0\x0elqk\x0f lqk"),
    ("sgr-reset-forms", 12, 3, CSI + "1;4;7;31;42ma" + CSI + "mb" + CSI + "1;31mc" + CSI + ";mD"),
    ("sgr-256-and-rgb", 12, 3, CSI + "38;5;196ma" + CSI + "48;5;21mb" + CSI + "38;2;10;200;30mc"
     + CSI + "38:5:46md" + CSI + "0m"),
    ("sgr-bright", 12, 3, CSI + "90ma" + CSI + "97;100mb" + CSI + "39;49mc"),
    # --- alternate screen and save/restore -----------------------------------
    ("alt-1049-roundtrip", 10, 4, fill(10, 4) + CSI + "2;3H" + CSI + "?1049h" + "ALT"
     + CSI + "?1049l*"),
    ("alt-47-no-clear", 10, 4, CSI + "?47hone" + CSI + "?47l" + CSI + "?47h*"),
    ("alt-1047-clears-on-exit", 10, 4, CSI + "?1047hone" + CSI + "?1047l" + CSI + "?1047h*"),
    ("decsc-attrs-and-origin", 10, 5, CSI + "2;4r" + CSI + "?6h" + CSI + "31m" + E + "7"
     + CSI + "0m" + CSI + "?6l" + CSI + "5;5H" + E + "8*"),
    ("decrc-without-save", 10, 4, CSI + "3;3H" + E + "8*"),
    ("decaln-then-region", 10, 4, E + "#8" + CSI + "2;3r" + CSI + "3;1H\n*"),
    ("decstr-soft-reset", 10, 4, CSI + "?7l" + CSI + "4h" + CSI + "2;3r" + CSI + "!p"
     + CSI + "4;1H0123456789ab"),
    # --- controls inside sequences ---------------------------------------------
    ("c0-inside-csi", 10, 3, "abcd" + CSI + "\b2D*"),
    ("esc-restarts-csi", 10, 3, "abcd" + CSI + "3" + E + "[1D*"),
    ("can-in-osc", 10, 3, E + "]0;title\x18abc"),
    ("st-forms", 10, 3, E + "]2;x" + E + "\\a" + E + "P0;1|17/ab" + E + "\\b"),
    # --- scrolling the whole screen -----------------------------------------
    ("ind-at-bottom", 10, 3, fill(10, 3) + CSI + "3;1H" + E + "D*"),
    ("nel", 10, 3, "ab" + E + "E*"),
    ("lf-many", 10, 3, fill(10, 3) + CSI + "3;4H" + "\n" * 5 + "*"),
]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("quirk-*.bin"):
        old.unlink()
    for name, cols, rows, data in CASES:
        (OUT / ("quirk-%s.%dx%d.bin" % (name, cols, rows))).write_bytes(data.encode("utf-8"))
    print("%d quirk streams" % len(CASES))

if __name__ == "__main__":
    main()
