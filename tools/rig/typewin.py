"""typewin.py -- keys typed into an UP-Term window on the rig (the way claude_rig.py has always done it), and
the window's text read back through the clipboard: the helpers claude_rig.py and amigapi_rig.py share.

  open_xcon(title)   a titled XCON: window, focused by a click, its Shell at the prompt
  typ(s), key(code, qual)  characters / one key (down, up) into the active window
  screen_text(out)   the window's whole text: Right Amiga A (Select all), Right Amiga C (Copy), then
                     VTC:snipprobe CLIP <file> writes the clipboard's text, CLEAR sets it to "-" (make build/amiga/snipprobe),
                     then one click in the window drops the selection (render/vtwin.c VTI_SELECT_END: a
                     click clears it), so the window is left as it was
"""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import condev_rig as c

RET, ESC, TAB, BS = 0x44, 0x45, 0x42, 0x41
SHIFT, CTRL, ALT, LAMIGA, RAMIGA = 0x0001, 0x0008, 0x0010, 0x0040, 0x0080
# UP-Term's Meta key (the ESC prefix a program reads as Alt) is Left Amiga unless Settings > Meta key says Alt


_checked_at = 0.0


def require_active(force=False):
    """Keys go to whatever window is active: make the one open_xcon opened the active one, or stop (a stray
    window must never swallow the keys of a script; 2026-10-12). A window that is gone (its program ended and
    closed it, or changed its title) is not checked."""
    global _checked_at
    if not _last_title or time.time() - _checked_at < 3 and not force:
        return      # a check costs a UITREE round trip: keys typed in quick succession (steering a working agent) must not wait for it
    _checked_at = time.time()
    w = ami.window(_last_title)
    if w and not w['active']:
        click_in(_last_title)
        time.sleep(1)
        w = ami.window(_last_title)
        if w and not w['active']:
            raise SystemExit('[ERROR] window %r is not the active window and a click did not make it so '
                             '(another window is in front: close it)' % _last_title)


def typ(s):
    require_active()
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.6)


def key(k, q=0, wait=0.7):
    require_active()
    ami.key(k, q)
    time.sleep(wait)


_last_title = None   # the window open_xcon opened last: where screen_text clicks


def click_in(title):
    """One left click inside the window titled `title` (the selection goes, a program sees no key)."""
    w = ami.window(title)
    if not w:
        return False
    kx, ky = ami.pointer_scale()
    x, y, ww, hh = w['box']
    # inside the window, whatever its size: a click outside a small one (the 24x7 OSD menu) lands on another
    # window and CLOSEINACTIVE closes it
    ami.script(('move', int((x + min(300, ww // 2)) * kx), int((y + min(150, hh // 2)) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    return True


def open_xcon(title, box='0/12/760/440'):
    global _last_title
    _last_title = title
    c.run('Run >NIL: NewShell "XCON:%s/%s/CLOSE"' % (box, title))
    time.sleep(5)
    w = ami.window(title)
    if not w:
        raise SystemExit('window %s never appeared' % title)
    click_in(title)
    time.sleep(1)
    return w


def _clip(out):
    c.run('Delete %s QUIET' % out)
    c.run('VTC:snipprobe CLIP %s' % out)
    return c.run('Type %s' % out)[1]


def screen_text(out='RAM:screen.txt'):
    """The active window's text (the screen as the program drew it), rows joined by newlines.

    A selection ends where output overwrites it (UT5, as in xterm), so on a screen that keeps changing (a
    spinner) the Copy right after Select all can find nothing. The call first writes the sentinel "-" to the
    clipboard (VTC:snipprobe CLEAR), then sends Select all + Copy back to back: a read that is empty (a Copy of an
    empty selection) or "-" (no Copy happened) means the Copy found nothing, so the clipboard cannot hold an older
    text, and the pair is repeated, up to 8 tries."""
    require_active(True)
    new = ''
    c.run('VTC:snipprobe CLEAR')
    for _ in range(8):
        key(0x20, RAMIGA, wait=0.0)     # Select all, Copy at once: a spinner's next frame ends the selection (UT5)
        key(0x33, RAMIGA, wait=1.0)     # Copy
        new = _clip(out)
        if new.strip() not in ('', '-'):
            break
    if _last_title:
        click_in(_last_title)   # a click clears the selection (UT3): the next frame shows no highlight
        time.sleep(1)
        assert_no_selection(_last_title)
    return new


SELECTION_RGB = bytes((0xAA, 0xAA, 0xAA))


def assert_no_selection(title):
    """No pixel row of the window is mostly the selection colour (#aaa): a highlight that stays after the click
    is a failure (the owner saw grey stay, 2026-10-12). Text in a grey is a few pixels of a row, not half of it."""
    w = ami.window(title)
    if not w:
        return
    x, y, ww, hh = w['box']
    _, _, rows = ami.grab((x + 4, y + 14, max(ww - 8, 8), max(hh - 18, 8)))
    for n, r in enumerate(rows):
        px = [r[i:i + 3] for i in range(0, len(r), 3)]
        if px and sum(1 for p in px if p == SELECTION_RGB) * 2 > len(px):
            raise SystemExit('[ERROR] window %r: pixel row %d is mostly the selection colour #aaaaaa after the clearing '
                             'click: the selection did not go' % (title, n))
