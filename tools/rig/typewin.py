"""typewin.py -- keys typed into an UP-Term window on the rig (the way claude_rig.py has always done it), and
the window's text read back through the clipboard: the helpers claude_rig.py and amigapi_rig.py share.

  open_xcon(title)   a titled XCON: window, focused by a click, its Shell at the prompt
  typ(s), key(code, qual)  characters / one key (down, up) into the active window
  screen_text(title) the window's whole text: Right Amiga A (Select all), Right Amiga C (Copy), then
                     VTC:snipprobe CLIP <file> writes the clipboard's text (make build/amiga/snipprobe)
"""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import condev_rig as c

RET, ESC, TAB, BS = 0x44, 0x45, 0x42, 0x41
SHIFT, CTRL, ALT, LAMIGA, RAMIGA = 0x0001, 0x0008, 0x0010, 0x0040, 0x0080
# UP-Term's Meta key (the ESC prefix a program reads as Alt) is Left Amiga unless Settings > Meta key says Alt


def typ(s):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.6)


def key(k, q=0, wait=0.7):
    ami.key(k, q)
    time.sleep(wait)


def open_xcon(title, box='0/12/760/440'):
    c.run('Run >NIL: NewShell "XCON:%s/%s/CLOSE"' % (box, title))
    time.sleep(5)
    w = ami.window(title)
    if not w:
        raise SystemExit('window %s never appeared' % title)
    kx, ky = ami.pointer_scale()
    x, y, ww, hh = w['box']
    ami.script(('move', int((x + 300) * kx), int((y + 150) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    return w


def screen_text(out='RAM:screen.txt'):
    """The active window's text (the screen as the program drew it), rows joined by newlines."""
    c.run('Delete %s QUIET' % out)
    key(0x20, RAMIGA, wait=1)    # Select all
    key(0x33, RAMIGA, wait=1.5)  # Copy
    c.run('VTC:snipprobe CLIP %s' % out)
    return c.run('Type %s' % out)[1]
