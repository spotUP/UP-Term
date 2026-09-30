#!/usr/bin/env python3
"""snip_rig.py -- D1.3/DD10 of the console.device plan: with UP-Term's
console.device, a SNIPMAP unit's text dragged over with the mouse and
copied with Right Amiga C is on the clipboard; a CHARMAP unit copies
nothing (tests/amiga/snipprobe). Then XCON:'s own copy, which runs on the
same window core (render/vtwin). The rig must be up; `make
build/amiga/snipprobe build/amiga/up-console.device build/amiga/UPConsole`."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
failed = 0


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()), flush=True)


def copy_run(arg):
    run('Delete RAM:snipprobe.log QUIET')
    run('Run >NIL: VTC:snipprobe ' + arg)
    for _ in range(20):
        time.sleep(0.5)
        if 'READY' in run('Type RAM:snipprobe.log')[1]:
            break
    time.sleep(1)
    w = ami.window('snipprobe')
    x, y, ww, hh = w['box']
    kx, ky = ami.pointer_scale()
    # the text is on the first row of the unit: from its first cell to past its end
    y0 = y + 14 + 4
    x0, x1 = x + 6, x + 6 + 17 * 8 + 4
    ev = [('move', int(x0 * kx), int(y0 * ky)), ('wait', 2), ('button', 0, 1), ('wait', 3)]
    for i in range(1, 9):
        ev += [('move', int((x0 + (x1 - x0) * i // 8) * kx), int(y0 * ky)), ('wait', 2)]
    ev += [('wait', 3), ('button', 0, 0), ('wait', 10)]
    ami.script(*ev)
    time.sleep(1)
    ami.key(0x33, 0x0080)  # Right Amiga + C
    for _ in range(40):
        time.sleep(0.5)
        out = run('Type RAM:snipprobe.log')[1]
        if 'CLIP' in out:
            return out
    return run('Type RAM:snipprobe.log')[1]


def drag_copy(title, row_y):
    w = ami.window(title)
    x, y, ww, hh = w['box']
    kx, ky = ami.pointer_scale()
    y0 = y + row_y
    x0, x1 = x + 6, x + 6 + 17 * 8 + 4
    ev = [('move', int(x0 * kx), int(y0 * ky)), ('wait', 2), ('button', 0, 1), ('wait', 3)]
    for i in range(1, 9):
        ev += [('move', int((x0 + (x1 - x0) * i // 8) * kx), int(y0 * ky)), ('wait', 2)]
    ev += [('wait', 3), ('button', 0, 0), ('wait', 10)]
    ami.script(*ev)
    time.sleep(1)
    ami.key(0x33, 0x0080)  # Right Amiga + C
    time.sleep(2)


def xcon_copy():
    """XCON:'s own copy (the same window core): the first text row dragged over."""
    run('Delete RAM:snipprobe.log QUIET')
    run('VTC:snipprobe CLIP')  # nothing: only to clear the log
    run('Run >NIL: NewShell "XCON:0/20/400/80/xsnip"')
    time.sleep(4)
    ami.req(0x08, bytes([4]) + b'echo xcon copy me')
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(2)
    ami.req(0x08, bytes([4]) + b'endcli')  # typed, not entered: the line to copy stays above
    time.sleep(1)
    drag_copy('xsnip', 14 + 4 + 8)  # row 1 (row 0 is the Shell's banner): prompt and command
    run('Delete RAM:snipprobe.log QUIET')
    run('VTC:snipprobe CLIP')
    out = run('Type RAM:snipprobe.log')[1]
    ami.key(0x44)  # endcli
    return out


def main():
    for f in ("snipprobe", "up-console.device", "UPConsole"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    rc, out = run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    check(rc == 0, 'DEVICE ON', out)
    out = copy_run('')
    check('CLIP snipprobe copy me' in out, 'a SNIPMAP unit copies the dragged text', out)
    out = copy_run('CHARMAP')
    check('CLIP -' in out, 'a CHARMAP unit copies nothing', out)
    run('VTC:UPConsole DEVICE OFF')
    out = xcon_copy()
    check(re.search(r'CLIP \d+\.BOOTX:> echo xco', out), 'an XCON: window copies the dragged text', out)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
