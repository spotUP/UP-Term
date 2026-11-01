"""FP-3 rig (R1.9): an unconfirmed desktop theme apply does not survive a reboot.
UPTERM_RIG=<n> python3 tools/rig/amarchy_marker_rig.py <a|b> <state.json>
  a: record the md5 of every ENV:Sys / ENVARC:Sys file, desktop-theme on + flat-border-size thin (the ENVARC: icontrol write is the one that
     outlives a reboot), pick a theme and do NOT click Keep, check ENVARC:Amarchy/pending-revert exists, then stop. Reboot at once
     (UPTERM_RIG=<n> python3 tools/rig/rig.py reboot, after 5 s quiet) while the 15 s requester is still open.
  b: after the reboot and the Amarchy start: AMARCHY_DESK_MARKER_REVERTS == 1, marker and marker dir gone, ENV:Sys + ENVARC:Sys md5 equal the recorded ones."""
import json, os, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import amarchy_desk_rig as d
import ami, condev_rig as c, amarchy_theme_save_rig as t, amarchy_gui_rig as g

FILES = ['palette.prefs', 'wbpattern.prefs', 'pointer.prefs', 'font.prefs', 'Workbench.prefs', 'icontrol.prefs']


def md5(p):
    out = c.run('VTCX:Apps/bin/md5sum %s' % p, 30)[1].strip()
    return out[:32] if len(out) >= 32 and ' ' in out[:40] and 'rror' not in out else 'absent'


def snap():
    return {w + ':' + f: md5(w + ':Sys/' + f) for w in ('ENV', 'ENVARC') for f in FILES}


def exists(p):
    return 'rror' not in c.run('List %s QUICK' % p, 30)[1] and bool(c.run('List %s QUICK' % p, 30)[1].strip())


def main():
    ph, st = sys.argv[1], sys.argv[2]
    S = json.load(open(st)) if os.path.exists(st) else {}
    if ph == 'a':
        c.run('MakeDir >NIL: ENV:Amarchy')
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b'; Amarchy preferences.\n[profile default]\ndesktop-theme = on\nflat-system-look = on\nflat-window-gadgets = on\nflat-border-size = thin\n')
        d.put_prefs(f.name)
        os.unlink(f.name)
        d.pick(keep=True)       # settle: reach the thin state once and keep it, so the apply below changes the desktop again
        time.sleep(3)
        S['pre'] = snap()
        print('[INFO] pre-apply', S['pre'])
        d.pick(keep=False)      # the apply under test: no Keep
        t0 = time.time()
        d.say(exists('ENVARC:Amarchy/pending-revert'), 'a: marker ENVARC:Amarchy/pending-revert written at confirm start')
        S['applied'] = snap()
        print('[INFO] applied', S['applied'], 'seconds since pick returned', round(time.time() - t0, 1))
        d.say(S['applied'] != S['pre'], 'a: the apply changed ENV:Sys or ENVARC:Sys (else the test proves nothing)')
        json.dump(S, open(st, 'w'))
        print('[INFO] now (within the 15 s) wait 5 s quiet, then rig.py reboot; failed', d.failed)
    else:
        for _ in range(60):
            if g.process_running('Amarchy'):
                break
            time.sleep(2)
        time.sleep(10)
        r = int(t.env('AMARCHY_DESK_MARKER_REVERTS') or 0)
        d.say(r == 1, 'b: AMARCHY_DESK_MARKER_REVERTS == 1', str(r))
        d.say(not exists('ENVARC:Amarchy/pending-revert'), 'b: marker gone')
        d.say(not exists('ENVARC:Amarchy/pending-revert.d'), 'b: marker dir gone')
        now = snap()
        for k, v in S['pre'].items():
            d.say(now[k] == v, 'b: %s equals the pre-apply md5' % k, '%s vs %s' % (now[k], v))
    print('failed', d.failed)


main()
