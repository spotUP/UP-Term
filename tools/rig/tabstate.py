"""tabstate.py -- the handler's counters (`UPTerm state`) read from an UP-Term window.

`UPTerm state` answers only to a process whose console is an UP-Term window ("not an UP-Term window" from
the wasabi exec shell), so the line is typed into the active tab and its output read from RAM:. Used by
keymap_rig.py and amarchy_rig.py."""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import ami, condev_rig as c, vshwin

RET = 0x44


def open_host(title='kmhost'):
    """A titled XCON: window running vsh, focused by a click."""
    vshwin.open_vsh(title, wait=5)
    ami.click_px(100, 100)
    time.sleep(1)


def parse(text):
    d = {}
    for l in text.splitlines():
        f = l.replace('=', ' ').replace(':', ' ').split()
        if len(f) >= 2 and f[-1].lstrip('-').isdigit():
            d[f[0]] = int(f[-1])
    return d


def counters():
    c.run('Delete RAM:cnt.out QUIET')
    # C:UPTerm by path: the rig's shell path has no UPTerm (vsh answers "UPTerm: not found"); it is staged in C:, VTC: has none
    ami.req(0x08, bytes([4]) + b'C:UPTerm state >RAM:cnt.out')
    time.sleep(0.5)
    ami.key(RET)
    time.sleep(2.5)
    return parse(c.run('Type RAM:cnt.out')[1])
