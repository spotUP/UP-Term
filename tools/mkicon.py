#!/usr/bin/env python3
"""mkicon.py OUT.info [--x X --y Y] [--tool NAME] [--plain] [--tooltype T ...] -- the UP-Term
Workbench project icon (P8): default tool UP-Term:bin/vsh, tooltype WINDOW= the XCON
window vsh opens, a 64 KB stack. Written in the Workbench DiskObject format
(workbench/workbench.h, intuition/intuition.h): DiskObject, the Image and
its planar data, then the default tool and the tooltypes as length-prefixed
strings. The image is a small terminal window in the Workbench's four pens
(0 grey, 1 black, 2 white, 3 blue). --tool with --plain is the same image
for a plain program icon (UP-Term Prefs: its own tool, no window);
--tooltype adds tooltypes to it (a WINDOW= one replaces the default window) (the kit's Install icon: Installer's
APPNAME, MINUSER). --drawer writes a drawer icon (WBDRAWER with a DrawerData, 62 bytes as
workbench.h DRAWERDATAFILESIZE: the NewWindow, origin, dd_Flags, dd_ViewModes), a drawer drawn in the
same four pens, for a drawer Workbench has no icon for (VTC:Apps.info on the rig)."""
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
# 44 x 22 drawer: a black outline with a blue front, a white tab
DRAWER_ART = [
    "............................................",
    "............................................",
    "..ooooooooooooooooooooooooo.................",
    "..o#########################...............",
    "..o#bbbbbbbbbbbbbbbbbbbbbbb#................",
    "..o#bbbbbbbbbbbbbbbbbbbbbbb#................",
    ".o##########################################",
    ".o#oooooooooooooooooooooooooooooooooooooooo#.",
    ".o#o#######################################.",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb#..",
    ".o#o#######################################..",
    ".o#########################################..",
    "............................................",
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


# DrawerData as workbench.h writes it (DRAWERDATAFILESIZE = 62): NewWindow (48 bytes, intuition.h),
# dd_CurrentX/Y, dd_Flags, dd_ViewModes
def drawer_data():
    new_window = struct.pack('>hhhhBBLLLLLLLhhHHH',
                             50, 50, 400, 150,      # LeftEdge, TopEdge, Width, Height
                             255, 255,              # DetailPen, BlockPen: the screen's
                             0,                     # IDCMPFlags
                             0x000F,                # Flags: SIZEGADGET | DRAGBAR | DEPTHGADGET | CLOSEGADGET
                             0, 0, 0, 0, 0,         # FirstGadget, CheckMark, Title, Screen, BitMap
                             90, 40, 0xFFFF, 0xFFFF,  # MinWidth, MinHeight, MaxWidth, MaxHeight
                             1)                     # Type: WBENCHSCREEN
    return new_window + struct.pack('>llLH', 0, 0, 0, 0)   # origin 0/0, DDFLAGS_SHOWDEFAULT, DDVM_BYDEFAULT


def icon(x, y, tool, tooltypes, drawer=False):
    art = DRAWER_ART if drawer else ART
    w, h = len(art[0]), len(art)
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
        2 if drawer else 4, 0,         # do_Type WBDRAWER or WBPROJECT, pad
        0 if drawer else 1, 1 if tooltypes else 0,   # do_DefaultTool, do_ToolTypes: present
        x, y,                          # do_CurrentX/Y
        1 if drawer else 0, 0,         # do_DrawerData, do_ToolWindow
        0 if drawer else 65536)        # do_StackSize
    image = struct.pack('>hhhhhLBBL', 0, 0, w, h, 2, 1, 3, 0, 0) + planes(art)
    if drawer:
        tt = b''
        if tooltypes:
            tt = struct.pack('>L', (len(tooltypes) + 1) * 4) + b''.join(bstr(t) for t in tooltypes)
        return diskobj + drawer_data() + image + tt
    tt = b''
    if tooltypes:
        tt = struct.pack('>L', (len(tooltypes) + 1) * 4) + b''.join(bstr(t) for t in tooltypes)
    return diskobj + image + bstr(tool) + tt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--x', type=int)
    ap.add_argument('--y', type=int)
    ap.add_argument('--tool', default='UP-Term:bin/vsh')
    ap.add_argument('--drawer', action='store_true')
    ap.add_argument('--plain', action='store_true')
    ap.add_argument('--tooltype', action='append', default=[])
    a = ap.parse_args()
    x = a.x if a.x is not None else NO_ICON_POSITION
    y = a.y if a.y is not None else NO_ICON_POSITION
    # a --tooltype WINDOW=... replaces the default window: two WINDOW tooltypes made Workbench use the
    # first, the default 640x400 "UP-Term" window (W-I1: ClaudeCode's icon opened it, not its own)
    own_window = any(t.startswith("WINDOW=") for t in a.tooltype)
    tooltypes = ([] if a.plain or own_window else ["WINDOW=" + WINDOW]) + a.tooltype
    data = icon(x - (1 << 32) if x >= 1 << 31 else x, y - (1 << 32) if y >= 1 << 31 else y,
                a.tool, [] if a.drawer else tooltypes, a.drawer)
    open(a.out, 'wb').write(data)


if __name__ == '__main__':
    main()
