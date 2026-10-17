#!/usr/bin/env python3
"""A fixed 256-colour and truecolor screen for capture (htop is not installed):
the 16 standard colours, the 6x6x6 cube, the grey ramp, truecolor gradients,
and fg/bg/bold/underline/inverse mixes. Deterministic, 80x24, no scrolling."""
import sys

o = sys.stdout


def bg(n):
    return "\x1b[48;5;%dm " % n


def fgc(n, s="x"):
    return "\x1b[38;5;%dm%s" % (n, s)


def rgb_bg(r, g, b):
    return "\x1b[48;2;%d;%d;%dm " % (r, g, b)


rows = []
rows.append("".join(bg(n) for n in range(16)) + "\x1b[0m  standard + bright (bg)")
for r in range(6):                       # cube: 6 rows of 36
    rows.append("".join(bg(16 + r * 36 + i) for i in range(36)) + "\x1b[0m " +
                "".join(fgc(16 + r * 36 + i) for i in range(36)) + "\x1b[0m")
rows.append("".join(bg(232 + i) for i in range(24)) + "\x1b[0m " +
            "".join(fgc(232 + i) for i in range(24)) + "\x1b[0m grey ramp")
for k in range(3):                       # truecolor gradients, 64 columns
    ch = [lambda i: (i * 4, 0, 255 - i * 4), lambda i: (0, i * 4, 128), lambda i: (255, i * 4, i * 2)][k]
    rows.append("".join(rgb_bg(*ch(i)) for i in range(64)) + "\x1b[0m truecolor %d" % k)
rows.append("\x1b[38;2;255;128;0mtrue fg\x1b[0m \x1b[38;2;10;200;90;48;2;40;0;60mtrue fg+bg\x1b[0m "
            "\x1b[1;38;5;196mbold 196\x1b[0m \x1b[4;38;5;46munder 46\x1b[0m \x1b[7;38;5;27minv 27\x1b[0m")
rows.append("\x1b[1;4;38;2;255;255;0;48;5;17mbold+under true fg 256 bg\x1b[0m "
            "\x1b[31;1mred bold\x1b[0m \x1b[91mbright red\x1b[0m \x1b[93;104mbright pair\x1b[0m")
rows.append("\x1b[38:2::200:100:50mcolon truecolor\x1b[0m \x1b[38:5:99mcolon 256\x1b[0m "
            "\x1b[2mdim\x1b[0m \x1b[3mitalic\x1b[0m \x1b[9mstrike\x1b[0m")
o.write("\x1b[2J\x1b[H" + "\r\n".join(rows) + "\x1b[0m")
o.flush()
