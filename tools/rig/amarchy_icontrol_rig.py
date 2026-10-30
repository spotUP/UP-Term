#!/usr/bin/env python3
"""FLAT-P1-2 rig: flat-border-size thin through the real Amarchy apply path on AmigaOS 3.2 (3.1 is deferred to X1).
  UPTERM_RIG=<n> tools/rig/rig.py start --max --os 32 --purpose "<session>: icontrol thin border"
  UPTERM_RIG=<n> python3 tools/rig/amarchy_gui_rig.py --setup
  UPTERM_RIG=<n> python3 tools/rig/amarchy_icontrol_rig.py
Steps: save the stock ENVARC:Sys/icontrol.prefs once (SYS:Prefs/IControl, Save) when the rig has none; measure BorderTop of a
fresh IControl window (build/amiga/winprobe); desktop-theme = on + flat-border-size = thin in ENV:Amarchy/prefs; pick a theme
(amarchy_desk_rig.pick, the drawn menu); check the file geometry bytes and the BorderTop of a new window; restore the snapshot
T:amarchy-revert/ENVARC-Sys-icontrol.prefs (stock writes nothing: no revert action exists yet) and compare the md5 with the start."""
import hashlib, os, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ami, condev_rig as c
import amarchy_tile_rig as tile
import amarchy_desk_rig as d

ARC = 'ENVARC:Sys/icontrol.prefs'
ENVF = 'ENV:Sys/icontrol.prefs'
failed = 0


def md5(b):
    return hashlib.md5(b).hexdigest()


def btop(title):
    """BorderTop of the first window whose title starts with `title`, after a fresh open of the IControl editor."""
    rc, out = c.run('Run >NIL: SYS:Prefs/IControl')
    time.sleep(4)
    rc, out = c.run('RAM:winprobe')
    for ln in out.splitlines():
        if ln.startswith('W "%s' % title):
            return int(ln.split('btop=')[1].split()[0]), ln
    return None, out


def close_editor():
    """Cancel gadget of the editor window (uitree id 9), so no window stays open between measurements."""
    subprocess.run([sys.executable, os.path.join(HERE, 'ami.py'), 'gclick', 'IControl Preferences', '9'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def bar_back(what, secs=40):
    """The apply closes the bar around the screen reset and opens it again: wait for bar=1 and the window."""
    t0 = time.time()
    while time.time() - t0 < secs:
        rc, st = tile.send('AMARCHY', 'BAR STATUS')
        if rc == 0 and st.startswith('bar=1') and tile.bar_box() is not None:
            d.say(True, '%s: the bar is back on screen' % what, '%.0f s' % (time.time() - t0))
            return
        time.sleep(2)
    d.say(False, '%s: the bar is back on screen' % what, 'still %r after %d s' % (st, secs))


def main():
    global failed
    ami.put(os.path.join(HERE, '..', '..', 'build', 'amiga', 'winprobe'), 'RAM:winprobe')
    c.run('Protect RAM:winprobe +e')
    arc0 = ami.read_file(ARC)
    if arc0 is None:   # a fresh rig has none: Save from the stock editor (gadget 2) writes the stock file
        c.run('Run >NIL: SYS:Prefs/IControl')
        time.sleep(4)
        subprocess.run([sys.executable, os.path.join(HERE, 'ami.py'), 'gclick', 'IControl Preferences', '2'],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(3)
        arc0 = ami.read_file(ARC)
        if arc0 is None:
            sys.exit('[FAIL] could not save the stock %s from SYS:Prefs/IControl' % ARC)
    m0 = md5(arc0)
    print('[INFO] start md5 %s (%d bytes), geometry %s' % (m0, len(arc0), arc0[68:72].hex()))
    b0, ln = btop('IControl')
    print('[INFO] stock BorderTop: %s  %s' % (b0, ln.strip()))
    close_editor()
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(b'[profile default]\ndesktop-theme = on\nflat-border-size = thin\n')
    d.put_prefs(f.name)
    os.unlink(f.name)
    name, _ = d.pick()
    print('[INFO] picked %s' % name)
    bar_back('thin')
    arc1 = ami.read_file(ARC) or b''
    env1 = ami.read_file(ENVF) or b''
    print('[INFO] after thin: ARC geometry %s ENV geometry %s' % (arc1[68:72].hex(), env1[68:72].hex()))
    d.say(arc1 != arc0 and arc1 == env1, 'thin: ENVARC and ENV copies written, equal', 'md5 %s / %s' % (md5(arc1), md5(env1)))
    b1, ln = btop('IControl')
    print('[INFO] thin BorderTop: %s  %s' % (b1, ln.strip()))
    close_editor()
    d.say(b0 is not None and b1 is not None and b1 < b0, 'thin BorderTop below stock', 'stock %s thin %s' % (b0, b1))
    # back to stock through the real apply: flat-border-size = stock, pick another theme; the apply restores the snapshot
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(b'[profile default]\ndesktop-theme = on\nflat-border-size = stock\n')
    d.put_prefs(f.name)
    os.unlink(f.name)
    name, _ = d.pick()
    print('[INFO] picked %s (stock border)' % name)
    bar_back('stock')
    arc2 = ami.read_file(ARC) or b''
    env2 = ami.read_file(ENVF) or b''
    d.say(md5(arc2) == m0, 'stock: ENVARC md5 equals the start', '%s vs %s' % (md5(arc2), m0))
    d.say(md5(env2) == m0, 'stock: ENV md5 equals the start', '%s vs %s' % (md5(env2), m0))
    b2, ln = btop('IControl')
    print('[INFO] stock-again BorderTop: %s  %s' % (b2, ln.strip()))
    close_editor()
    d.say(b2 == b0, 'BorderTop back to stock', 'stock %s again %s' % (b0, b2))
    sys.exit(1 if d.failed else 0)


if __name__ == '__main__':
    main()
