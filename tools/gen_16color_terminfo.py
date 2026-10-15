#!/usr/bin/env python3
"""W46 /colors 16: the kit's xterm-16color and vtcon-amiga-16color entries,
made from the compiled vtcon entry so terminfo/vtcon.terminfo stays the one
source of what the window does.

  python3 -I tools/gen_16color_terminfo.py <terminfo dir holding vtcon> <out.terminfo>

xterm-16color is vtcon with 16 colours: colors#16, pairs#256, setaf/setab as
30-37/40-47 and 90-97/100-107, and no direct colour (RGB, Tc, setrgbf,
setrgbb, Setulc). It cannot be written as "RGB@, Tc@, use=vtcon": tic (ncurses
6.4) does not cancel an extended boolean through use=, so programs would still
read RGB and Tc as set. vtcon-amiga-16color is the same for an AMIGA
personality window: no bce, no xenl, as vtcon-amiga.
"""
import os
import subprocess
import sys

DROP = {"RGB", "Tc", "setrgbf", "setrgbb", "Setulc"}
SET = {
    "colors": "colors#16",
    "pairs": "pairs#0x100",
    "setaf": r"setaf=\E[%?%p1%{8}%<%t3%p1%d%e9%p1%{8}%-%d%;m",
    "setab": r"setab=\E[%?%p1%{8}%<%t4%p1%d%e10%p1%{8}%-%d%;m",
}
HEADER = "xterm-16color|xterm with 16 colors (vtcon told 16 colours by /colors 16),"
AMIGA = "vtcon-amiga-16color|vtcon Amiga console told 16 colours,\n\tbce@, xenl@, use=xterm-16color,\n"


def cap_name(item):
    """'colors#256' -> 'colors', 'bce' -> 'bce', 'kbs=^?' -> 'kbs'."""
    for i, ch in enumerate(item):
        if ch in "#=@":
            return item[:i]
    return item


def entry(vtcon_source):
    """The two entries' source from `infocmp -x -1 vtcon` output."""
    out, seen = [], set()
    for line in vtcon_source.splitlines():
        if not line.startswith("\t"):
            if line.startswith("vtcon|"):
                out.append(HEADER)
            continue  # infocmp's comment lines
        item = line.strip().rstrip(",")
        name = cap_name(item)
        if name in DROP:
            continue
        if name in SET:
            item = SET[name]
            seen.add(name)
        out.append("\t" + item + ",")
    missing = set(SET) - seen
    if len(out) < 2 or out[0] != HEADER or missing:
        raise SystemExit("gen_16color_terminfo: vtcon entry lacks %s" % (sorted(missing) or "its header"))
    return "\n".join(out) + "\n" + AMIGA


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src = subprocess.run(["infocmp", "-x", "-1", "vtcon"], check=True, capture_output=True, text=True,
                         env=dict(os.environ, TERMINFO=sys.argv[1])).stdout
    with open(sys.argv[2], "w") as f:
        f.write(entry(src))


if __name__ == "__main__":
    main()
