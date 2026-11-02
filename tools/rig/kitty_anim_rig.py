#!/usr/bin/env python3
"""kitty_anim_rig.py -- K36: a kitty animation plays off the vtwin frame clock, and `image-animation` stops it.
Types a stream into an XCON: window: a 64x64 picture of four frames (red, green, blue, yellow, gap 100 ms each,
looping) started with a=a,s=3. Takes SHOTS screenshots about 150 ms apart (<out>-<n>.png) and measures, by
script, the pixels that differ between neighbouring shots.
  UPTERM_RIG=<n> python3 tools/rig/kitty_anim_rig.py <out-prefix> on|off
"on" (profile default): PASS when neighbouring shots differ (the clock switched frames).
"off" (profile image-animation = off): PASS when every shot equals the first (the clock never ran).
The PROF lines "PROF kitty anim switches/ticks" (handler built with PROF=1) go to serial.log: read them after
`rig.py stop`. Switch the screen first with `rig.py rtg` or `rig.py aga` (they reboot the rig).
"""
import base64, pathlib, struct, sys, time, zlib
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

VTC = paths.RIG / 'vtc'
S = 64
WX, WY, WW, WH = 0, 20, 400, 200
SHOTS = 4
COLS = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))


def apc(ctrl, data):
    """The data in 4096-byte base64 chunks (m=1 on all but the last)."""
    b = base64.b64encode(data)
    parts = [b[i:i + 4096] for i in range(0, len(b), 4096)] or [b'']
    out = b''
    for i, p in enumerate(parts):
        last = i == len(parts) - 1
        head = ctrl if i == 0 else ''
        m = 'm=0' if last else 'm=1'
        out += b'\x1b_G' + (head + ',' if head else '').encode() + m.encode() + b';' + p + b'\x1b\\'
    return out


def png(w, h, rgb):
    raw = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    out, mode = sys.argv[1], sys.argv[2]
    stream = b'\x1b[2J\x1b[3;3H' + apc('a=T,f=24,s=%d,v=%d,i=1,p=1,q=2' % (S, S), bytes(COLS[0]) * (S * S))
    for col in COLS[1:]:
        stream += apc('a=f,i=1,c=1,f=24,s=%d,v=%d,z=100,q=2' % (S, S), bytes(col) * (S * S))
    stream += b'\x1b_Ga=a,i=1,r=1,z=100,q=2\x1b\\' + b'\x1b_Ga=a,i=1,s=3,q=2\x1b\\' + b'\x1b[12;1H'
    (VTC / 'kanim-stream.bin').write_bytes(stream)
    had = c.run('Copy ENV:up-term/up-term RAM:kanim.bak QUIET')[0] == 0
    try:
        c.run('MakeDir ENV:up-term QUIET')
        c.run('Delete ENV:up-term/up-term QUIET')
        if mode == 'off':
            (VTC / 'kanim.conf').write_text('[profile default]\nimage-animation = off\n')
            c.run('Copy VTC:kanim.conf ENV:up-term/up-term QUIET')
        c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/kanim/CLOSE/XTERM"' % (WX, WY, WW, WH))
        time.sleep(4)
        kx, ky = ami.pointer_scale()
        ami.script(('move', int((WX + 200) * kx), int((WY + 100) * ky)), ('wait', 2), ('button', 0, 1),
                   ('wait', 2), ('button', 0, 0), ('wait', 5))
        time.sleep(1)
        ami.req(0x08, bytes([4]) + b'Type VTC:kanim-stream.bin')
        time.sleep(0.5)
        ami.key(0x44)
        time.sleep(6)
        shots = []
        for n in range(SHOTS):
            b = ami.req(0x07)
            w, h = struct.unpack('>HH', b[2:6])
            px = bytes(b[8:8 + w * h * 3])
            pathlib.Path('%s-%d.png' % (out, n)).write_bytes(png(w, h, px))
            shots.append(px)
            time.sleep(0.15)
        diffs = []
        for n in range(1, SHOTS):
            diffs.append(sum(1 for i in range(0, len(px) - 2, 3) if shots[n][i:i + 3] != shots[n - 1][i:i + 3]))
        vs_first = [sum(1 for i in range(0, len(px) - 2, 3) if shots[n][i:i + 3] != shots[0][i:i + 3]) for n in range(1, SHOTS)]
        print('shot', w, h, 'neighbour diffs', diffs, 'vs first', vs_first)
        ok = (all(d == 0 for d in vs_first)) if mode == 'off' else (any(d > 0 for d in diffs))
        print('mode', mode, 'PASS' if ok else 'FAIL')
    finally:
        if had:
            c.run('Copy RAM:kanim.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:kanim.bak QUIET')
        (VTC / 'kanim.conf').unlink(missing_ok=True)
        ami.req(0x08, bytes([4]) + b'EndCLI')
        ami.key(0x44)
        time.sleep(3)


main()
