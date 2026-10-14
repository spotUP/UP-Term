#!/usr/bin/env python3
"""ami.py -- the rig's Amiga client, a client of wasabid (https://github.com/creep-ltx/wasabi, `make wasabi`).
   ami.py ping | info | exec "cmd" [secs] | menus [win] | uitree | ui verb win gadget [text]
          | shot out.png [x y w h] | rexx "program" | get path out | put local path
          | key code [qual] | type text | screens
          | gclick win gadget   (clicks a uitree gadget by its id, matched exactly: '90' is not '290')

The importers (72 files) keep the API they were written against: req(opcode, payload) with the legacy opcode
numbers and answer shapes (UITREE and MENUS text, SHOT bytes, EXEC rc + output), key, script, window, grab, put,
read_file, restore_file, pointer_scale, click_px, png, main. Underneath, every call is wasabi's own client code
(tools/rig/wasabi.py load_client: Conn, do_run, put/get, read_ui, list_windows, grab_pixels, ui_click, arexx): this
file translates, it does not speak the protocol itself. There is no other agent to fall back to.

Coordinates: wasabi's mouse and window lists are in grab space (rows doubled on a 640x256 screen), which is exactly
the pointer units the old INPUT calls took. UITREE text is rebuilt in screen pixels (rows divided back), so
pointer_scale() is (1, row_scale) and every script that scales a uitree box by it keeps working.

BREAK (0x09) is Ctrl-C to every command running in a run slot (SLOTS, then FREE with the Ctrl-C-only flag: reaches
exactly that command, not a same-named task).
Gaps (raise Gap, named, SystemExit): POINTER (wasabi has no pointer query: MOUSE only moves, WINDOWS/INFO carry no
pointer position), and any INPUT sub-operation or UIACT verb that has no wasabi command.
Mapped to the nearest command: raw key codes -> KEY frame with raw (code, qualifier) events; region grab -> crop
of the full grab (raw, rows not doubled); gadget by id -> id from the gadget list, then a mouse click; tick-timed
input script -> mouse move/down/up frames with the waits timed on the host (50 ticks per second): a held button
crosses TCP round trips, so a very short wait can stretch."""
import contextlib, os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths   # the one reader of UPTERM_RIG: rig 2's wasabid is on 1235
import wasabi as rigwasabi

# the emulator by default; a real Amiga running wasabid with AMI_HOST and AMI_TOKEN (its key: never in this repo)
# set in the environment
HOST = os.environ.get('AMI_HOST', '127.0.0.1')
PORT = int(os.environ.get('AMI_PORT') or paths.WASABI_PORT)
TOKEN = os.environ.get('AMI_TOKEN', paths.WASABI_KEY)

NOPOS = -32768          # MOUSE: keep the pointer where it is
TICKS = 50.0            # Amiga ticks per second (script waits)


class Gap(SystemExit):
    """A call wasabi cannot do; the message says which gap."""
    def __init__(self, what):
        super().__init__('ami.py gap: ' + what)


def _m():
    return rigwasabi.load_client()


@contextlib.contextmanager
def _conn(timeout=None):
    m = _m()
    try:
        c = m.Conn(HOST, PORT, TOKEN, timeout=timeout)
    except m.WasabiError as e:
        raise SystemExit('auth failed: %s' % e)
    try:
        yield c
    finally:
        c.close()


def _err(e):
    return SystemExit('ERR: ' + str(e))


def _mouse(c, action, button=0, count=0, x=NOPOS, y=NOPOS):
    c.send(_m().MOUSE, struct.pack('>HHHhh', action, button, count, x, y))
    c.expect_ok()


def _keys(c, events):
    """KEY op 0: raw (code, qualifier) events, key-up = code | 0x80."""
    c.send(_m().KEY, struct.pack('>H', 0) + b''.join(struct.pack('>HH', code, q) for code, q in events))
    c.expect_ok()


def _type(c, data):
    for i in range(0, len(data), 2048):
        c.send(_m().KEY, struct.pack('>H', 1) + data[i:i + 2048])
        c.expect_ok()


def _exec(c, payload):
    m = _m()
    secs = struct.unpack('>H', payload[:2])[0]
    out = []
    rc = m.do_run(c, payload[2:].decode('latin-1'), merge=True, sink=out.append, max_time=secs or None)
    return struct.pack('>I', rc) + ''.join(out).encode('latin-1')


def _break(c):
    """Ctrl-C to each command running in a run slot (one connection per RUN, so this one is a second connection)."""
    m = _m()
    for slot, state, _, _, _ in m.slot_rows(c):
        if state == 'running':
            c.send(m.FREE, struct.pack('>II', slot, m.FREE_BREAK_ONLY))
            m.drain_to_end(c, [].append)
    return b''


def _get(c, path):
    m = _m()
    parts = []
    c.send(m.GET, m.pack_str(path))
    m.drain_to_end(c, parts.append)
    return b''.join(parts)


def _text(c, tag, payload=b''):
    m = _m()
    parts = []
    c.send(tag, payload)
    m.drain_to_end(c, parts.append)
    return b''.join(parts)


def _shot(c, payload):
    m = _m()
    w, dh, _, pixels, _, _ = m.grab_pixels(c, raw=True)
    h = dh
    if payload:
        x, y, bw, bh = struct.unpack('>4H', payload[:8])
        x1, y1 = (x + bw if bw else w), (y + bh if bh else h)
        x0, y0, x1, y1 = min(x, w), min(y, h), min(x1, w), min(y1, h)
        pixels = m.crop_rgb(pixels, w, (x0, y0, x1, y1))
        w, h = x1 - x0, y1 - y0
    return bytes([2, 0]) + struct.pack('>HHH', w, h, 0) + pixels


def _screens(c):
    m = _m()
    rows = []
    for line in _text(c, m.SCREEN, struct.pack('>I', 0) + m.pack_str('')).decode('latin-1').splitlines():
        p = line.split(' ', 4)
        if len(p) >= 4:
            rows.append(p + [''] * (5 - len(p)))
    b = bytes([len(rows)])
    for i, (_, w, h, d, t) in enumerate(rows):
        t = t.encode('latin-1')[:255]
        b += bytes([i]) + struct.pack('>HHBIB', int(w), int(h), int(d), 0, len(t)) + t
    return b


def _q(s):
    return s.replace('"', "'")


def _uitree(c):
    """The front screen as amiagent's UITREE text: S, W (screen pixels), G (screen pixels, rows not doubled)."""
    m = _m()
    scr = m.read_ui(c)
    if not scr:
        return b''
    sc = scr[0]
    k = sc['row_scale']
    out = ['S 0 %dx%d depth=%d "%s"' % (sc['width'], sc['height'], sc['depth'], _q(sc['title']))]
    for i, w in enumerate(sc['windows']):
        out.append('W %d %d %d %dx%d %s "%s"' % (i, w['left'], w['top'] // k, w['width'], w['height'] // k,
                                                 'active' if w['active'] else '-', _q(w['title'])))
        for g in w['gadgets']:
            if g['kind'] == 'row':
                continue
            kind = 'sys:' + g['kind'] if g['kind'] in m.SYSTEM_KINDS else g['kind']
            state = ','.join(n for n in ('disabled', 'selected') if g[n]) or '-'
            line = 'G %d %d %d %dx%d %s %s "%s"' % (g['id'], g['x'], g['y'] // k, g['width'], g['height'] // k,
                                                    kind, state, _q(g['label']))
            if g['value'] and g['kind'] in ('string', 'integer'):
                line += ' "%s"' % _q(g['value'])
            out.append(line)
    return ('\n'.join(out) + '\n').encode('latin-1')


def _menus(c, sel):
    m = _m()
    scr = m.read_ui(c, every_screen=bool(sel))
    if sel:
        wins = [w for _, w in m.ui_windows(scr, sel)]
    else:
        wins = sorted((w for s in scr for w in s['windows'] if w['menus']), key=lambda w: not w['active'])
    if not wins:
        return b''
    w = wins[0]
    out = ['W "%s"' % _q(w['title'])]
    seen = set()
    items = sorted((i for i in w['menus'] if not i['bar']), key=lambda i: (i['code'] & 31, (i['code'] >> 5) & 63, (i['code'] >> 11) & 31))
    for it in items:
        mn = it['code'] & 31
        if mn not in seen:
            seen.add(mn)
            out.append('M %d on "%s"' % (mn, _q(it['path'].split('/')[0])))
    # amiagent listed a sub-menu's parent item (s '-', flags submenu) before its sub-items; wasabi lists the leaves
    # only, so the parent is rebuilt from the leaf's path
    rows = {}
    for it in items:
        mn, ii, sub = it['code'] & 31, (it['code'] >> 5) & 63, (it['code'] >> 11) & 31
        if sub != 31 and (mn, ii, 31) not in rows:
            rows[(mn, ii, -1)] = (mn | ii << 5 | 31 << 11, 'submenu', '', it['path'].split('/')[-2].rstrip(' \xbb'))
        rows[(mn, ii, sub)] = (it['code'], ','.join(n for n in ('disabled', 'checked') if it[n]) or '-',
                               it['key'] or '', it['path'].split('/')[-1])
    for (mn, ii, sub), (code, flags, key, text) in sorted(rows.items()):
        out.append('I %d %d %s %d %s "%s" "%s"' % (mn, ii, '-' if sub in (31, -1) else sub, code, flags,
                                                   _q(key), _q(text)))
    return ('\n'.join(out) + '\n').encode('latin-1')


def _input(c, p):
    op = p[0]
    if op == 1:
        x, y = struct.unpack('>HH', p[1:5]); _mouse(c, 0, 0, 0, x, y)
    elif op == 3:
        code, down, q = p[1], p[2], struct.unpack('>H', p[3:5])[0]
        _keys(c, [(code | (0 if down else 0x80), q)])
    elif op == 4:
        _type(c, p[1:])
    elif op == 5:
        x, y = struct.unpack('>HH', p[1:5]); _mouse(c, 1, p[5], p[6], x, y)
    elif op == 8:
        i = 2
        for _ in range(p[1]):
            e = p[i]
            if e == 1:
                x, y = struct.unpack('>HH', p[i + 1:i + 5]); _mouse(c, 0, 0, 0, x, y); i += 5
            elif e == 2:
                _mouse(c, 2 if p[i + 2] else 3, p[i + 1]); i += 3
            elif e == 9:
                time.sleep(struct.unpack('>H', p[i + 1:i + 3])[0] / TICKS); i += 3
            elif e == 3:
                code, down, q = p[i + 1], p[i + 2], struct.unpack('>H', p[i + 3:i + 5])[0]
                _keys(c, [(code | (0 if down else 0x80), q)]); i += 5
            else:
                raise Gap('INPUT script event %d has no wasabi command' % e)
    else:
        raise Gap('INPUT operation %d has no wasabi command' % op)


def _click_gadget(c, window, gadget, double=False):
    m = _m()
    if gadget.isdigit():
        for sc, w in m.ui_windows(m.read_ui(c, every_screen=bool(window)), window or None):
            for g in w['gadgets']:
                if g['id'] == int(gadget) and g['kind'] != 'row':
                    x, y = g['x'] + g['width'] // 2, g['y'] + g['height'] // 2
                    _mouse(c, 1, 0, 2 if double else 1, x, y)
                    return x, y
    w, g = m.ui_click(c, gadget, window or None, nth=1, double=double)
    return g['x'] + g['width'] // 2, g['y'] + g['height'] // 2


def _uiact(c, payload):
    m = _m()
    f = payload.decode('latin-1').split('\t')
    verb, window, gadget = f[0], f[1] if len(f) > 1 else '', f[2] if len(f) > 2 else ''
    if verb in ('click', 'dclick'):
        x, y = _click_gadget(c, window, gadget, verb == 'dclick')
        return ('click at %d,%d' % (x, y)).encode()
    if verb == 'settext':
        _click_gadget(c, window, gadget)
        _keys(c, m.key_combo_events('ramiga+x'))
        _type(c, (f[3] + '\r').encode('latin-1'))
        return b'set'
    if verb == 'menushort':
        _keys(c, m.key_combo_events('ramiga+' + gadget))
        return b'menushort'
    raise Gap('UIACT verb %r has no wasabi command' % verb)


def _rexx(c, prog):
    m = _m()
    q = '"' if '"' not in prog else ("'" if "'" not in prog else None)
    if q is None:
        raise Gap('ARexx program with both quote characters (wasabi sends the program as one quoted string)')
    rc, _, res = m.arexx(c, 'REXX', q + prog + q)
    return struct.pack('>I', rc & 0xFFFFFFFF) + res.encode('latin-1')


def req(code, payload=b'', timeout=150):
    """One legacy operation, answered in the legacy shape. timeout: the connection's silence limit."""
    m = _m()
    if code == 0x0B:
        raise Gap('POINTER position: wasabi has no pointer query (pointer_scale() is (1, row_scale))')
    with _conn(timeout) as c:
        try:
            if code == 0x01: return c.banner.encode('latin-1')
            if code == 0x02: return _exec(c, payload)
            if code == 0x03: return _get(c, payload.decode('latin-1'))
            if code == 0x04:
                n = struct.unpack('>H', payload[:2])[0]
                m.put_bytes(c, payload[2 + n:], payload[2:2 + n].decode('latin-1')); return b''
            if code == 0x06: return _text(c, m.INFO)
            if code == 0x09: return _break(c)
            if code == 0x07: return _shot(c, payload)
            if code == 0x08: _input(c, payload); return b''
            if code == 0x0A: return _screens(c)
            if code == 0x0D: return _uitree(c)
            if code == 0x0E: return _uiact(c, payload)
            if code == 0x0F: return _menus(c, payload.decode('latin-1'))
            if code == 0x12: return _rexx(c, payload.decode('latin-1'))
        except m.WasabiError as e:
            raise _err(e)
    raise Gap('legacy operation 0x%02x has no wasabi command' % code)


def read_file(path):
    """The file's bytes on the Amiga, None when it is not there. The daemon or
    the rig being gone is not 'not there': that SystemExit goes on."""
    try:
        return req(0x03, path.encode('latin-1'))
    except SystemExit as e:
        if str(e).startswith('ERR'): return None
        raise

def restore_file(path, saved):
    """Put read_file's answer back: the bytes, or the file gone when it was None."""
    if saved is None:
        req(0x02, struct.pack('>H', 30) + ('Delete >NIL: "%s" QUIET' % path).encode('latin-1'))
        return
    import tempfile
    parent = path.rsplit('/', 1)[0] if '/' in path else path.rsplit(':', 1)[0] + ':'
    req(0x02, struct.pack('>H', 30) + ('MakeDir >NIL: "%s"' % parent).encode('latin-1'))
    with tempfile.NamedTemporaryFile(delete=False) as t: t.write(saved)
    try: put(t.name, path)
    finally: os.unlink(t.name)

def key(code, qual=0):
    # Down and up in ONE KEY frame: two requests arrive hundreds of ms apart under host load and the Amiga
    # auto-repeats the 'held' key (garbled tcsh input, 2026-09-29).
    with _conn() as c:
        try: _keys(c, [(code, qual), (code | 0x80, qual)])
        except _m().WasabiError as e: raise _err(e)

def script(*events):
    # Each event is ('move', x, y) | ('button', b, down) | ('wait', ticks); see the Gaps note on the timing.
    body = b''
    for e in events:
        if e[0] == 'move': body += bytes([1]) + struct.pack('>HH', int(e[1]), int(e[2]))
        elif e[0] == 'button': body += bytes([2, e[1], e[2]])
        elif e[0] == 'wait': body += bytes([9]) + struct.pack('>H', e[1])
        else: raise ValueError(e)
    req(0x08, bytes([8, len(events)]) + body)

def pointer_scale():
    # Mouse units vs uitree pixels: wasabi's mouse takes grab space, whose rows are the screen's rows times the
    # screen's row_scale (2 on 640x256); the uitree here is in screen rows.
    with _conn() as c:
        scr = _m().list_windows(c)[0]
    return 1.0, float(scr[0]['row_scale']) if scr else 1.0

def click_px(x, y):
    # A left click at screen pixel (x, y) of the front screen (uitree's coordinates), scaled to mouse units.
    sx, sy = pointer_scale()
    req(0x08, bytes([5]) + struct.pack('>HH', int(round(x * sx)), int(round(y * sy))) + bytes([0, 1]))

def window(title):
    # The front screen's window titled exactly `title`, from the uitree:
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

def grab(box=None):
    # The front screen (or box = (x, y, w, h) of it) as (w, h, rows): each row w*3 bytes of RGB.
    b = req(0x07, struct.pack('>4H', *box) if box else b'')
    w, h = struct.unpack('>HH', b[2:6])
    px = b[8:]
    return w, h, [px[y * w * 3:(y + 1) * w * 3] for y in range(h)]

def put(local, remote):
    # a file onto the Amiga, streamed from disk (wasabi's PUT carries 32-bit sizes: no parts)
    with _conn(600) as c:
        try: _m().do_put(c, local, remote, quiet=True)
        except _m().WasabiError as e: raise _err(e)

def png(path, w, h, rows):
    _m().write_png(path, w, h, b''.join(rows))

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
        put(a[1], a[2])
    elif cmd == 'screens':
        b = req(0x0A); print(b.hex())
    elif cmd == 'shot':
        w, h, rows = grab(tuple(map(int, a[2:6])) if len(a) >= 6 else None)
        png(a[1], w, h, rows); print(a[1], w, h)
    elif cmd == 'key':
        # a raw key code (hex) and qualifier (hex): down, then up
        code, qual = int(a[1], 16), int(a[2], 16) if len(a) > 2 else 0
        key(code, qual)
    elif cmd == 'type': req(0x08, bytes([4]) + a[1].encode('latin-1'))
    elif cmd == 'gclick':
        # the gadget with id (or role, 'sys:close') a[2] in the window whose title contains a[1], clicked at its middle
        tree = req(0x0D).decode('latin-1').splitlines()
        win = None
        for line in tree:
            f = line.split()
            if line.startswith('W ') and a[1].lower() in line.lower():
                win = f[1]
            elif win is not None and line.startswith('W '):
                win = None
            elif win is not None and line.startswith('G ') and a[2].lower() in (f[1].lower(), f[5].lower()):
                x, y = int(f[2]), int(f[3]); w, h = map(int, f[4].split('x'))
                cx, cy = x + w // 2, y + h // 2
                click_px(cx, cy)
                print('click at %d,%d' % (cx, cy)); return
        raise SystemExit('no matching gadget')
    else: raise SystemExit(__doc__)

if __name__ == '__main__': main(sys.argv[1:])
