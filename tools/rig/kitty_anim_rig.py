#!/usr/bin/env python3
"""kitty_anim_rig.py -- K36: a kitty animation plays off the vtwin frame clock, and `image-animation` stops it.
Types a stream into an XCON: window: a 64x64 picture of four frames (red, green, blue, yellow, gap 100 ms each,
looping) started with a=a,s=3. Takes SHOTS screenshots about 150 ms apart (<out>-<n>.png) and measures, by
script, the pixels that differ between neighbouring shots.
  UPTERM_RIG=<n> python3 tools/rig/kitty_anim_rig.py <out-prefix> on|off [gap-ms]
gap-ms (default 100, "on" only): the frame gap. Above 100 the shots are SPAN_S apart over a long span and the tool
counts how many neighbouring shots changed: a clock that keeps pace changes about SPAN_S / gap per span (K36-GAP-1S).
"on" (profile default): PASS when neighbouring shots differ (the clock switched frames).
"off" (profile image-animation = off): PASS when every shot equals the first (the clock never ran).
The PROF lines "PROF kitty anim switches/ticks" (handler built with PROF=1) go to serial.log: read them after
`rig.py stop`. Switch the screen first with `rig.py rtg` or `rig.py aga` (they reboot the rig).
"""
import base64, pathlib, struct, sys, time, zlib
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths

VTC = paths.RIG / 'vtc'
S = 64
WX, WY, WW, WH = 0, 20, 400, 200
SHOTS = 4
SPAN_S, SHOT_EVERY_S = 8.0, 0.5
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


def crop_rgb(px, w, x0, y0, cw, ch):
    return b''.join(px[((y0 + y) * w + x0) * 3:((y0 + y) * w + x0 + cw) * 3] for y in range(ch))


def window_diffs(shots, w, h, rect=(WX, WY, WW, WH)):
    """Pixels that differ inside the terminal window's rectangle only (screen pixels, clamped to the shot), as
    (neighbour diffs, diffs vs the first shot). Outside the window the pointer and screen repaint leave residue that
    is not the animation (K36-OFF-DIFF)."""
    x0, y0 = max(0, rect[0]), max(0, rect[1])
    cw, ch = min(w, rect[0] + rect[2]) - x0, min(h, rect[1] + rect[3]) - y0
    wins = [crop_rgb(px, w, x0, y0, cw, ch) for px in shots]

    def count(a, b):
        return sum(1 for i in range(0, len(a) - 2, 3) if a[i:i + 3] != b[i:i + 3])
    return ([count(wins[n], wins[n - 1]) for n in range(1, len(wins))],
            [count(wins[n], wins[0]) for n in range(1, len(wins))])


def build_stream(gap=100):
    """The kitty animation stream the rig types and the kit ships as dist/updemo/kanim-stream.bin
    (tools/gen_updemo_media.py): four 64x64 frames, `gap` ms each, looping."""
    stream = b'\x1b[2J\x1b[3;3H' + apc('a=T,f=24,s=%d,v=%d,i=1,p=1,q=2' % (S, S), bytes(COLS[0]) * (S * S))
    for col in COLS[1:]:
        stream += apc('a=f,i=1,c=1,f=24,s=%d,v=%d,z=%d,q=2' % (S, S, gap), bytes(col) * (S * S))
    stream += b'\x1b_Ga=a,i=1,r=1,z=%d,q=2\x1b\\' % gap + b'\x1b_Ga=a,i=1,s=3,q=2\x1b\\' + b'\x1b[12;1H'
    return stream


def main():
    import ami, condev_rig as c
    out, mode = sys.argv[1], sys.argv[2]
    gap = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    nshots, every = (SHOTS, 0.15) if gap <= 100 else (int(SPAN_S / SHOT_EVERY_S) + 1, SHOT_EVERY_S)
    stream = build_stream(gap)
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
        t0 = time.time()
        for n in range(nshots):
            b = ami.req(0x07)
            w, h = struct.unpack('>HH', b[2:6])
            px = bytes(b[8:8 + w * h * 3])
            pathlib.Path('%s-%d.png' % (out, n)).write_bytes(png(w, h, px))
            shots.append(px)
            time.sleep(every)
        span = time.time() - t0
        diffs, vs_first = window_diffs(shots, w, h)
        if gap > 100:
            print('gap', gap, 'ms; shot span %.1f s; changed neighbour pairs %d of %d; expected about %.1f' % (span, sum(1 for d in diffs if d), len(diffs), span * 1000 / gap))
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


if __name__ == '__main__':
    main()
