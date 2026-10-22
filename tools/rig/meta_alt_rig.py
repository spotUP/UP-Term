#!/usr/bin/env python3
"""meta_alt_rig.py -- Alt is Meta by default (plan 2026-10-22-amarchy-commodity.md, Phase A6, decision D2).

On the Swedish keymap, Left Alt + b and Left Alt + f reach the vsh line editor as M-b and M-f
(word back, word forward). Each check types a command, edits it with the Alt key, runs it and
reads the redirected output from RAM:.
  1. no meta line in the profile (the default, meta = alt): Alt+b moves one word back
  2. no meta line: Alt+f moves one word forward
  3. meta = amiga in the profile: Alt+b no longer edits (the line runs unedited)
Run with the rig up and the handler installed: python3 tools/rig/meta_alt_rig.py
NOT RUN when written (Phase A, no rig allowed in that session); run it before landing Phase B.
"""
import pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
import vshwin

VTC = paths.RIG / 'vtc'
OUT = 'RAM:meta.out'
LALT = 0x10                       # IEQUALIFIER_LALT
RAW_B, RAW_F, RAW_RETURN = 0x35, 0x23, 0x44
RAW_X = 0x32
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def typed(s):
    ami.req(0x08, bytes([4]) + s.encode())
    time.sleep(0.5)


def line_edit(keys):
    """open a window, type the redirected echo, press the Alt keys, add an x, run, return the output text"""
    c.run('Delete %s QUIET' % OUT)
    vshwin.open_vsh('meta', box='0/20/640/200', wait=6)    # the line editor under test is vsh's
    x, y, w, h = ami.window('meta')['box']
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((x + w // 2) * kx), int((y + h // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    typed('>%s echo one two' % OUT)
    for raw in keys:
        ami.key(raw, LALT)
        time.sleep(0.5)
    ami.key(RAW_X)
    time.sleep(0.5)
    ami.key(RAW_RETURN)
    time.sleep(2)
    typed('exit')
    ami.key(RAW_RETURN)
    time.sleep(3)
    return c.run('Type %s' % OUT)[1].strip()


def main():
    had = c.run('Copy ENV:up-term/up-term RAM:meta.bak QUIET')[0] == 0
    try:
        c.run('MakeDir ENV:up-term QUIET')
        c.run('Delete ENV:up-term/up-term QUIET')
        seen = line_edit([RAW_B])
        check(seen == 'one xtwo', 'default (meta = alt): Alt+b moves one word back (M-b)', seen)
        seen = line_edit([RAW_B, RAW_B, RAW_F])
        check(seen == 'onex two', 'default (meta = alt): Alt+f moves one word forward (M-f)', seen)
        (VTC / 'meta.conf').write_text('[profile default]\nmeta = amiga\n')
        c.run('Copy VTC:meta.conf ENV:up-term/up-term QUIET')
        seen = line_edit([RAW_B])
        check(seen == 'one twox', 'meta = amiga: Alt+b is not Meta, the x lands at the line end', seen)
    finally:
        if had:
            c.run('Copy RAM:meta.bak ENV:up-term/up-term QUIET')
        else:
            c.run('Delete ENV:up-term/up-term QUIET')
        c.run('Delete RAM:meta.bak RAM:meta.out QUIET')
        (VTC / 'meta.conf').unlink(missing_ok=True)
    print('meta_alt_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
