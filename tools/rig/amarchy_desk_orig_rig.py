"""DESK-STALE-ORIG rig: a Revert drops the originals of the targets it restored, so a hand edit made after the Revert is the original the next apply saves.
UPTERM_RIG=<n> python3 tools/rig/amarchy_desk_orig_rig.py   (rig up, Amarchy running, rig prefs as after amarchy_persist_rig.py c)
  1. desktop theme on, pick a theme without Keep: the 15 s timeout reverts it (AMARCHY_DESK_ORIG_DROPPED rises, desk-original/ holds no file)
  2. pointer.prefs edited by hand (one byte, ENV: and ENVARC:), theme picked again with Keep, desktop theme off, pick: pointer.prefs equals the hand edit
KNOWN OPEN (session 29): step 1 passes for font/wbpattern/palette (ORIG_DROPPED 0 -> 4 on the timeout Revert) but pointer.prefs ENV: stays at the themed bytes
after the Revert and its original is kept, so this script still fails on pointer; cause not yet found (revert not applied, or a re-apply after it)."""
import os, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import amarchy_desk_rig as d
import ami, condev_rig as c, amarchy_theme_save_rig as t
import amarchy_persist_rig as pr

F = 'pointer.prefs'


def main():
    c.run('MakeDir >NIL: ENV:Amarchy')
    c.run('Delete ENVARC:Amarchy/desk-original ALL QUIET FORCE >NIL:', 30)
    pr.prefs(True)
    n0 = pr.sent('AMARCHY_DESK_ORIG_DROPPED')
    d.pick(keep=False)
    time.sleep(22)   # the Keep requester times out after 15 s and reverts
    n1 = pr.sent('AMARCHY_DESK_ORIG_DROPPED')
    left = c.run('List ENVARC:Amarchy/desk-original QUICK', 30)[1].strip()
    print('[INFO] DROPPED %d -> %d; desk-original after the Revert: %r' % (n0, n1, left))
    d.say(n1 > n0, 'Revert: AMARCHY_DESK_ORIG_DROPPED moved')
    d.say(F not in left, 'Revert: no %s original left' % F, left[:80])
    raw = bytearray(ami.read_file('ENV:Sys/' + F))
    raw[-1] ^= 1
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(bytes(raw))
    ami.put(f.name, 'RAM:hand-edit.prefs')   # put cannot rename over a file in ENV:/ENVARC: (rename into place failed)
    for w in ('ENV', 'ENVARC'):
        c.run('Copy RAM:hand-edit.prefs %s:Sys/%s CLONE' % (w, F), 30)
    os.unlink(f.name)
    want = pr.md5('ENV:Sys/' + F)
    d.pick(keep=True)
    time.sleep(3)
    print('[INFO] originals now:', c.run('List ENVARC:Amarchy/desk-original QUICK', 30)[1].strip().replace('\n', ' | '))
    pr.prefs(False)
    d.pick(keep=True)
    time.sleep(3)
    for w in ('ENV', 'ENVARC'):
        got = pr.md5('%s:Sys/%s' % (w, F))
        d.say(got == want, 'off: %s:Sys/%s equals the hand edit' % (w, F), '%s vs %s' % (got, want))
    print('failed', d.failed)
    return 1 if d.failed else 0


sys.exit(main())
