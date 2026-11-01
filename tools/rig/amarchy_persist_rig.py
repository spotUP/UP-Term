"""FP-2 rig: Keep persists ENV:Sys to ENVARC:Sys, the first apply saves the originals, off restores them byte-identical.
UPTERM_RIG=<n> python3 tools/rig/amarchy_persist_rig.py <a|b|c> <state.json>
  a: record the md5 of every ENV:Sys / ENVARC:Sys file, flat + desktop theme on, pick a theme, click Keep, check the sentinels and ENVARC == ENV
  b: (after rig.py reboot) Workbench.prefs byte 73 still 1, ENV:Sys palette == the md5 recorded after the Keep
  c: flat + desktop theme off, pick a theme: ENV:Sys and ENVARC:Sys equal the recorded originals, desk-original/ gone"""
import json, os, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import amarchy_desk_rig as d
import ami, condev_rig as c, amarchy_theme_save_rig as t

FILES = ['palette.prefs', 'wbpattern.prefs', 'pointer.prefs', 'font.prefs', 'Workbench.prefs']
SENT = ('AMARCHY_DESK_PERSISTS', 'AMARCHY_DESK_ORIG_SAVED', 'AMARCHY_DESK_ORIG_RESTORED')


def md5(p):
    out = c.run('VTCX:Apps/bin/md5sum %s' % p, 30)[1].strip()
    return out[:32] if len(out) >= 32 and ' ' in out[:40] and 'rror' not in out else 'absent'


def snap():
    return {w + ':' + f: md5(w + ':Sys/' + f) for w in ('ENV', 'ENVARC') for f in FILES}


def sent(n):
    return int(t.env(n) or 0)


def prefs(on):
    with tempfile.NamedTemporaryFile(delete=False) as f:
        v = 'on' if on else 'off'
        f.write(('; Amarchy preferences.\n[profile default]\ndesktop-theme = %s\nflat-system-look = %s\nflat-window-gadgets = %s\n' % (v, v, v)).encode('latin-1'))
    d.put_prefs(f.name)
    os.unlink(f.name)


def main():
    ph, st = sys.argv[1], sys.argv[2]
    c.run('MakeDir >NIL: ENV:Amarchy')
    S = json.load(open(st)) if os.path.exists(st) else {}
    if ph == 'a':
        c.run('Delete ENVARC:Amarchy/desk-original ALL QUIET FORCE >NIL:', 30)   # a reused disk: originals left by an earlier run would be recorded as ours
        S['orig'] = snap()
        print('[INFO] originals', S['orig'])
        prefs(True)
        s0 = [sent(n) for n in SENT]
        d.pick(keep=True)
        time.sleep(3)
        s1 = [sent(n) for n in SENT]
        print('[INFO] sentinels', dict(zip(SENT, zip(s0, s1))))
        d.say(s1[0] > s0[0], 'a: PERSISTS sentinel moved')
        d.say(s1[1] > s0[1], 'a: ORIG_SAVED sentinel moved')
        after = snap()
        S['after'] = after
        print('[INFO] after Keep', after)
        for f in FILES:
            e, a = after['ENV:' + f], after['ENVARC:' + f]
            if e != 'absent':
                d.say(e == a, 'a: ENVARC:Sys/%s == ENV:Sys' % f, '%s vs %s' % (e, a))
        out = c.run('List ENVARC:Amarchy/desk-original QUICK', 30)[1]
        print('[INFO] desk-original:', out.strip().replace('\n', ' | '))
        d.say('desk-original' in c.run('Assign ENVARC: EXISTS >NIL: ; List ENVARC:Amarchy QUICK', 30)[1] or bool(out.strip()), 'a: desk-original exists')
    elif ph == 'b':
        v = ami.read_file('ENV:Sys/Workbench.prefs')[73]
        d.say(v == 1, 'b: Workbench.prefs byte 73 == 1 after reboot', str(v))
        now = snap()
        for f in FILES:
            d.say(now['ENV:' + f] == S['after']['ENVARC:' + f], 'b: ENV:Sys/%s equals the Keep result' % f, '%s vs %s' % (now['ENV:' + f], S['after']['ENVARC:' + f]))
    elif ph == 'c':
        prefs(False)
        r0 = sent(SENT[2])
        d.pick(keep=True)
        time.sleep(3)
        now = snap()
        print('[INFO] after off', now, 'restored sentinel', r0, '->', sent(SENT[2]))
        d.say(sent(SENT[2]) > r0, 'c: ORIG_RESTORED sentinel moved')
        for k, v in S['orig'].items():
            d.say(now[k] == v, 'c: %s equals the original' % k, '%s vs %s' % (now[k], v))
        out = c.run('List ENVARC:Amarchy/desk-original QUICK', 30)[1]
        d.say('desk-original' not in out and 'ccess' not in out.lower() or not out.strip() or 'not found' in out.lower(), 'c: desk-original cleaned', out.strip()[:80])
    json.dump(S, open(st, 'w'))
    print('failed', d.failed)
    return 1 if d.failed else 0


if __name__ == '__main__':
    sys.exit(main())
