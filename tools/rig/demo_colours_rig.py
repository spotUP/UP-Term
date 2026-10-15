#!/usr/bin/env python3
"""demo_colours_rig.py -- W-T11 on the rig: the colours of UPDemo's scroll-region scenes, read back as pixels.

UPDemo's "scroll regions" (scene 6) and "tmux" (scene 7) scenes (350 and 450 ticks, each alone) print their text in xterm palette colours
(SGR 38;5;n: 75 111 147 183 219 for the verbs, 77 and 214 for the result, 240 and 244 for the dim parts). On a 24-bit
Workbench whose colour map is used up every window drew in the nearest of the few pens left (colour 240 and
48;2;88;88;88 purple). Each scene runs in a titled XCON: window; after a few seconds the window's pixels are read
and every palette colour the scene uses must be there, exactly, on 20 or more pixels. Self-checking, text only.

  UPTERM_RIG=<n> python3 -I tools/rig/demo_colours_rig.py [--hog] [scene ...]

--hog first runs VTC:rgbprobe HOG, which takes every free pen of the Workbench screen and keeps them until the rig
boots again (the state of rig 2 on 2026-10-14). Needs the rig up on the RTG screen (`rig.py rtg`), handler and
UPDemo installed (`make build/amiga/vtcon-handler build/amiga/UPDemo`, copied to VTC:), `make build/amiga/rgbprobe`."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, paths, typewin
import condev_rig as c

LEVELS = (0, 95, 135, 175, 215, 255)
# scene number, name, the palette indexes its text uses (demo/updemo.c sc_region, sc_tmux)
SCENES = {6: ('scroll regions', (75, 111, 147, 183, 219, 77, 214, 240, 244)),
          7: ("tmux", (111, 183, 34))}
failed = 0


def palette(i):
    """xterm 256 colour i >= 16 as bytes"""
    if i >= 232:
        v = 8 + 10 * (i - 232)
        return bytes((v, v, v))
    i -= 16
    return bytes((LEVELS[i // 36], LEVELS[i // 6 % 6], LEVELS[i % 6]))


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok  ' if ok else 'FAIL', what, '' if ok else ': ' + str(seen).strip()[-300:]), flush=True)
    return ok


def demo_title(prefix='UP-Term demo'):
    """UPDemo retitles its window ("UP-Term demo", "UP-Term demo: 92 x 30", ...): the first window of that name"""
    import shlex
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W '):
            t = shlex.split(line)[-1]
            if t.startswith(prefix) and ': a notification' not in t:
                return t
    return None


def pixels(title):
    x, y, w, h = ami.window(title)['box']
    _, _, rows = ami.grab((x + 4, y + 14, max(w - 8, 8), max(h - 18, 8)))
    n = {}
    for r in rows:
        for i in range(0, len(r) - 2, 3):
            p = bytes(r[i:i + 3])
            n[p] = n.get(p, 0) + 1
    return n


def until_closed(secs=40):
    end = time.time() + secs
    while demo_title() and time.time() < end:
        time.sleep(1)
    return demo_title() is None


def scene(num, secs):
    name, used = SCENES[num]
    title = 'wt11-%d' % num
    if demo_title():
        check(False, '%s: no UPDemo window open before the scene starts (left over: %r)' % (name, demo_title()))
        return
    script = 'wt11-%d.script' % num
    (paths.RIG / 'vtc' / script).write_text('VTC:UPDemo FULL %d\nEndCLI\n' % num)
    c.run('Run >NIL: NewShell "XCON:0/12/760/500/%s/CLOSE" FROM VTC:%s' % (title, script))
    end = time.time() + 60
    while demo_title() is None and time.time() < end:
        time.sleep(0.5)
    if not check(demo_title() is not None, '%s: the window opens' % name):
        return
    time.sleep(secs)
    title = demo_title()
    n = pixels(title) if title else {}
    want = [palette(i) for i in used]
    got = [(i, n.get(palette(i), 0)) for i in used]
    short = ['%d=#%s:%d' % (i, palette(i).hex(), k) for i, k in got if k < 20]
    check(not short, '%s: every palette colour of the scene is on 20 or more pixels (%d of %d)' %
          (name, sum(k >= 20 for _, k in got), len(got)), ' '.join(short))
    other = sorted(((k, p.hex()) for p, k in n.items() if k >= 20 and p not in want and len(set(p)) > 1),
                   reverse=True)[:8]
    print('info %s: other coloured pixels (20 or more): %s' % (name, ' '.join('#%s:%d' % (h, k) for k, h in other)),
          flush=True)
    if title:
        typewin._last_title = title
        typewin.click_in(title)
        typewin.key(0x45, wait=3)  # Esc ends UPDemo; its window closes with it
    check(until_closed(), '%s: Esc ends UPDemo and its window closes' % name)


def main(args):
    if '--hog' in args:
        c.run('Run >NIL: VTC:rgbprobe HOG')
        time.sleep(8)
        print('info penprobe: %s' % c.run('VTC:penprobe')[1].strip(), flush=True)
    for a in args:
        if a.isdigit():
            scene(int(a), 3)
    if not any(a.isdigit() for a in args):
        for num in SCENES:
            scene(num, 3)
    print('demo_colours_rig: %s' % ('FAIL' if failed else 'PASS'))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main(sys.argv[1:])
