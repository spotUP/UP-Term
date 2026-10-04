#!/usr/bin/env python3
"""race_rig.py -- UP-Term against CCON, side by side on the rig, for the eye
(ledger S1). Two windows on one PAL hires screen, 16 colours: CCON above,
UP-Term below, the same size. Both shells wait for RAM:go (they look once a
second, so one may start up to a second before the other), then run the same
commands: a long listing, a text file typed, a file with a colour change
every few characters, a clear and a listing again. The console that draws
faster is done first; each prints its seconds at the end.

They share the one emulated CPU, so the times are not benchmark numbers
(conbench in one window at a time gives those): it is the same work at the
same moment, to look at.

Needs creep's ccon-handler in build/rig/vtc (github creep-ltx/AmigaTools,
ccon/ccon-handler) and the rig up with the handler installed:
  python3 tools/rig/rig.py start && python3 tools/rig/race_rig.py [gif]
With "gif": frames are grabbed during the race and written to
~/Desktop/upterm-vs-ccon.gif (the grabbing slows both the same).
"""
import pathlib, struct, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / 'build/rig/vtc'

SCRIPT = """.KEY NAME
.BRA {
.KET }
FailAt 21
Lab wait
If NOT EXISTS RAM:go
  Wait 1
  Skip wait BACK
EndIf
Date >RAM:{NAME}.t0
List SYS:C
Type VTC:race.txt
Type VTC:race-colour.txt
Echo "*ec" NOLINE
List SYS:C
Type VTC:race-colour.txt
Date >RAM:{NAME}.t1
Echo "{NAME} is done"
"""


def shot():
    from PIL import Image
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    if b[0] == 1:
        nc = struct.unpack('>H', b[6:8])[0]
        im = Image.frombytes('P', (w, h), bytes(b[8 + nc * 3:8 + nc * 3 + w * h]))
        im.putpalette(list(b[8:8 + nc * 3]))
        return im.convert('RGB')
    return Image.frombytes('RGB', (w, h), bytes(b[8:8 + w * h * 3]))


def click(title):
    x, y, w, h = ami.window(title)['box']
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)


def t(s, wt=1.0):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(wt)


def secs(name):
    def one(f):
        hms = c.run('Type RAM:%s.%s' % (name, f))[1].split()[-1].split(':')
        return int(hms[0]) * 3600 + int(hms[1]) * 60 + int(hms[2])
    return one('t1') - one('t0')


def main():
    gif = 'gif' in sys.argv[1:]
    if not (VTC / 'ccon-handler').exists():
        sys.exit('race_rig: build/rig/vtc/ccon-handler is missing (see the top of this file)')
    (VTC / 'race.s').write_text(SCRIPT)
    (VTC / 'race.txt').write_text(''.join('%03d  the quick brown fox jumps over the lazy dog, %d times\n' % (i, i)
                                          for i in range(150)))
    (VTC / 'race-colour.txt').write_bytes(b''.join(
        b''.join(b'\x1b[3%dm%s ' % (1 + (i + k) % 7, w) for k, w in
                 enumerate([b'red', b'green', b'yellow', b'blue', b'magenta', b'cyan', b'white', b'bold', b'plain']))
        + b'\x1b[0m\n' for i in range(60)))
    (VTC / 'ccon.mount').write_text('CCON:\n    Handler = VTCX:ccon-handler\n    StackSize = 8192\n'
                                    '    Priority = 5\n    GlobVec = -1\n#\n')
    c.run('Delete RAM:go RAM:CCON.t0 RAM:CCON.t1 RAM:UPTERM.t0 RAM:UPTERM.t1 QUIET')
    c.run('Mount CCON: FROM VTC:ccon.mount')
    # the screen is UP-Term's own (PAL hires, 16 colours); CCON opens on it as a visitor
    c.run('Run >NIL: NewShell "XCON:0/134/640/122/UP-Term/OWNSCREEN/SCREENMODE 0x29000/DEPTH 4/AMIGA"')
    time.sleep(8)
    c.run('Run >NIL: NewShell "CCON:0/12/640/122/CCON/SCREENUP-Term"')
    time.sleep(8)
    click('CCON')
    t('Execute VTC:race.s CCON')
    click('UP-Term')
    t('Execute VTC:race.s UPTERM')
    time.sleep(2)
    frames = []
    c.run('Echo >RAM:go go')
    t0 = time.time()
    done = {}
    while len(done) < 2 and time.time() - t0 < 300:
        if gif:
            frames.append(shot())
        else:
            time.sleep(1)
        for n in ('CCON', 'UPTERM'):
            if n not in done and c.run('List RAM:%s.t1 NOHEAD' % n)[0] == 0:
                done[n] = time.time() - t0
    time.sleep(2)
    if gif:
        frames.append(shot())
    for n in ('UPTERM', 'CCON'):
        print('%-7s %s' % (n, ('%d s (the Amiga\'s clock)' % secs(n)) if n in done else 'not done in 300 s'))
    if gif and frames:
        out = pathlib.Path.home() / 'Desktop/upterm-vs-ccon.gif'
        big = [f.resize((f.width, f.height * 2)) for f in frames]  # PAL hires pixels are tall
        big[0].save(out, save_all=True, append_images=big[1:], duration=400, loop=0)
        print('wrote', out, len(frames), 'frames')
    c.run('Delete RAM:go QUIET')
    return 0


if __name__ == '__main__':
    sys.exit(main())
