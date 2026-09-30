#!/usr/bin/env python3
"""ami.py -- client for amiagent (Aminet comm/net/amiagent, PROTOCOL.md) in the rig.
   ami.py ping | break | info | exec "cmd" [secs] | menus [win] | uitree | ui verb win gadget [text]
          | shot out.png [x y w h] | rexx "program" | get path out | put local path
          | key code [qual] | screens
          | gclick win gadget   (clicks a UITREE gadget on the front screen, scaled: amiagent's
                                 INPUT takes Intuition pointer units -- twice the lines of a
                                 non-interlaced screen -- while UITREE gives screen pixels;
                                 needs DCT:ptrpos)"""
import socket, struct, sys, zlib

HOST, PORT, TOKEN = '127.0.0.1', 7846, b'rigtoken'

def req(code, payload=b'', timeout=150):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    def frame(c, p): s.sendall(b'AMI0' + bytes([c, 0, 0, 0]) + struct.pack('>I', len(p)) + p)
    def recv_frame():
        h = b''
        while len(h) < 12:
            d = s.recv(12 - len(h))
            if not d: raise SystemExit('connection closed')
            h += d
        st, n = h[4], struct.unpack('>I', h[8:12])[0]
        body = b''
        while len(body) < n:
            d = s.recv(min(65536, n - len(body)))
            if not d: break
            body += d
        return st, body
    frame(0x10, TOKEN); st, body = recv_frame()
    if st != 0: raise SystemExit('auth failed: %r' % body)
    frame(code, payload); st, body = recv_frame(); s.close()
    if st != 0: raise SystemExit('ERR: ' + body.decode('latin-1'))
    return body

def key(code, qual=0):
    # Down, 2 ticks, up in ONE request (INPUT op 8 SCRIPT): two requests
    # arrive hundreds of ms apart under host load and the Amiga auto-
    # repeats the 'held' key (garbled tcsh input, 2026-09-29).
    ev = lambda d: bytes([3, code, d]) + struct.pack('>H', qual)
    req(0x08, bytes([8, 3]) + ev(1) + bytes([9]) + struct.pack('>H', 2) + ev(0))

def script(*events):
    # INPUT op 8 SCRIPT: events run on the Amiga's own ticks (a drag held
    # across TCP round trips reads as a held button). Each event is
    # ('move', x, y) | ('button', b, down) | ('wait', ticks).
    body = b''
    for e in events:
        if e[0] == 'move': body += bytes([1]) + struct.pack('>HH', int(e[1]), int(e[2]))
        elif e[0] == 'button': body += bytes([2, e[1], e[2]])
        elif e[0] == 'wait': body += bytes([9]) + struct.pack('>H', e[1])
        else: raise ValueError(e)
    req(0x08, bytes([8, len(events)]) + body)

def pointer_scale():
    # INPUT MOVE takes Intuition pointer units, UITREE gives screen pixels:
    # park the pointer at (200, 200) and ask POINTER where the screen has it.
    req(0x08, bytes([1]) + struct.pack('>HH', 200, 200))
    x, y = struct.unpack('>HH', req(0x0B)[:4])
    return 200.0 / max(x, 1), 200.0 / max(y, 1)

def window(title):
    # The front screen's window titled exactly `title`, from UITREE:
    # {'box': (x, y, w, h), 'active': bool, 'sys:size': (x, y, w, h), ...}
    import shlex
    found = None
    for line in req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W '):
            if found is not None: break
            f = shlex.split(line)
            if f[-1] == title:
                w, h = map(int, f[4].split('x'))
                found = {'box': (int(f[2]), int(f[3]), w, h), 'active': f[5] == 'active'}
        elif found is not None and line.startswith('G '):
            f = shlex.split(line)
            w, h = map(int, f[4].split('x'))
            found.setdefault(f[5], (int(f[2]), int(f[3]), w, h))
    return found

def png(path, w, h, rows):
    raw = b''.join(b'\0' + r for r in rows)
    def ch(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))

def main(a):
    cmd = a[0]
    if cmd == 'ping': print(req(0x01).decode('latin-1'))
    elif cmd == 'break': print(req(0x09).decode('latin-1'))
    elif cmd == 'info': print(req(0x06).decode('latin-1'))
    elif cmd == 'exec':
        b = req(0x02, struct.pack('>H', int(a[2]) if len(a) > 2 else 60) + a[1].encode('latin-1'))
        print('rc', struct.unpack('>I', b[:4])[0]); sys.stdout.write(b[4:].decode('latin-1'))
    elif cmd == 'menus': print(req(0x0F, (a[1] if len(a) > 1 else '').encode('latin-1')).decode('latin-1'))
    elif cmd == 'uitree': print(req(0x0D).decode('latin-1'))
    elif cmd == 'ui': print(req(0x0E, '\t'.join(a[1:]).encode('latin-1')).decode('latin-1'))
    elif cmd == 'rexx':
        b = req(0x12, a[1].encode('latin-1')); print('rc', struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1'))
    elif cmd == 'get': open(a[2], 'wb').write(req(0x03, a[1].encode('latin-1')))
    elif cmd == 'put':
        p = a[2].encode('latin-1'); req(0x04, struct.pack('>H', len(p)) + p + open(a[1], 'rb').read())
    elif cmd == 'screens':
        b = req(0x0A); print(b.hex())
    elif cmd == 'shot':
        payload = struct.pack('>4H', *map(int, a[2:6])) if len(a) >= 6 else b''
        b = req(0x07, payload)
        fmt, w, h, nc = b[0], *struct.unpack('>HHH', b[2:8])
        if fmt == 1:
            pal = b[8:8 + nc * 3]; px = b[8 + nc * 3:]
            rows = [b''.join(pal[i * 3:i * 3 + 3] for i in px[y * w:(y + 1) * w]) for y in range(h)]
        else:
            px = b[8:]; rows = [px[y * w * 3:(y + 1) * w * 3] for y in range(h)]
        png(a[1], w, h, rows); print(a[1], w, h)
    elif cmd == 'key':
        # INPUT op 3 KEY: rawcode u8, down u8, qualifier u16 -- down, then up
        code, qual = int(a[1], 16), int(a[2], 16) if len(a) > 2 else 0
        key(code, qual)
    elif cmd == 'type': req(0x08, bytes([4]) + a[1].encode('latin-1'))
    elif cmd == 'gclick':
        # the scale: park the pointer at (200, 200) and read where the front screen has it
        req(0x08, bytes([1]) + struct.pack('>HH', 200, 200))
        import re
        out = req(0x02, struct.pack('>H', 10) + b'DCT:ptrpos')[4:].decode('latin-1')
        sx, sy = map(int, re.search(r'" (-?\d+),(-?\d+) viewmodes', out).groups())
        kx, ky = 200.0 / max(sx, 1), 200.0 / max(sy, 1)
        tree = req(0x0D).decode('latin-1').splitlines()
        win = None
        for line in tree:
            f = line.split()
            if line.startswith('W ') and a[1].lower() in line.lower():
                win = f[1]
            elif win is not None and line.startswith('W '):
                win = None
            elif win is not None and line.startswith('G ') and a[2].lower() in line.lower():
                x, y = int(f[2]), int(f[3]); w, h = map(int, f[4].split('x'))
                cx, cy = int((x + w // 2) * kx), int((y + h // 2) * ky)
                req(0x08, bytes([5]) + struct.pack('>HH', cx, cy) + bytes([0, 1]))
                print('click at %d,%d (scale %.2f,%.2f)' % (cx, cy, kx, ky)); return
        raise SystemExit('no matching gadget')
    else: raise SystemExit(__doc__)

if __name__ == '__main__': main(sys.argv[1:])
