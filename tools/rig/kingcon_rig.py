"""H8 on the rig: completion = kingcon in an XCON: window behaves as the
owner's KingCON does (thoughts/shared/research/2026-10-02_kingcon-completion.md),
and completion = unix (no key) is unchanged.

Each case types `Echo >RAM:kc.out <word>`, presses the keys, runs the line
and reads RAM:kc.out: what the completion put on the line is what Echo
wrote. The selection window is checked in the UI tree. The profile lives in
ENV:up-term/up-term only (ENVARC: is never touched) and is removed at the end.

Run with the rig up and the handler installed (rig.py install + a reboot):
  python3 tools/rig/kingcon_rig.py
"""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c

TAB, RET, ESC = 0x42, 0x44, 0x45
SHIFT, LALT = 0x0001, 0x0010
passed = total = 0
serial = 0


def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + seen.strip()) if seen and not ok else ''))


def windows():
    return [l for l in ami.req(0x0D).decode('latin-1').splitlines() if l.startswith('W ')]


def has_window(title, wait=8.0):
    """Is the window up -- waiting for it: the first command scan of C:
    and the path takes seconds (later ones come from the cache)."""
    end = time.time() + wait
    while True:
        if any(l.endswith('"%s"' % title) for l in windows()):
            return True
        if time.time() >= end:
            return False
        time.sleep(0.5)


def t(s):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.5)


def k(code, q=0):
    ami.key(code, q)
    time.sleep(0.7)


# The Complete menu, on the Workbench screen's bar (topaz 8). amiagent's MENUS
# lists the strip (titles and items in order, separators as empty disabled
# rows) but no boxes, so the places are derived from that live listing, never
# hardcoded: titles sit side by side, 8 px per character, so the pointer x is
# the characters of the titles before Complete plus TITLE_INSET into the title
# (the sweep 2026-10-06 hit from x=192 to 234 for a start of 184: 29 is the
# middle of the span); items are ITEM_PITCH rows below the first row centre,
# a separator ITEM_SEP.
CHAR_W, TITLE_INSET = 8, 29
FIRST_ITEM_Y, ITEM_PITCH, ITEM_SEP, BAR_Y = 20, 12, 6, 5
MENU_TITLE = 'Complete'


def menu_place(item):
    """(x, y) in screen pixels of `item` in the MENU_TITLE menu, from MENUS."""
    x = None
    titles, rows = 0, []
    cur = None
    for l in ami.req(0x0F).decode('latin-1').splitlines():
        f = l.split('"')
        if l.startswith('M '):
            cur = f[1]
            if cur == MENU_TITLE:
                x = titles + TITLE_INSET
            elif x is None:
                titles += len(cur) * CHAR_W
        elif l.startswith('I ') and cur == MENU_TITLE:
            rows.append(f[-2])
    if x is None or item not in rows:
        raise SystemExit('menu %s > %s not in the strip' % (MENU_TITLE, item))
    y = FIRST_ITEM_Y
    for name in rows[:rows.index(item)]:
        y += ITEM_SEP if name == '' else ITEM_PITCH
    return x, y


def menu(item):
    kx, ky = ami.pointer_scale()
    x, y = menu_place(item)
    ami.script(('move', int(x * kx), int(BAR_Y * ky)), ('wait', 2), ('button', 1, 1), ('wait', 8),
               ('move', int(x * kx), int(y * ky)), ('wait', 5), ('button', 1, 0),
               ('wait', 5))
    time.sleep(1)


def menu_state():
    """{item: 'checked' | 'uncheck' | '-'} for the active window's menus."""
    out = {}
    for l in ami.req(0x0F).decode('latin-1').splitlines():
        if l.startswith('I '):
            f = l.split('"')
            out[f[-2]] = l.split()[5]
    return out


def session(steps, profile):
    """A fresh XCON: window with its own title; steps are text or
    (key, qualifier) or ('look', fn). Returns what Echo wrote."""
    global serial
    serial += 1
    title = 'kc%d' % serial
    if profile:
        c.run('MakeDir >NIL: ENV:up-term')
        c.run('Echo >ENV:up-term/up-term "[profile default]*Ncompletion = %s"' % profile)
    else:
        c.run('Delete >NIL: ENV:up-term/up-term QUIET')
    c.run('Delete >NIL: RAM:kc.out QUIET')
    c.run('Run >NIL: NewShell "XCON:0/20/640/300/%s/CLOSE"' % title)
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    w = ami.window(title)
    x, y, ww, hh = w['box']
    ami.script(('move', int((x + 300) * kx), int((y + 150) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    for st in steps:
        if isinstance(st, str):
            t(st)
        elif st[0] == 'look':
            st[1]()
        elif st[0] == 'menu':
            menu(st[1])
        else:
            k(*st)
    time.sleep(1.5)
    out = c.run('Type RAM:kc.out')[1] if c.run('List >NIL: RAM:kc.out')[0] == 0 else ''
    t('endcli')
    k(RET)
    time.sleep(1)
    return out.strip()


def main():
    first_s = sorted([n for n in c.run('List S: FILES LFORMAT %n')[1].split('\n')
                      if n and not n.lower().endswith('.info')], key=str.lower)

    # kingcon: one match, a name and a space
    out = session(['Echo >RAM:kc.out S:Shell-Sta', (TAB,), (RET,)], 'kingcon')
    check(out == 'S:Shell-Startup', 'kingcon Tab: one match goes in whole', out)
    # a directory: "/" and no space
    out = session(['Echo >RAM:kc.out SYS:Pre', (TAB,), 'x', (RET,)], 'kingcon')
    check(out == 'SYS:Prefs/x', 'kingcon Tab: a directory gets "/" and no space', out)
    # several: the window; Tab Tab Shift+Tab moves to the second; Return takes it
    seen = {}
    out = session(['Echo >RAM:kc.out S:', (TAB,),
                   ('look', lambda: seen.update(win=has_window('Select filename'))),
                   (TAB,), (TAB,), (TAB, SHIFT), (RET,),
                   ('look', lambda: seen.update(gone=not has_window('Select filename', 0))), (RET,)],
                  'kingcon')
    check(seen.get('win'), 'kingcon Tab: several matches open "Select filename"')
    check(seen.get('gone'), 'Return in the list closes it')
    check(len(first_s) > 1 and out == 'S:' + first_s[1],
          'Tab, Tab, Shift+Tab, Return takes the second name (sorted)', '%r vs %r' % (out, first_s[:2]))
    # Escape: the line as it was
    out = session(['Echo >RAM:kc.out S:', (TAB,), (ESC,), (RET,)], 'kingcon')
    check(out == 'S:', 'Escape in the list leaves the line as it was', out)
    # Shift+Tab: devices, volumes and assigns
    seen = {}
    out = session(['Echo >RAM:kc.out Sy', (TAB, SHIFT),
                   ('look', lambda: seen.update(win=has_window('Select device'))), (RET,), (RET,)],
                  'kingcon')
    check(seen.get('win'), 'kingcon Shift+Tab: "Select device"')
    check(out == 'System:', 'the volume lists before the assign (System: before SYS:)', out)
    # Alt+Tab: commands
    seen = {}
    session(['Lo', (TAB, LALT), ('look', lambda: seen.update(win=has_window('Select command'))),
             (ESC,), ('look', lambda: seen.update(gone=not has_window('Select command', 0))),
             (0x41,), (0x41,)], 'kingcon')
    check(seen.get('win'), 'kingcon Alt+Tab: "Select command"')
    check(seen.get('gone'), 'Escape closes it')
    # quoting: a name with a space
    c.run('MakeDir >NIL: "RAM:kc dir"')
    c.run('Echo >"RAM:kc dir/my file" x')
    out = session(['Echo >RAM:kc.out RAM:kc', (TAB,), 'my', (TAB,), (RET,)], 'kingcon')
    check(out == 'RAM:kc dir/my file', 'kingcon quotes a name with a space (and closes the quote)', out)
    c.run('Delete >NIL: "RAM:kc dir" ALL QUIET')

    # unix (no key): unchanged -- one match the same, several: no window
    out = session(['Echo >RAM:kc.out S:Shell-Sta', (TAB,), (RET,)], None)
    check(out == 'S:Shell-Startup', 'unix Tab: one match goes in whole', out)
    seen = {}
    session(['Echo >RAM:kc.out S:', (TAB,),
             ('look', lambda: seen.update(win=has_window('Select filename', 3))), (RET,)], None)
    check(not seen.get('win'), 'unix Tab: no selection window')

    # KingCON's other styles (kingcon-mode = FNCMODE letters)
    hip = [n for n in first_s if n.lower().startswith('hip')]
    B = 'kingcon*Nkingcon-mode = B'
    out = session(['Echo >RAM:kc.out S:', (TAB,), (TAB,), (TAB,), (TAB, SHIFT), (RET,)], B)
    check(out == 'S:' + first_s[1], 'B: Tab cycles inline (Tab, Tab, Tab, Shift+Tab = the second)',
          '%r vs %r' % (out, first_s[:2]))
    seen = {}
    session(['Echo >RAM:kc.out S:', (TAB,), '\x13',
             ('look', lambda: seen.update(win=has_window('Select filename'))), (ESC,), (RET,)], B)
    check(seen.get('win'), 'B: Ctrl+S during a cycle opens the window')
    if len(hip) > 1:
        common = hip[0]
        for h in hip[1:]:
            i = 0
            while i < len(common) and i < len(h) and common[i].lower() == h[i].lower():
                i += 1
            common = common[:i]
        out = session(['Echo >RAM:kc.out S:Hip', (TAB,), (RET,)], 'kingcon*Nkingcon-mode = C')
        check(out == 'S:' + common, 'C: the part all names share first', '%r vs %r' % (out, common))
        out = session(['Echo >RAM:kc.out S:Hip', (TAB,), (TAB,), (RET,)], 'kingcon*Nkingcon-mode = CB')
        check(out == 'S:' + hip[0], 'CB: shared part, then the cycle from the first', '%r vs %r' % (out, hip[0]))
        seen = {}
        session(['Echo >RAM:kc.out S:Hip', (TAB,), (TAB,),
                 ('look', lambda: seen.update(win=has_window('Select filename'))), (ESC,), (RET,)],
                'kingcon*Nkingcon-mode = CW')
        check(seen.get('win'), 'CW: shared part, then the window on the next Tab')
        out = session(['Echo >RAM:kc.out S:Hip', (TAB,), (RET,)], 'kingcon*Nkingcon-mode = L')
        check(out == 'S:Hip', 'L: the list printed, the line left as it was', out)
    # .info files: hidden by default, kingcon-info = show lists them
    out = session(['Echo >RAM:kc.out SYS:Prefs.in', (TAB,), (RET,)], 'kingcon')
    check(out == 'SYS:Prefs.in', 'kingcon hides .info files', out)
    out = session(['Echo >RAM:kc.out SYS:Prefs.in', (TAB,), (RET,)], 'kingcon*Nkingcon-info = show')
    check(out == 'SYS:Prefs.info', 'kingcon-info = show lists them', out)
    # Tab on an empty word: the ASL file requester, in the current directory
    # (the Shell's: BOOTX: on the rig); the File field has the cursor
    seen = {}
    out = session(['Echo >RAM:kc.out ', (TAB,),
                   ('look', lambda: seen.update(win=has_window('Select filename'))),
                   'boot.log', (RET,), (RET,)], 'kingcon')
    check(seen.get('win'), 'W: Tab on an empty word opens the file requester')
    check(out == 'BOOTX:boot.log', 'the chosen file goes in with its drawer', out)

    # KingCON's Complete menu (kingcon only): the keys' completions, the
    # cache switches and Show .info, for this window
    seen = {}
    out = session([('look', lambda: seen.update(m=menu_state())),
                   'Echo >RAM:kc.out S:Shell-Sta', ('menu', 'Filename'), (RET,)], 'kingcon')
    m = seen.get('m', {})
    check(m.get('Enable cache') == 'checked' and m.get('Show .info') == 'uncheck' and 'Filename' in m,
          'kingcon: the Complete menu, cache on, .info off', str(m))
    check(out == 'S:Shell-Startup', 'Complete > Filename completes as Tab does', out)
    seen = {}
    session(['Lo', ('menu', 'Command'), ('look', lambda: seen.update(win=has_window('Select command'))),
             (ESC,), (0x41,), (0x41,)], 'kingcon')
    check(seen.get('win'), 'Complete > Command opens "Select command"')
    seen = {}
    session(['Sy', ('menu', 'Device'), ('look', lambda: seen.update(win=has_window('Select device'))),
             (ESC,), (0x41,), (0x41,)], 'kingcon')
    check(seen.get('win'), 'Complete > Device opens "Select device"')
    seen = {}
    out = session([('menu', 'Show .info'), ('look', lambda: seen.update(m=menu_state())),
                   'Echo >RAM:kc.out SYS:Prefs.in', (TAB,), (RET,)], 'kingcon')
    check(seen.get('m', {}).get('Show .info') == 'checked', 'Show .info checks', str(seen.get('m')))
    check(out == 'SYS:Prefs.info', 'and the window lists .info files from then on', out)
    seen = {}
    session([('menu', 'Reset cache'), ('menu', 'Purge cache'), 'Lo', (TAB, LALT),
             ('look', lambda: seen.update(win=has_window('Select command'))), (ESC,), (0x41,), (0x41,)],
            'kingcon')
    check(seen.get('win'), 'after Reset cache and Purge cache the commands are found again')
    seen = {}
    session([('menu', 'Enable cache'), ('look', lambda: seen.update(m=menu_state())), 'Lo', (TAB, LALT),
             ('look', lambda: seen.update(win=has_window('Select command'))), (ESC,), (0x41,), (0x41,)],
            'kingcon')
    check(seen.get('m', {}).get('Enable cache') == 'uncheck' and seen.get('win'),
          'Enable cache off: unchecked, commands still found (read afresh)', str(seen.get('m')))
    seen = {}
    session([('look', lambda: seen.update(m=menu_state()))], 'kingcon*Nkingcon-cache = off')
    check(seen.get('m', {}).get('Enable cache') == 'uncheck', 'kingcon-cache = off: the menu starts unchecked')
    seen = {}
    session([('look', lambda: seen.update(m=menu_state()))], None)
    check('Filename' not in seen.get('m', {'Filename': 1}), 'unix: no Complete menu')

    c.run('Delete >NIL: ENV:up-term/up-term RAM:kc.out QUIET')
    print('kingcon_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
