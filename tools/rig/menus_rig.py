#!/usr/bin/env python3
"""menus_rig.py -- ledger H9 (Edit and View menus) on the rig. The items run
through their slash commands (the same menu_run() a pick runs); the strip
itself is read from the window's menus.

  1. The strip: UP-Term, Edit, View, Settings; Edit holds Copy, Paste,
     Select all, Find..., Find next, Clear screen, Clear scrollback, Reset
     terminal; View Bigger font, Smaller font, 80 x 24, 132 x 43.
  2. /size COLSxROWS: the window's inner area is that many cells, or the
     command is refused and the window stays when the screen is too small
     (80x24 on the rig's 640x256 screen).
  3. /font-size to the face's next designed size (topaz 9 up from topaz 8
     on tall pixels, topaz 11 down from TopazPro 16 on square ones): /size
     40x12 again gives another height; the opposite step goes back.
  4. Select all (Right Amiga A): the text area shows the selection (a pixel of the empty
     area changes colour); a click clears it.
  5. /clear scrollback: a word scrolled off the top is no longer found
     (UPTerm find; the word comes through a shell variable so the typed
     line cannot be the hit).
  6. /about: the About requester opens.
  7. Settings > Cursor > Blinking, picked from the menu in an idle window:
     the cursor blinks; picked again, it is steady.

Run with the rig up and the handler installed (rig.py install + a reboot):
  python3 tools/rig/menus_rig.py
"""
import pathlib, shutil, struct, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
RET = 0x44
RCMD = 0x0080
TITLE = 'menus'
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def t(s, wait=1.5):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)
    ami.key(RET)
    time.sleep(wait)


def strip():
    """{menu title: [item names]} of the active window"""
    out, titles = {}, {}
    for l in ami.req(0x0F).decode('latin-1').splitlines():
        f = l.split()
        if l.startswith('M ') and len(f) > 2:
            titles[f[1]] = l.split('"')[-2]
            out[titles[f[1]]] = []
        elif l.startswith('I ') and len(f) > 5 and f[3] == '-':
            name = l.split('"')[-2]
            if name:
                out.setdefault(titles.get(f[1]), []).append(name)
    return out


def box():
    return ami.window(TITLE)['box']


def pixel(x, y):
    b = ami.req(0x07)
    w = struct.unpack('>H', b[2:4])[0]
    o = 8 + (y * w + x) * 3
    return tuple(b[o:o + 3])


def find_cursor():
    """the middle of the block cursor: a run of 8 pixels of one colour that
    is not the background, on a text row (glyph strokes are at most 6 wide;
    the title bar above and the scroll bar and borders are left out). Its
    colour is the palette's, not the default foreground: the planar cursor
    flips the planes in use, a blue pen on the rig's Workbench (2026-10-15).
    None when not found"""
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    x0, y0, ww, hh = box()
    for y in range(y0 + 14, y0 + hh - 4):
        run, last = 0, None
        for x in range(x0 + 4, x0 + ww - 20):
            o = 8 + (y * w + x) * 3
            px = tuple(b[o:o + 3])
            run = run + 1 if px == last and px != (0, 0, 0) else 1
            last = px
            if run == 8:
                return (x - 4, y + 2)
    return None


def screen_width():
    for l in ami.req(0x0D).decode('latin-1').splitlines():
        if l.startswith('S '):
            return int(l.split()[2].split('x')[0])
    return 640


def rc_of(cmd):
    c.run('Delete RAM:mn.rc QUIET')
    t(cmd)
    t('Echo >RAM:mn.rc $RC')
    return c.run('Type RAM:mn.rc')[1].strip()


def click(x, y):
    kx, ky = ami.pointer_scale()
    ami.script(('move', int(x * kx), int(y * ky)), ('wait', 2), ('button', 0, 1), ('wait', 2),
               ('button', 0, 0), ('wait', 5))
    time.sleep(1)


def main():
    shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
    try:
        c.run('Run >NIL: NewShell "XCON:0/20/500/200/%s/CLOSE"' % TITLE)
        time.sleep(4)
        x, y, w, h = box()
        click(x + 200, y + 100)

        # 1
        st = strip()
        want_edit = ['Copy', 'Paste', 'Select all', 'Find...', 'Find next', 'Clear screen', 'Clear scrollback',
                     'Reset terminal']
        want_view = ['Bigger font', 'Smaller font', '80 x 24', '132 x 43']
        check(list(st)[:4] == ['UP-Term', 'Edit', 'View', 'Settings'], 'the strip: UP-Term, Edit, View, Settings',
              list(st))
        check(st.get('Edit') == want_edit and st.get('View') == want_view, 'the Edit and View items',
              (st.get('Edit'), st.get('View')))

        # 2, 3: /size sets the window to cols x rows cells, or is refused
        # (window unchanged) when the screen is too small for the grid. The
        # rig's screen is the Workbench's (PAL 640x256 since wasabi, topaz 8
        # on tall pixels): 80 columns of topaz 8 plus the borders do not fit
        # it, and the handler answers "the screen is too small for it".
        # The cell size comes from two sizes that fit.
        sw = screen_width()
        t('/size 60x10', wait=3)
        x, y, wa, ha = box()
        t('/size 40x12', wait=3)
        x, y, w1, h1 = box()
        cw, bw = (wa - w1) // 20, w1 - 40 * ((wa - w1) // 20)
        check(wa > w1 and (wa - w1) % 20 == 0 and 6 <= cw <= 16 and bw < 40,
              '/size 60x10 and 40x12: the width follows the columns, a whole cell each', (wa, w1))
        t('/size 80x24', wait=3)
        got = box()[2:]
        if bw + 80 * cw > sw:
            check(got == (w1, h1), '/size 80x24 on a %d pixel screen: refused, the window stays' % sw, (got, w1, h1))
        else:
            check(got[0] == bw + 80 * cw, '/size 80x24 gives the window 80 cells', got)
        t('/size 40x12', wait=3)
        # /font-size steps to the face's next designed size: a rig with
        # tall pixels has topaz 8 -> 9 up and nothing down, square pixels
        # (TopazPro 16) topaz 11 down. Whichever direction exists changes
        # the 40 x 12 window's height, and the opposite one goes back.
        x, y, w1, h1 = box()
        stepped = None
        for way, back in (('bigger', 'smaller'), ('smaller', 'bigger')):
            t('/font-size ' + way, wait=4)
            t('/size 40x12', wait=3)
            x, y, w2, h2 = box()
            if h2 != h1:
                stepped = (way, back)
                break
        check(stepped is not None, '/font-size steps to the face\'s next designed size: the 40 x 12 window changes',
              (h1, h2))
        if stepped:
            t('/font-size ' + stepped[1], wait=4)
            t('/size 40x12', wait=3)
            x, y, w3, h3 = box()
            check(h3 == h1, '/font-size %s goes back' % stepped[1], (h1, h3))
        else:
            check(False, '/font-size back: no step to go back from')
        t('/size 60x10', wait=3)

        # 4
        x, y, w, h = box()
        empty = (x + w - 40, y + h - 30)
        before = pixel(*empty)
        # Right Amiga A, the menu item's shortcut: a typed /select-all is
        # followed by the new prompt, which writes over the selected cells
        # and drops the selection (xterm's rule, UT5) before it can be read
        ami.key(0x20, RCMD)
        time.sleep(3)
        sel = pixel(*empty)
        check(sel != before, 'Select all shows the whole area selected', (before, sel))
        click(x + 40, y + 40)
        check(pixel(*empty) == before, 'a click clears the selection', pixel(*empty))

        # 5
        t('Set w gone-word')
        t('Echo $w')
        for i in range(30):
            t('Echo line %d' % i, wait=0.3)
        # >NIL:: the answer ("find: gone-word") would put the word back on screen
        rc = rc_of('VTC:UPTerm find $w >NIL:')
        check(rc == '0', 'the scrolled-off word is found before', rc)
        t('/clear scrollback')
        rc = rc_of('VTC:UPTerm find $w >NIL:')
        check(rc == '10', '/clear scrollback: the word is gone', rc)

        # 7: Settings > Cursor > Blinking by a real menu pick (a typed command
        # prints an answer, and output started the frame clock anyway: only
        # a pick in an idle window shows the bug). The places are the rig's
        # Workbench screen (topaz 8), measured from a screenshot 2026-10-03; one row
        # lower since Settings > Screen came above Cursor.
        def pick_blinking():
            kx, ky = ami.pointer_scale()
            ami.script(('move', int(150 * kx), int(5 * ky)), ('wait', 2), ('button', 1, 1), ('wait', 8),
                       ('move', int(190 * kx), int(62 * ky)), ('wait', 8), ('move', int(240 * kx), int(62 * ky)),
                       ('wait', 4), ('move', int(260 * kx), int(103 * ky)), ('wait', 6), ('button', 1, 0),
                       ('wait', 5))
            time.sleep(1.5)

        t('/clear screen', wait=2)
        cur = find_cursor()
        check(cur is not None, 'the block cursor is found on screen')
        pick_blinking()
        seen = set()
        for i in range(12):
            seen.add(pixel(*cur))
            time.sleep(0.25)
        check(len(seen) >= 2, 'Settings > Cursor > Blinking picked in an idle window: the cursor blinks', seen)
        pick_blinking()
        seen = set()
        for i in range(10):
            seen.add(pixel(*cur))
            time.sleep(0.25)
        check(len(seen) == 1, 'unticked: the cursor is steady (the shapes are the steady ones)', seen)

        # 6
        t('/about', wait=2)
        about = ami.window('About UP-Term')
        check(about is not None, '/about opens the About requester')
        if about:
            ax, ay, aw, ah = about['box']
            click(ax + aw // 2, ay + ah - 8)  # its one gadget, OK, along the bottom
        t('EndCLI')
    finally:
        c.run('Delete RAM:mn.rc QUIET')
        (VTC / 'UPTerm').unlink(missing_ok=True)
    print('menus_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
