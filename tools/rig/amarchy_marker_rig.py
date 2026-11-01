"""FP-3 rig (R1.9): an unconfirmed desktop theme apply does not survive a reboot.
UPTERM_RIG=<n> python3 tools/rig/amarchy_marker_rig.py <a|b> <state.json>
  a: record the md5 of every ENV:Sys / ENVARC:Sys file, desktop-theme on + flat-border-size thin (the ENVARC: icontrol write is the one that
     outlives a reboot), pick a theme and do NOT click Keep, check ENVARC:Amarchy/pending-revert exists, then stop. Reboot at once
     (UPTERM_RIG=<n> python3 tools/rig/rig.py reboot, after 5 s quiet) while the 15 s requester is still open.
  Before a: `UPTERM_RIG=<n> python3 tools/rig/amarchy_gui_rig.py --setup` (stages the build to VTCX:Apps/bin and starts it).
  b: after the reboot and the Amarchy start (a --os 32 rig boots the clean 3.2 image, whose S:User-Startup has no kit
     block and starts no Amarchy: b then starts VTCX:Apps/bin/Amarchy itself, the way a kit's `Run >NIL: UP-Term:bin/Amarchy`
     would; the marker is read at that start, desk_dos_marker_recover): AMARCHY_DESK_MARKER_REVERTS == 1, marker and marker dir gone, ENV:Sys + ENVARC:Sys md5 equal the recorded ones."""
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
    # the return code, not the text: 3.2's List says 'object not found' (rc 20) for a missing file, no 'rror' in it
    rc, out = c.run('List %s QUICK' % p, 30)
    return rc == 0 and bool(out.strip())


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
        # The apply under test: no Keep. The pick returns only when the apply is done and the menu closed, which takes longer than the 15 s
        # requester: its timeout revert has deleted the marker by then. So the pick runs in a thread and the marker is polled while it runs;
        # the marker is written last (after its copies), so seeing it means the set is complete.
        import threading
        pt = threading.Thread(target=lambda: d.pick(keep=False), daemon=True)
        pt.start()
        t0 = time.time()
        seen = False
        while pt.is_alive() and time.time() - t0 < 120 and not seen:
            seen = exists('ENVARC:Amarchy/pending-revert')
            if not seen:
                time.sleep(0.3)
        t1 = time.time()
        d.say(seen, 'a: marker ENVARC:Amarchy/pending-revert written at confirm start', 'seen %.1f s after the pick began' % (t1 - t0))
        # no snapshot of the applied state here: the 15 s requester is running and the reboot must come before it times out; the marker itself
        # proves the apply changed a target (desk_dos_apply asks for a confirm only when r > 0)
        json.dump(S, open(st, 'w'))
        print('[INFO] now (within the 15 s) wait 5 s quiet, then rig.py reboot; failed', d.failed)
    else:
        for _ in range(15):
            if g.process_running('Amarchy'):
                break
            time.sleep(2)
        else:
            print('[INFO] the boot started no Amarchy (no kit S:User-Startup block on this system): starting it', flush=True)
            g.start_amarchy('b')
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
