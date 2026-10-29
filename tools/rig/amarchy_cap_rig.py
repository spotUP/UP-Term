#!/usr/bin/env python3
"""amarchy_cap_rig.py -- Amarchy screen capture (plan 2026-10-29-amarchy-p4-capture-notify-power.md R4.3, R4.4a, R4.4b, R4.5).

Entry point: the Amarchy commodity's ARexx verb `CAP screen|window|region` (the keymap binding is R4.7, not wired
yet), sent to the live commodity. Sentinel that the new code ran: the ARexx verb SHOTS answers the counter N and N moves only when
shot.c wrote a file; the file is a FORM ILBM in RAM:Shots whose BMHD is the screen / window / dragged rectangle.
The planar proof runs on the chipset
screen of tests/amiga/shotscreen.c, a 320x200x4 screen with the active window "shotwin" (160x90).
Expectations: screen -> BMHD 320x200; window -> BMHD = shotwin's Width x Height; region drag (40,40)-(240,140) in
wasabi pointer units (twice the 320x200 lores pixels: 100x50 pixels) -> BMHD 101x51 (inclusive; 201x101 on a hires screen); Esc in the region window -> no new file, shots unchanged; clipboard unit 0 holds the same bytes
as the file (FORM ILBM). Needs the rig up (rig.py start --max) and build/amiga/{Amarchy,snipprobe,shotscreen}. UPTERM_RIG=<n>."""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c
import amarchy_rig as ar

BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'build', 'amiga')
bad = 0
ESC = 0x45


def check(ok, what, seen=''):
    global bad
    bad += not ok
    print('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': %s' % (seen,)), flush=True)
    return ok


def cap(arg):
    m = ami._m()
    with ami._conn() as conn:
        r = m.arexx(conn, 'AMARCHY', 'CAP ' + arg)
    return r[0], r[2].strip()


def bmhd(buf):
    if buf is None or buf[:4] != b'FORM' or buf[8:12] != b'ILBM':
        return None
    pos = 12
    while pos + 8 <= len(buf):
        tag, size = buf[pos:pos + 4], struct.unpack('>I', buf[pos + 4:pos + 8])[0]
        if tag == b'BMHD':
            return struct.unpack('>HH', buf[pos + 8:pos + 12])
        pos += 8 + size + (size & 1)
    return None


def shots():
    return int(ar.arexx('SHOTS') or -1)


def form_ok(buf):
    return buf is not None and buf[:4] == b'FORM' and struct.unpack('>I', buf[4:8])[0] == len(buf) - 8


def clip_bytes():
    c.run('Delete RAM:clip.iff QUIET')
    c.run('RAM:snipprobe CLIPRAW RAM:clip.iff')
    return ami.read_file('RAM:clip.iff')


def wait_win(title, secs=15):
    end = time.time() + secs
    while time.time() < end:
        w = ami.window(title)
        if w:
            return w
        time.sleep(0.5)
    return None


def main():
    for n in ('Amarchy', 'snipprobe', 'shotscreen', 'shotfill'):
        ami.put(os.path.join(BUILD, n), 'RAM:' + n)
    c.run('Protect RAM:Amarchy RWED')
    if ar.port_up():
        ar.arexx('QUIT')
        time.sleep(2)
    c.run('Delete RAM:Shots ALL QUIET')
    import tempfile
    with tempfile.NamedTemporaryFile('w', delete=False, encoding='latin-1') as t:
        t.write('[profile default]\nshot-dir = RAM:Shots\n')
    ami.put(t.name, 'ENV:Amarchy/prefs')   # an earlier run's shot-dir must not leak into this one
    c.run('Run >NIL: RAM:Amarchy')
    time.sleep(3)
    check(ar.port_up(), 'Amarchy up')
    n0 = shots()
    check(n0 == 0, 'SHOTS answers 0 at start', n0)
    st = ar.arexx('STATUS') or ''
    check(len(st) > 95 and st.endswith(' shots=0') and 'wbrefused=' in st, 'STATUS is not cut at the old 96 bytes and ends with shots=0', (len(st), st[-30:]))

    c.run('Run >NIL: RAM:shotscreen 60')
    w = wait_win('shotwin')
    check(w is not None, 'chipset screen with shotwin is up')
    time.sleep(1)

    # screen
    rc, path = cap('screen')
    check(rc == 0 and path.startswith('RAM:Shots/shot-'), 'CAP screen rc 0, path in RAM:Shots', (rc, path))
    check(shots() == 1, 'shots=1 after CAP screen', shots())
    check((ar.arexx('STATUS') or '').endswith(' shots=1'), 'STATUS shots=1 after the capture')
    f = ami.read_file(path)
    check(form_ok(f) and bmhd(f) == (320, 200), 'screen file is FORM ILBM, BMHD 320x200', bmhd(f))
    cb = clip_bytes()
    check(cb is not None and cb == f, 'clipboard unit 0 holds the same FORM ILBM as the file', None if cb is None else len(cb))

    # window
    w = ami.window('shotwin')
    rc, path = cap('window')
    f = ami.read_file(path)
    want = (w['box'][2], w['box'][3]) if w else None
    check(rc == 0 and shots() == 2, 'CAP window rc 0, shots=2', (rc, shots()))
    check(form_ok(f) and bmhd(f) == want, 'window file BMHD = window Width x Height %s' % (want,), bmhd(f))
    cb = clip_bytes()
    check(cb == f, 'clipboard holds the window capture')

    # region
    before = set(c.run('List RAM:Shots LFORMAT %N')[1].split())
    rc, _ = cap('region')
    check(rc == 0, 'CAP region opens the rubber-band window', rc)
    time.sleep(1)
    ami.script(('move', 40, 40), ('wait', 3), ('button', 0, 1), ('wait', 3), ('move', 140, 90), ('wait', 3),
               ('move', 240, 140), ('wait', 3), ('button', 0, 0), ('wait', 5))
    time.sleep(2)
    check(shots() == 3, 'shots=3 after region drag', shots())
    new = sorted(set(c.run('List RAM:Shots LFORMAT %N')[1].split()) - before)
    f = ami.read_file('RAM:Shots/' + new[0]) if len(new) == 1 else None
    check(form_ok(f) and bmhd(f) == (101, 51), 'region file BMHD 101x51 (inclusive; the pointer script is in 640x400 units, this lores screen is half that, so 201x101 there)', bmhd(f))
    check(clip_bytes() == f, 'clipboard holds the region capture')

    # Esc
    rc, _ = cap('region')
    time.sleep(1)
    ami.key(ESC, 0)
    time.sleep(2)
    n = c.run('List RAM:Shots LFORMAT %N')[1].split()
    check(shots() == 3 and len(n) == 3, 'Esc: no new file, shots unchanged', (shots(), len(n)))

    # bad argument
    rc, _ = cap('bogus')
    check(rc == 10, 'CAP with a bad argument answers rc 10', rc)

    # Workbench (RTG truecolour): refused, not a corrupt file
    # Workbench (RTG truecolour) refusal: closing shotscreen puts the RTG Workbench in front
    end = time.time() + 120
    while ami.window('shotwin') and time.time() < end:
        time.sleep(2)
    # Workbench (RTG truecolour): a 24-bit deep ILBM (D-RTG), known pixels from FillPixelArray
    c.run('RAM:shotfill')
    # shot-dir key: the capture lands in the drawer the settings file names
    with tempfile.NamedTemporaryFile('w', delete=False, encoding='latin-1') as t:
        t.write('[profile default]\nshot-dir = RAM:ShotsKey\n')
    ami.put(t.name, 'ENV:Amarchy/prefs')
    rc, path = cap('screen')
    check(rc == 0 and path.startswith('RAM:ShotsKey/shot-'), 'shot-dir key: CAP screen lands in RAM:ShotsKey', (rc, path))
    check(form_ok(ami.read_file(path)), 'the file in the shot-dir drawer is a whole FORM')

    n = shots()
    rc, path = cap('screen')
    check(rc == 0 and shots() == n + 1, 'RTG Workbench: CAP screen writes a file (rc 0, counter +1)', (rc, shots(), path))
    buf = ami.read_file(path) if rc == 0 else None
    check(form_ok(buf), 'deep file is one whole FORM', None if buf is None else len(buf))
    if buf:
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
        import ilbm_decode
        wd, ht, depth, camg, cmap, rows = ilbm_decode.decode(buf)
        check((wd, ht, depth) == (800, 600, 24) and cmap == b'', 'deep BMHD 800x600x24, no CMAP', (wd, ht, depth, len(cmap)))
        check(rows[100][100] == 0xFF8040 and rows[107][115] == 0xFF8040, 'FillPixelArray rect 1 reads back 0xFF8040',
              (hex(rows[100][100]), hex(rows[107][115])))
        check(rows[200][300] == 0x1020FF and rows[203][303] == 0x1020FF, 'FillPixelArray rect 2 reads back 0x1020FF',
              (hex(rows[200][300]), hex(rows[203][303])))
        check(rows[99][99] != 0xFF8040 and rows[108][116] != 0xFF8040, 'pixels just outside rect 1 differ')
        cl = ami.read_file('ENV:Amarchy/clips') or b''
        check(b'ILBM' not in cl and b'BMHD' not in cl and b'BODY' not in cl, 'clipwatch did not add the ILBM clip to the text history', len(cl))
        cb = clip_bytes()
        check(cb == buf, 'the clipboard holds the same FORM as the file', None if cb is None else (len(cb), len(buf)))
    print('bad=%d' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
