#!/usr/bin/env python3
"""aspect_rig.py -- fonts by pixel shape (plan 2026-10-03-screens-and-dctelnet.md
P2, render/fontpair): topaz is drawn as TopazPro 16 on square pixels, as
topaz 8 on the Amiga's tall ones.

The cell height is read from the window: /size 40x12 makes the text area
12 cells tall, so the window's height says 8 or 16 pixels a cell.
  1. the rig's Workbench (a graphics card, square pixels): 16
  2. a screen of its own in PAL hires (SCREENMODE 0x29000, tall pixels): 8
  3. font-aspect = off in the profile, on the Workbench: 8
  4. a window on the Workbench (16) moved live (/screen own, the profile's
     screen-mode PAL hires): 8
The fonts come from the kit's dist/fonts (VTC:fonts, added to FONTS:).
Run with the rig up and the handler installed: python3 tools/rig/aspect_rig.py
"""
import pathlib, shutil, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / 'build/rig/vtc'
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def t(s, wt=2):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(wt)


def cell_height(geom, title, opts):
    """open a window, /size 40x12 and 40x20, the cell height from the difference"""
    c.run('Run >NIL: NewShell "XCON:%s/%s/%s"' % (geom, title, opts))
    time.sleep(6)
    x, y, w, h = ami.window(title)['box']
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    t('/size 40x12', 3)
    x, y, w, h1 = ami.window(title)['box']
    t('/size 40x20', 3)
    x, y, w, h2 = ami.window(title)['box']
    t('EndCLI', 4)
    return (h2 - h1) // 8   # 8 more rows


def main():
    shutil.rmtree(VTC / 'fonts', ignore_errors=True)
    shutil.copytree(ROOT / 'dist/fonts', VTC / 'fonts')
    had = c.run('Copy ENV:up-term/up-term RAM:aspect.bak QUIET')[0] == 0
    c.run('Assign FONTS: VTC:fonts ADD')
    try:
        c.run('Delete ENV:up-term/up-term QUIET')
        h = cell_height('0/20/640/300', 'sq', 'CLOSE')
        check(h == 16, 'the Workbench on a graphics card (square pixels): topaz drawn as TopazPro 16', h)
        h = cell_height('0/20/640/200', 'pal', 'OWNSCREEN/SCREENMODE 0x29000')
        check(h == 8, 'a PAL hires screen of its own (tall pixels): topaz 8', h)
        c.run('MakeDir ENV:up-term QUIET')
        (VTC / 'aspect.conf').write_text('[profile default]\nfont-aspect = off\n')
        c.run('Copy VTC:aspect.conf ENV:up-term/up-term QUIET')
        h = cell_height('0/20/640/300', 'off', 'CLOSE')
        check(h == 8, 'font-aspect = off: topaz 8 on the square pixels too', h)
        # 4: a live move to tall pixels changes the font (vtwin_fit_aspect)
        (VTC / 'aspect.conf').write_text('[profile default]\nscreen-mode = 0x29000\n')
        c.run('Copy VTC:aspect.conf ENV:up-term/up-term QUIET')
        c.run('Run >NIL: NewShell "XCON:0/20/640/300/mv/CLOSE"')
        time.sleep(6)
        x, y, w, h = ami.window('mv')['box']
        kx, ky = ami.pointer_scale()
        ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
                   ('wait', 2), ('button', 0, 0), ('wait', 5))
        time.sleep(1)
        t('/screen own', 6)
        t('/size 40x12', 3)
        h1 = ami.window('mv')['box'][3]
        t('/size 40x20', 3)
        h2 = ami.window('mv')['box'][3]
        check((h2 - h1) // 8 == 8, 'a live move to a PAL screen of its own: TopazPro 16 becomes topaz 8',
              (h2 - h1) // 8)
        t('EndCLI', 4)
    finally:
        c.run('Assign FONTS: VTC:fonts REMOVE')
        if had:
            c.run('Copy RAM:aspect.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:aspect.bak QUIET')
        (VTC / 'aspect.conf').unlink(missing_ok=True)
    print('aspect_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
