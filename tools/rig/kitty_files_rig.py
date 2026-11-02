#!/usr/bin/env python3
"""kitty_files_rig.py -- K27: kitty graphics t=t / t=f / refused path through the handler's file reader.

Rig with a PROF=1 handler. Writes two raw 32x32 RGB files (red/yellow stripes, green/blue stripes), puts them in
T: (the first named tty-graphics-protocol-*, the second plain), types a stream sending the first with t=t, the second
with t=f and a CON: path with t=f into an XCON: window, then lists T: and takes a screenshot (raw RGB, <out>.rgb).
Prints coloured-pixel counts of the screen and the T: listing; the handler's PROF counters
("kitty files read/deleted") are in serial.log after `rig.py stop`.

  UPTERM_RIG=<n> python3 tools/rig/kitty_files_rig.py <out-prefix>
"""
import base64, pathlib, struct, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

VTC = paths.RIG / 'vtc'
W = H = 32
WX, WY, WW, WH = 0, 20, 400, 200


def stripes(cols):
    return b''.join(bytes(cols[(y // 8) % 2]) * W for y in range(H))


def apc(ctrl, path):
    return b'\x1b_G' + ctrl.encode() + b';' + base64.b64encode(path.encode()) + b'\x1b\\'


def main():
    out = sys.argv[1]
    (VTC / 'kf1.rgb').write_bytes(stripes([(255, 0, 0), (255, 255, 0)]))
    (VTC / 'kf2.rgb').write_bytes(stripes([(0, 255, 0), (0, 0, 255)]))
    stream = (apc('a=T,t=t,f=24,s=32,v=32,q=2', '/tmp/tty-graphics-protocol-1.rgb') + b'\r\n'
              + apc('a=T,t=f,f=24,s=32,v=32,q=2', 'T:kf2.rgb') + b'\r\n'
              + apc('a=T,t=f,f=24,s=32,v=32,i=9', 'CON:') + b'\r\n')
    (VTC / 'kf-stream.bin').write_bytes(stream)
    c.run('Copy VTC:kf1.rgb T:tty-graphics-protocol-1.rgb QUIET')
    c.run('Copy VTC:kf2.rgb T:kf2.rgb QUIET')
    print('T: before:', c.run('List T: LFORMAT "%N" >VTC:kf-list0.txt')[0], (VTC / 'kf-list0.txt').read_text().split())
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/kfiles/CLOSE/XTERM"' % (WX, WY, WW, WH))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((WX + 200) * kx), int((WY + 100) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:kf-stream.bin')
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(8)
    ami.key(0x44)  # the EBADF reply sits in front of the next typed line: clear it
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Echo alive-after-con >VTC:kf-alive.txt')
    ami.key(0x44)
    time.sleep(3)
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    px = b[8:]
    pathlib.Path(out + '.rgb').write_bytes(struct.pack('>HH', w, h) + bytes(px))
    near = lambda p, t: all(abs(p[i] - t[i]) < 40 for i in range(3))
    cnt = {n: 0 for n in ('red', 'yellow', 'green', 'blue')}
    tg = {'red': (255, 0, 0), 'yellow': (255, 255, 0), 'green': (0, 255, 0), 'blue': (0, 0, 255)}
    for i in range(0, len(px) - 2, 3):
        p = px[i:i + 3]
        for n, t in tg.items():
            if near(p, t):
                cnt[n] += 1
    print('shot', w, h, 'pixels', cnt)
    c.run('List T: LFORMAT "%N" >VTC:kf-list1.txt')
    print('T: after:', (VTC / 'kf-list1.txt').read_text().split())
    print('alive:', (VTC / 'kf-alive.txt').read_text().strip() if (VTC / 'kf-alive.txt').exists() else None)
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)


main()
