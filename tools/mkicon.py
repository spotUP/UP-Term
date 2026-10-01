#!/usr/bin/env python3
"""mkicon.py OUT.info [--x X --y Y] [--tool NAME] [--plain] -- the UP-Term
Workbench project icon (P8): default tool C:vsh, tooltype WINDOW= the XCON
window vsh opens, a 64 KB stack. Written in the Workbench DiskObject format
(workbench/workbench.h, intuition/intuition.h): DiskObject, the Image and
its planar data, then the default tool and the tooltypes as length-prefixed
strings. The image is a small terminal window in the Workbench's four pens
(0 grey, 1 black, 2 white, 3 blue). --tool with --plain is the same image
for a plain program icon (UP-Term Prefs: its own tool, no window)."""
import argparse, struct

WINDOW = "XCON:0/20/640/400/UP-Term/CLOSE"
NO_ICON_POSITION = 0x80000000

# 44 x 22, one character per pixel: . grey  # black  o white  b blue
ART = [
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "b#################o#o#o################o#o#b",
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "b##########################################b",
    "b##ooo#####################################b",
    "b###ooo####################################b",
    "b####ooo###################################b",
    "b###ooo####################################b",
    "b##ooo#####oooooooo########################b",
    "b##########################################b",
    "b##########################################b",
    "b##o#o#ooo#####ooo#ooo#oo##o#o#############b",
    "b##o#o#o#o######o##o###o#o#ooo#############b",
    "b##o#o#ooo#ooo##o##oo##oo##o#o#############b",
    "b##o#o#o########o##o###o#o#o#o#############b",
    "b###o##o########o##ooo#o#o#o#o#############b",
    "b##########################################b",
    "b##########################################b",
    "b##########################################b",
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "............................................",
    "............................................",
]
PEN = {'.': 0, '#': 1, 'o': 2, 'b': 3}


def planes(art, depth=2):
    h, w = len(art), len(art[0])
    words = (w + 15) // 16
    out = b''
    for p in range(depth):
        for row in art:
            bits = [(PEN[c] >> p) & 1 for c in row] + [0] * (words * 16 - w)
            for i in range(words):
                v = 0
                for b in bits[i * 16:(i + 1) * 16]:
                    v = (v << 1) | b
                out += struct.pack('>H', v)
    return out


def bstr(s):
    b = s.encode('latin-1') + b'\0'
    return struct.pack('>L', len(b)) + b


def icon(x, y, tool, tooltypes):
    w, h = len(ART[0]), len(ART)
    gadget = struct.pack('>LhhhhHHHLLLLLHL',
                         0,             # NextGadget
                         0, 0, w, h,    # Left, Top, Width, Height
                         0x0004,        # Flags: GFLG_GADGIMAGE, complement highlight
                         0x0003,        # Activation: RELVERIFY | GADGIMMEDIATE
                         0x0001,        # GadgetType: BOOLGADGET
                         1,             # GadgetRender: an image follows
                         0, 0, 0, 0,    # SelectRender, GadgetText, MutualExclude, SpecialInfo
                         0,             # GadgetID
                         1)             # UserData: WB_DISKREVISION
    diskobj = struct.pack('>HH', 0xE310, 1) + gadget + struct.pack(
        '>BBLLllLLl',
        4, 0,                          # do_Type WBPROJECT, pad
        1, 1 if tooltypes else 0,      # do_DefaultTool, do_ToolTypes: present
        x, y,                          # do_CurrentX/Y
        0, 0,                          # do_DrawerData, do_ToolWindow
        65536)                         # do_StackSize
    image = struct.pack('>hhhhhLBBL', 0, 0, w, h, 2, 1, 3, 0, 0) + planes(ART)
    tt = b''
    if tooltypes:
        tt = struct.pack('>L', (len(tooltypes) + 1) * 4) + b''.join(bstr(t) for t in tooltypes)
    return diskobj + image + bstr(tool) + tt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--x', type=int)
    ap.add_argument('--y', type=int)
    ap.add_argument('--tool', default='C:vsh')
    ap.add_argument('--plain', action='store_true')
    a = ap.parse_args()
    x = a.x if a.x is not None else NO_ICON_POSITION
    y = a.y if a.y is not None else NO_ICON_POSITION
    tooltypes = [] if a.plain else ["WINDOW=" + WINDOW]
    data = icon(x - (1 << 32) if x >= 1 << 31 else x, y - (1 << 32) if y >= 1 << 31 else y,
                a.tool, tooltypes)
    open(a.out, 'wb').write(data)


if __name__ == '__main__':
    main()
