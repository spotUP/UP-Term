#!/usr/bin/env python3
"""chainprobe_rig.py -- DP1 of the console.device plan on the rig: runs
tests/amiga/chainprobe (an IDCMP-less SIMPLE_REFRESH window and a CON:
window, input handlers at 9/5/-5 counting every event class) and drives
the machine meanwhile: types into each window, clicks, drags, resizes,
depth-arranges, drags one window over the other (damage for REFRESHWINDOW)
and clicks both close gadgets. Prints the probe's log (handler list, count
table, RESULT lines) and saves it to build/rig/shots/chainprobe.log;
exits non-zero on a FAIL line. The rig must be up;
`make build/amiga/chainprobe` first."""
import os, pathlib, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
OUT = paths.RIG / "shots"
SECONDS = 50

def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

def centre(box):
    x, y, w, h = box
    return x + w // 2, y + h // 2

def main():
    shutil.copyfile(ROOT / "build/amiga/chainprobe", VTC / "chainprobe")
    OUT.mkdir(parents=True, exist_ok=True)
    run('Run >NIL: VTC:chainprobe %d' % SECONDS)
    time.sleep(3)
    kx, ky = ami.pointer_scale()
    P = lambda x, y: ('move', int(x * kx), int(y * ky))

    def click(x, y):
        ami.script(P(x, y), ('wait', 2), ('button', 0, 1), ('wait', 2), ('button', 0, 0), ('wait', 5))

    def drag(frm, dx, dy):
        x, y = frm
        steps = [P(x + dx * i // 8, y + dy * i // 8) for i in range(1, 9)]
        ev = [P(x, y), ('wait', 2), ('button', 0, 1), ('wait', 3)]
        for s in steps:
            ev += [s, ('wait', 2)]
        ev += [('wait', 3), ('button', 0, 0), ('wait', 10)]
        ami.script(*ev)

    def win(title):
        w = ami.window(title)
        if not w:
            raise SystemExit('no window titled %s on the front screen' % title)
        return w

    def type_in(title, text):
        x, y, w, h = win(title)['box']
        click(x + w // 2, y + h // 2)
        ami.req(0x08, bytes([4]) + text.encode('latin-1')); time.sleep(0.4)
        ami.key(0x44); time.sleep(0.5)

    steps = []
    def step(what, fn):
        fn(); steps.append(what); print('step:', what); time.sleep(1)

    step('type into the IDCMP-less window', lambda: type_in('chainprobe', 'abc'))
    step('drag the IDCMP-less window', lambda: drag(centre(win('chainprobe')['sys:drag']), 40, 20))
    step('resize the IDCMP-less window', lambda: drag(centre(win('chainprobe')['sys:size']), 60, 30))
    step('depth gadget of the IDCMP-less window', lambda: click(*centre(win('chainprobe')['sys:depth'])))
    step('depth gadget again', lambda: click(*centre(win('chainprobe')['sys:depth'])))
    step('type into the CON: window', lambda: type_in('chainprobe-con', 'xyz'))
    step('resize the CON: window', lambda: drag(centre(win('chainprobe-con')['sys:size']), 40, 30))
    # the CON: window over the probe window and back: damage to a SIMPLE_REFRESH window
    step('drag the CON: window over the IDCMP-less one', lambda: drag(centre(win('chainprobe-con')['sys:drag']), -250, 0))
    step('drag it back', lambda: drag(centre(win('chainprobe-con')['sys:drag']), 250, 0))
    step('close gadget of the IDCMP-less window', lambda: click(*centre(win('chainprobe')['sys:close'])))
    step('close gadget of the CON: window', lambda: click(*centre(win('chainprobe-con')['sys:close'])))
    ami.main(['shot', str(OUT / 'chainprobe.png')])

    log = ''
    for _ in range(SECONDS + 30):
        rc, log = run('Type RAM:chainprobe.log')
        if 'passed ' in log:
            break
        time.sleep(1)
    (OUT / 'chainprobe.log').write_text(log)
    sys.stdout.write(log)
    if 'passed ' not in log:
        print('FAIL the probe did not finish')
        return 1
    return 0 if 'FAIL' not in log else 1

if __name__ == '__main__':
    sys.exit(main())
