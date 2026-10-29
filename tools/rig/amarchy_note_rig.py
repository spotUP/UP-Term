#!/usr/bin/env python3
"""amarchy_note_rig.py -- the Amarchy notice port, amnotify and safe-off (plan 2026-10-29-amarchy-p4-capture-notify-power.md
R4.9a, R4.10, R4.12 script part, R4.15 notes=N).

Entry point: the running Amarchy commodity. Sentinel that the new code ran: STATUS ends with notes=N and N moves only when a
NOTIFY reached note_core through the port AMARCHY.NOTIFY (ARexx NOTIFY / COUNT / CLEAR), through the CLI RAM:amnotify, or
through the script S/safe-off (Reboot DRYRUN, then amnotify). NOTES on AMARCHY answers `notes=N shown=N last=<title>`.
Steps: baseline 0 -> NOTIFY verb +1 -> amnotify (Unix and AmigaDOS forms) +2 -> bad args rc 10 -> safe-off +1 and
last=Safe to switch off -> QUIT removes the port and amnotify answers rc 5. Needs the rig up (rig.py start --max) and
build/amiga/{Amarchy,amnotify,Reboot}. UPTERM_RIG=<n>."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, condev_rig as c
import amarchy_rig as ar

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, '..', '..', 'build', 'amiga')
SAFEOFF = os.path.join(HERE, '..', '..', 'dist', 'S', 'safe-off')
bad = 0


def check(ok, what, seen=''):
    global bad
    bad += not ok
    print('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': %s' % (seen,)), flush=True)
    return ok


def notify(cmd):
    """(rc, result) from the port AMARCHY.NOTIFY; (-1, '') when it is gone."""
    m = ami._m()
    try:
        with ami._conn() as conn:
            r = m.arexx(conn, 'AMARCHY.NOTIFY', cmd)
        return r[0], r[2].strip()
    except m.WasabiError:
        return -1, ''


def notes_of():
    """The NOTES verb on AMARCHY as a dict."""
    d = {}
    s = ar.arexx('NOTES')
    for k in ('notes', 'shown'):
        for w in s.split():
            if w.startswith(k + '='):
                d[k] = int(w.split('=')[1])
    d['last'] = s.split(' last=', 1)[1] if ' last=' in s else None
    return d


def status_notes():
    st = ar.arexx('STATUS') or ''
    return st, int(st.rsplit(' notes=', 1)[1]) if ' notes=' in st else -1


def main():
    for n in ('Amarchy', 'amnotify', 'Reboot'):
        ami.put(os.path.join(BUILD, n), 'RAM:' + n)
    ami.put(SAFEOFF, 'RAM:safe-off')
    c.run('Protect RAM:Amarchy RWED')
    if ar.port_up():
        ar.arexx('QUIT')
        time.sleep(2)
    c.run('Path RAM: ADD')
    c.run('Run >NIL: RAM:Amarchy')
    time.sleep(3)
    check(ar.port_up(), 'Amarchy up')
    rc, n = notify('COUNT')
    check(rc == 0 and n == '0', 'AMARCHY.NOTIFY answers COUNT 0 at start', (rc, n))
    st, v = status_notes()
    check(v == 0 and st.endswith(' notes=0') and ' shots=' in st, 'STATUS ends with notes=0', st[-40:])
    print('INFO STATUS length %d bytes (REXX_HOST_OUT 160 including the NUL)' % len(st))
    check(len(st) <= 159, 'STATUS fits REXX_HOST_OUT', len(st))
    check(notes_of() == {'notes': 0, 'shown': 0, 'last': ''}, 'NOTES at start', notes_of())

    # the port verb
    rc, _ = notify('NOTIFY Hello|first body|3')
    check(rc == 0, 'NOTIFY verb rc 0', rc)
    check(notify('COUNT') == (0, '1'), 'COUNT rose to 1', notify('COUNT'))
    check(status_notes()[1] == 1, 'STATUS notes=1 after the port NOTIFY', status_notes()[0][-20:])
    d = notes_of()
    check(d['notes'] == 1 and d['shown'] == 1 and d['last'] == 'Hello', 'NOTES notes=1 shown=1 last=Hello', d)
    check(notify('NOTIFY |nothing')[0] == 10, 'NOTIFY without a title answers rc 10')
    check(notify('BOGUS')[0] == 10, 'an unknown verb answers rc 10')
    check(status_notes()[1] == 1, 'a refused NOTIFY does not count')

    # the CLI, both forms
    rc = c.run('RAM:amnotify Done "build ok" SECONDS 3')[0]
    check(rc == 0 and status_notes()[1] == 2, 'amnotify (AmigaDOS form) rc 0, notes=2', (rc, status_notes()[1]))
    rc = c.run('RAM:amnotify -t 4 Unixform "from -t"')[0]
    d = notes_of()
    check(rc == 0 and d['notes'] == 3 and d['last'] == 'Unixform', 'amnotify (Unix form) rc 0, notes=3, last=Unixform', (rc, d))
    rc = c.run('RAM:amnotify')[0]
    check(rc == 10 and status_notes()[1] == 3, 'amnotify with no title: rc 10, no notice', (rc, status_notes()[1]))
    rc = c.run('RAM:amnotify X SECONDS abc')[0]
    check(rc == 10 and status_notes()[1] == 3, 'amnotify with bad SECONDS: rc 10, no notice', (rc, status_notes()[1]))
    rc, out = c.run('RAM:amnotify ?')
    check(rc == 0 and 'amnotify' in out and status_notes()[1] == 3, 'amnotify ? prints help and posts nothing', (rc, out[:60]))
    check(notify('CLEAR')[0] == 0 and notes_of()['shown'] == 0 and notes_of()['notes'] == 3, 'CLEAR empties the queue, the count stays')

    # safe-off (R4.12): Reboot DRYRUN flushes every volume and does not restart; rc 0 -> "Safe to switch off"
    rc, out = c.run('Reboot DRYRUN')
    print('INFO Reboot DRYRUN rc %d: %s' % (rc, out.strip().replace('\n', ' | ')[:200]))
    want = 'Safe to switch off' if rc == 0 else 'Not all disks answered'
    # the script as the menu row runs it: with amnotify and Reboot on the command path
    import tempfile
    with tempfile.NamedTemporaryFile('w', suffix='.run', delete=False) as w:
        w.write('Path RAM: ADD\nExecute RAM:safe-off\n')
    ami.put(w.name, 'RAM:so-run')
    os.unlink(w.name)
    c.run('Execute RAM:so-run')
    d = notes_of()
    check(d['notes'] == 4 and d['last'] == want, 'safe-off: notes=4, last=%s' % want, d)
    check(ami.read_file('T:safeoff.txt') is not None, 'safe-off wrote T:safeoff.txt')

    # quit
    ar.arexx('QUIT')
    time.sleep(2.5)
    check(notify('COUNT')[0] == -1, 'port AMARCHY.NOTIFY is gone after QUIT')
    rc, out = c.run('RAM:amnotify Late body')
    check(rc == 5 and 'not running' in out, 'amnotify after QUIT: rc 5 with a message, no hang', (rc, out[:80]))
    print('PASS' if not bad else 'FAIL (%d)' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
