#!/usr/bin/env python3
"""slash_rig.py -- ledger C1 on the rig: UP-Term's /commands in an XCON:
window's line, and C:UPTerm from the shell (plan
thoughts/shared/plans/2026-10-03-slash-commands.md).

  1. "/cursor bar" typed at the prompt: Settings > Cursor > Bar is checked,
     and the shell is still there (the next command runs).
  2. "/" alone is still the parent directory: Cd RAM:sl/sub, "/", the
     shell's directory is RAM:sl.
  3. Tab: "/cursor-b" completes to "/cursor-blink "; "on" and Return check
     Settings > Cursor > Blinking.
  4. "/help" lists the commands into the window (UPTerm find, from the
     same shell, finds a help line in the scrollback; a made-up one it
     does not -- the words come through a shell variable, so the typed
     command line on screen cannot be what is found).
  5. C:UPTerm: "UPTerm cursor block" checks Block; "UPTerm cursor round" is
     refused with return code 10.
  6. A leading blank sends the line on as typed: " /cursor underline" leaves
     the cursor alone.

Run with the rig up and the handler installed (rig.py install + a reboot):
  python3 tools/rig/slash_rig.py
"""
import shutil, pathlib, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / 'build/rig/vtc'
RET, TAB = 0x44, 0x42
TITLE = 'slash'
passed = total = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + str(seen).strip()) if seen and not ok else ''),
          flush=True)


def t(s, enter=True, wait=1.5):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)
    if enter:
        ami.key(RET)
        time.sleep(wait)


def menu_state():
    """{(menu title, item, sub item): 'checked' | 'uncheck' | ...} for the
    active window, by the names shown (the menus' places move as menus come)"""
    out, titles, items = {}, {}, {}
    for l in ami.req(0x0F).decode('latin-1').splitlines():
        f = l.split()
        if l.startswith('M ') and len(f) > 2:
            titles[f[1]] = l.split('"')[-2]
        elif l.startswith('I ') and len(f) > 5:
            name = l.split('"')[-2]
            if f[3] == '-':
                items[(f[1], f[2])] = name
                out[(titles.get(f[1]), name, None)] = f[5]
            else:
                out[(titles.get(f[1]), items.get((f[1], f[2])), name)] = f[5]
    return out


CURSOR = {'block': ('Settings', 'Cursor', 'Block'), 'underline': ('Settings', 'Cursor', 'Underline'),
          'bar': ('Settings', 'Cursor', 'Bar'), 'blinking': ('Settings', 'Cursor', 'Blinking')}


def checked(name):
    return 'checked' in menu_state().get(CURSOR[name], '')


def rc_of(cmd):
    """the shell's return code after cmd, typed in the window"""
    c.run('Delete RAM:sl.rc QUIET')
    t(cmd)
    t('Echo >RAM:sl.rc $RC')
    return c.run('Type RAM:sl.rc')[1].strip()


def main():
    shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
    c.run('Delete RAM:sl ALL QUIET')
    c.run('MakeDir RAM:sl RAM:sl/sub')
    try:
        c.run('Run >NIL: NewShell "XCON:0/20/640/300/%s/CLOSE"' % TITLE)
        time.sleep(4)
        x, y, w, h = ami.window(TITLE)['box']
        kx, ky = ami.pointer_scale()
        ami.script(('move', int((x + 300) * kx), int((y + 150) * ky)), ('wait', 2), ('button', 0, 1),
                   ('wait', 2), ('button', 0, 0), ('wait', 5))
        time.sleep(1)

        # 1
        t('/cursor bar')
        check(checked('bar') and not checked('block'), '"/cursor bar": Settings > Cursor > Bar is checked',
              menu_state().get(CURSOR['bar']))
        c.run('Delete RAM:sl.out QUIET')
        t('Echo >RAM:sl.out alive')
        check(c.run('Type RAM:sl.out')[1].strip() == 'alive', 'the shell runs the next line')

        # 2
        t('Cd RAM:sl/sub')
        t('/')
        c.run('Delete RAM:sl.cd QUIET')
        t('Cd >RAM:sl.cd')
        cd = c.run('Type RAM:sl.cd')[1].strip()
        check(cd.lower().endswith(':sl'), '"/" alone is still the parent directory', cd)

        # 3
        t('/cursor-b', enter=False)
        ami.key(TAB)
        time.sleep(1)
        t('on')
        check(checked('blinking'), 'Tab completes "/cursor-b" to "/cursor-blink "; "on" checks Blinking',
              menu_state().get(CURSOR['blinking']))

        # 4
        # the searched words come through a shell variable: the typed line
        # on screen shows "$w", so only the help can hold the expanded text
        t('/help', wait=2)
        t('Set w bright')
        rc = rc_of('VTC:UPTerm find takes the $w colours')
        check(rc == '0', '"/help" listed the commands into the window (UPTerm find sees one)', rc)
        t('Set w zzqq')
        rc = rc_of('VTC:UPTerm find takes the $w colours')
        check(rc == '10', 'and a line that is not there is not found (return code 10)', rc)

        # 5
        rc = rc_of('VTC:UPTerm cursor block')
        check(rc == '0' and checked('block'), 'UPTerm cursor block: Block checked, return code 0',
              (rc, menu_state().get(CURSOR['block'])))
        rc = rc_of('VTC:UPTerm cursor round')
        check(rc == '10' and checked('block'), 'UPTerm cursor round: refused, return code 10', rc)

        # 6
        t(' /cursor underline')
        check(checked('block') and not checked('underline'), 'a leading blank sends the line on as typed',
              menu_state().get(CURSOR['underline']))
        t('EndCLI')
    finally:
        c.run('Delete RAM:sl RAM:sl.out RAM:sl.cd RAM:sl.rc ALL QUIET')
        (VTC / 'UPTerm').unlink(missing_ok=True)
    print('slash_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
