#!/usr/bin/env python3
"""amarchy_p2_rig.py -- reachability of Amarchy P2 (plan 2026-10-29-amarchy-p2-status-bar.md, R2.16). Rig purpose text:
"Amarchy s24: P2 tick and BAR STATUS rig check". Needs the rig up (UPTERM_RIG=<n> rig.py start --max --purpose ...);
this script never starts or stops a rig. Entry point: the ARexx port AMARCHY of the running commodity (the same
rexx_cmd dispatch a script or menu row reaches). Helpers are amarchy_tile_rig's (send, wait_for, bar_box, stage).

Steps, in order (each prints "[OK] <step>: ..." or "[FAIL] <step>: <detail>"); later P2 rows add steps here:
  start        Amarchy restarted; STATUS answers, ends with ticks=N, fits the 160 byte reply (no cut key)
  tick_rises   R2.3a/R2.9: ticks= rises by 2 to 4 in 3 s (the 1 s commodity tick runs)
  bar_status   BAR STATUS answers "bar=1 draws=N panels=N launches=N raises=N ticks=N", draws >= 1, a window "Amarchy bar"
  bar_off      BAR OFF: bar=0, no window "Amarchy bar", ticks still rising (the tick outlives the bar)
  bar_on       BAR ON: bar=1, the window is back, ticks still rising
  status_keys  STATUS no longer carries draws= panels= launches= raises= (they moved to BAR STATUS)
  clock        R2.6: BAR TEXT segment 2 reads like "Wed 15:25"; a real left click flips it to "Wed 29 Oct 2026", another back
  ram          R2.7: segment 3 reads "C <n> F <n>" and the numbers move or stay valid across 5 s of ticks
  cpu          R2.8: segment 5 reads "CPU <n>%"; idle it stays under 50, while idleprobe busy burns the CPU it reaches 50 or more
  network      R2.10: segment 6 reads "Network emulated" on the rig (UAE bsdsocket, no interface query is made)
  screens      R2.11: a public screen entry (id 200 and up) exists, exactly one carries the front-screen mark (*); a real
               left click on it keeps the bar alive and the entry listed
  system       R2.12a: a real left click on segment 4 raises BAR STATUS panels= by 1; Escape closes the menu
  bar_hold     W43: BAR OFF then BAR ON during the 30 s Workbench-mode hold open no window; the bar opens when the hold ends
  bar_conf     R2.13a/R2.13c: ENV:Amarchy/bar.conf with "start off" and Amarchy restarted gives BAR STATUS bar=0 and no window;
               a real Super+B (ami.key, LAmiga + B) makes bar=1 and opens the window; the test file is removed and Amarchy restarted
  python3 tools/rig/amarchy_p2_rig.py [--only STEP] [--no-build]
Resumable: --only STEP reruns one step against the state the earlier steps left; verdicts are written to
<RIG>/amarchy-p2/verdict.json as each step completes. Exit code is nonzero when any step fails."""
import argparse, json, pathlib, re, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import condev_rig as c
import stage_rig
import amarchy_tile_rig as t

OUT = pathlib.Path(paths.RIG) / 'amarchy-p2'
KEYS_BAR = ['bar', 'draws', 'panels', 'launches', 'raises', 'ticks']


def kv(text):
    return {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', text)}


def status():
    rc, res = t.send('AMARCHY', 'STATUS', timeout=5)
    return rc, res


def ticks():
    rc, res = status()
    return kv(res).get('ticks') if rc == 0 else None


def rises(secs=3):
    t0 = ticks()
    time.sleep(secs)
    t1 = ticks()
    return t0, t1, t0 is not None and t1 is not None and 2 <= t1 - t0 <= 4


def s_start():
    t.send('AMTILE', 'QUIT')
    t.send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
    ok = t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
    rc, res = status()
    return bool(ok) and rc == 0 and res.endswith(tuple('0123456789')) and 'ticks=' in res, 'len %d: %r' % (len(res), res)


def s_tick_rises():
    t0, t1, ok = rises()
    return ok, 'ticks %r -> %r in 3 s' % (t0, t1)


def s_bar_status():
    rc, res = t.send('AMARCHY', 'BAR STATUS', timeout=5)
    d = kv(res)
    ok = rc == 0 and list(d) == KEYS_BAR and d['bar'] == 1 and d['draws'] >= 1 and t.bar_box() is not None
    return ok, 'rc %r len %d: %r, bar window %r' % (rc, len(res), res, t.bar_box())


def s_bar_off():
    rc, res = t.send('AMARCHY', 'BAR OFF', timeout=5)
    d = kv(res)
    _, srs = status()
    t0, t1, rising = rises()
    gone = t.bar_box() is None
    ok = rc == 0 and d.get('bar') == 0 and kv(srs).get('bar') == 0 and gone and rising
    return ok, 'rc %r reply %r, STATUS bar=%r, window gone %s, ticks %r -> %r' % (rc, res, kv(srs).get('bar'), gone, t0, t1)


def s_bar_on():
    rc, res = t.send('AMARCHY', 'BAR ON', timeout=5)
    d = kv(res)
    t0, t1, rising = rises()
    back = t.wait_for(lambda: t.bar_box() is not None, 3)
    ok = rc == 0 and d.get('bar') == 1 and bool(back) and rising
    return ok, 'rc %r reply %r, window back %s, ticks %r -> %r' % (rc, res, bool(back), t0, t1)


def s_status_keys():
    rc, res = status()
    gone = [k for k in ('draws', 'panels', 'launches', 'raises') if k + '=' in res]
    return rc == 0 and not gone and 'bar=' in res and res.rstrip().split(' ')[-1].startswith('ticks='), \
        'moved keys still in STATUS: %r; %r' % (gone, res)


def segs():
    """BAR TEXT -> {id: (x, w, accent, text)}."""
    rc, res = t.send('AMARCHY', 'BAR TEXT', timeout=5)
    out = {}
    for m in re.finditer(r'(\d+)@(\d+)\+(\d+)(\*?)=([^|]*)', res if rc == 0 else ''):
        out[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), bool(m.group(4)), m.group(5))
    return out


def click_seg(sid):
    import amarchy_bar_rig as b
    x, w, _, _ = segs()[sid]
    _, h = b.screen_size()
    b.click(0, at=(x + w // 2, h - 4))


CLOCK = r'[A-Z][a-z]{2} \d\d:\d\d'
DATE = r'[A-Z][a-z]{2} \d\d [A-Z][a-z]{2} \d{4}'


def s_clock():
    a = segs().get(2, (0, 0, 0, ''))[3]
    click_seg(2)
    b = segs().get(2, (0, 0, 0, ''))[3]
    click_seg(2)
    c2 = segs().get(2, (0, 0, 0, ''))[3]
    ok = re.fullmatch(CLOCK, a) and re.fullmatch(DATE, b) and re.fullmatch(CLOCK, c2)
    return bool(ok), 'text %r -> click -> %r -> click -> %r' % (a, b, c2)


def s_ram():
    a = segs().get(3, (0, 0, 0, ''))[3]
    time.sleep(5)
    b = segs().get(3, (0, 0, 0, ''))[3]
    pat = r'C \S+ F \S+'
    return bool(re.fullmatch(pat, a) and re.fullmatch(pat, b)), 'ram %r then %r' % (a, b)


def seg_text(sid):
    return segs().get(sid, (0, 0, 0, ''))[3]


def cpu_pct(text):
    m = re.fullmatch(r'CPU (\d+)%', text)
    return int(m.group(1)) if m else None


def s_cpu():
    import ami
    time.sleep(3)
    idle = cpu_pct(seg_text(5))
    ami.put(str(stage_rig.BUILD / 'idleprobe'), 'RAM:idleprobe')
    c.run('Run >NIL: RAM:idleprobe busy 600')
    peak = 0
    for _ in range(8):
        time.sleep(1.1)
        p = cpu_pct(seg_text(5))
        peak = max(peak, p if p is not None else 0)
    ok = idle is not None and idle < 50 and peak >= 50
    return ok, 'idle %r, peak under busy loop %d, text %r' % (idle, peak, seg_text(5))


def s_network():
    a = seg_text(6)
    time.sleep(6)
    b = seg_text(6)
    return a == b == 'Network emulated', 'network %r then %r' % (a, b)


def s_screens():
    sc = {i: v for i, v in segs().items() if 200 <= i < 210}
    marks = [i for i, v in sc.items() if v[2]]
    ok = bool(sc) and len(marks) == 1
    if ok:
        click_seg(min(sc))
        ok = bool(t.send('AMARCHY', 'BAR STATUS', timeout=5)[0] == 0) and any(200 <= i < 210 for i in segs())
    return ok, 'entries %r, marked %r' % ({i: v[3] for i, v in sc.items()}, marks)


def s_system():
    before = kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('panels', 0)
    click_seg(4)
    after = kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('panels', 0)
    import ami
    ami.key(0x45, 0)
    time.sleep(1)
    return after == before + 1, 'panels %r -> %r' % (before, after)


def s_bar_hold():
    """W43 hold: ENV:Sys/screenmode.prefs names a mode other than the Workbench's, so Amarchy waits (up to
    WBM_TIMEOUT_TICKS = 30 s) before opening any window. BAR OFF then BAR ON said during the wait are only recorded
    (bar_plan); the window must not open during the hold and must open when the hold ends."""
    import amarchy_bar_rig as b
    w, _ = b.screen_size()
    other = 'rtg' if w <= 640 else 'aga16'
    t.send('AMTILE', 'QUIT')
    t.send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    c.run('Copy ENV:Sys/screenmode.prefs RAM:smsave.prefs')
    c.run('Copy VTC:screenmode.%s.prefs ENV:Sys/screenmode.prefs' % other)
    try:
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        up = t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
        t0 = time.time()
    finally:
        c.run('Copy RAM:smsave.prefs ENV:Sys/screenmode.prefs')
    rc1, r1 = t.send('AMARCHY', 'BAR OFF', timeout=5)
    time.sleep(3)
    held1 = t.bar_box() is None
    rc2, r2 = t.send('AMARCHY', 'BAR ON', timeout=5)
    time.sleep(3)
    held2 = t.bar_box() is None
    opened = t.wait_for(lambda: t.bar_box() is not None, 40)
    el = time.time() - t0
    ok = bool(up) and rc1 == 0 and rc2 == 0 and held1 and held2 and bool(opened) and 20 <= el <= 40
    return ok, 'prefs %s (hold), BAR OFF rc %r %r no window %s; BAR ON rc %r %r no window %s; window opened %s after %.0f s' % (
        other, rc1, r1, held1, rc2, r2, held2, bool(opened), el)


RAW_B = 0x35
SUPER = 0x40


def s_bar_conf():
    """bar.conf `start off` is read at start (R2.13a); Super+B (keymap row bar:toggle, R2.13c) flips the bar through the real keyboard path"""
    import ami
    t.send('AMTILE', 'QUIT')
    t.send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    c.run('MakeDir >NIL: ENV:Amarchy')
    c.run('Echo "start off" >ENV:Amarchy/bar.conf')
    try:
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        up = t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
        time.sleep(3)
        d0 = kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1])
        none0 = t.bar_box() is None
        ami.key(RAW_B, SUPER)
        on = t.wait_for(lambda: kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('bar') == 1, 5)
        win = t.wait_for(lambda: t.bar_box() is not None, 5)
        ami.key(RAW_B, SUPER)
        off = t.wait_for(lambda: kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('bar') == 0, 5)
    finally:
        c.run('Delete >NIL: ENV:Amarchy/bar.conf')
        t.send('AMARCHY', 'QUIT')
        time.sleep(1.5)
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
    ok = bool(up) and d0.get('bar') == 0 and none0 and bool(on) and bool(win) and bool(off)
    return ok, 'start off: bar=%r no window %s; Super+B -> bar=1 %s window %s; Super+B -> bar=0 %s' % (
        d0.get('bar'), none0, bool(on), bool(win), bool(off))


STEPS = [('start', s_start), ('tick_rises', s_tick_rises), ('bar_status', s_bar_status), ('bar_off', s_bar_off),
         ('bar_on', s_bar_on), ('status_keys', s_status_keys),
         ('clock', s_clock), ('ram', s_ram), ('cpu', s_cpu), ('network', s_network), ('screens', s_screens), ('system', s_system),
         ('bar_hold', s_bar_hold), ('bar_conf', s_bar_conf)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', choices=[n for n, _ in STEPS], help='run one step against the state left by earlier runs')
    ap.add_argument('--no-build', action='store_true', help='do not rebuild/stage Amarchy')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    verdict = OUT / 'verdict.json'
    done = json.loads(verdict.read_text()) if a.only and verdict.exists() else {}
    stage_rig.report(stage_rig.stage_build(names=('Amarchy',), build=not a.no_build))
    fails = 0
    for name, fn in STEPS:
        if a.only and a.only != name:
            continue
        try:
            ok, detail = fn()
        except Exception as e:
            ok, detail = False, 'exception %r' % (e,)
        fails += not ok
        done[name] = [bool(ok), detail]
        verdict.write_text(json.dumps(done))
        print('%s %s: %s' % ('[OK]' if ok else '[FAIL]', name, detail), flush=True)
    print('amarchy_p2_rig: %d step(s) failed' % fails)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
