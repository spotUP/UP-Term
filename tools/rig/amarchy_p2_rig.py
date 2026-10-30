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
  columns      AM-BAR-COLUMNS: segment x and width unchanged across a clock flip to the long form and CPU text changes; none past the screen edge
  network      R2.10: segment 6 reads "Network emulated" on the rig (UAE bsdsocket, no interface query is made)
  screens      R2.11: a public screen entry (id 200 and up) exists, exactly one carries the front-screen mark (*); a real
               left click on it keeps the bar alive and the entry listed
  system       R2.12a: a real left click on segment 4 raises BAR STATUS panels= by 1; Escape closes the menu
  bar_hold     W43: BAR OFF then BAR ON during the 30 s Workbench-mode hold open no window; the bar opens when the hold ends
  bar_conf     R2.13a/R2.13c: ENV:Amarchy/bar.conf with "start off" and Amarchy restarted gives BAR STATUS bar=0 and no window;
               a real Super+B (ami.key, LAmiga + B) makes bar=1 and opens the window; the test file is removed and Amarchy restarted
  tray         R2.12: bar.conf `segment ram off`, `right system tray cpu` and `run Mark <cmd>`: segment 3 gone, x order 4 < 300 < 5,
               segment 300 reads Mark, a real left click on it runs the command (sentinel file made only after the click)
  recolour     R2.5b (OMARCHY-AI F3): saving theme A then B in ENV:Amarchy/prefs makes BAR PENS answer that theme with its bg/text
               colours and recolours=+1 each, and BAR STATUS draws= rises (the bar was drawn again); test files and prefs restored
  python3 tools/rig/amarchy_p2_rig.py [--only STEP] [--no-build]
Resumable: --only STEP reruns one step against the state the earlier steps left; verdicts are written to
<RIG>/amarchy-p2/verdict.json as each step completes. Exit code is nonzero when any step fails."""
import argparse, json, pathlib, re, shutil, sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import condev_rig as c
import stage_rig
import amarchy_tile_rig as t
import ami
import typewin as tw

OUT = pathlib.Path(paths.RIG) / 'amarchy-p2'
KEYS_BAR = ['bar', 'draws', 'panels', 'launches', 'raises', 'ticks', 'net']


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


def xs():
    return {k: (v[0], v[1]) for k, v in segs().items()}


def s_columns():
    """AM-BAR-COLUMNS / BAR-CLOCK-ALT-CLIP: every segment keeps its x and width while the clock flips to the long form and
    the CPU text changes under a busy loop; nothing reaches past the screen width; the clock slot holds the long form."""
    import amarchy_bar_rig as b
    sw, _ = b.screen_size()
    before = xs()
    for _ in range(3):                      # the first click of a fresh script run is sometimes taken as focus only
        click_seg(2)
        flipped = seg_text(2)
        if re.fullmatch(DATE, flipped):
            break
    mid = xs()
    ami.put(str(stage_rig.BUILD / 'idleprobe'), 'RAM:idleprobe')
    c.run('Run >NIL: RAM:idleprobe busy 600')
    texts = {seg_text(5)}
    for _ in range(8):
        time.sleep(1.1)
        texts.add(seg_text(5))
    after = xs()
    if re.fullmatch(DATE, seg_text(2)):
        click_seg(2)
    over = [k for k, (x, w) in after.items() if w and x + w > sw]
    ok = before == mid == after and not over and re.fullmatch(DATE, flipped) and len(texts) > 1 
    return bool(ok), 'before %r, flipped %r mid %r, cpu texts %r, after %r, past edge %r' % (before, flipped, mid, sorted(texts), after, over)


def s_network():
    a = seg_text(6)
    time.sleep(6)
    b = seg_text(6)
    net = kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('net')
    return a == b == 'Network emulated' and net == 1, 'network %r then %r, BAR STATUS net=%r (1 = emulated)' % (a, b, net)


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


def pens():
    rc, res = t.send('AMARCHY', 'BAR PENS', timeout=5)
    d = dict(re.findall(r'(\w+)=(\S+)', res))
    return rc, res, d


def s_recolour():
    """R2.5b (closes OMARCHY-AI F3): saving another theme recolours the bar. BAR PENS reports the theme the helper's pens
    belong to, how often they were retaken and the role colours; BAR STATUS draws= shows the bar was drawn again"""
    prefs, drawer = 'ENV:Amarchy/prefs', 'ENVARC:Amarchy/themes'
    names = [('p2rea', '102030', 'E0E0E0'), ('p2reb', 'C0D0E0', '101010')]
    c.run('MakeDir >NIL: ENVARC:Amarchy')
    c.run('MakeDir >NIL: ' + drawer)
    c.run('Delete >NIL: T:p2prefs.bak')
    had = c.run('Copy >NIL: %s T:p2prefs.bak' % prefs)[0] == 0
    seen = []
    try:
        for n, bg, fg in names:
            c.run('Echo "background = *"#%s*"" >%s/%s.theme' % (bg, drawer, n))
            c.run('Echo "foreground = *"#%s*"" >>%s/%s.theme' % (fg, drawer, n))
        if t.bar_box() is None:
            t.send('AMARCHY', 'BAR ON', timeout=5)
        rc0, r0, d0 = pens()
        draws0 = kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('draws', 0)
        n0 = int(d0.get('recolours', -1))
        for i, (n, bg, fg) in enumerate(names):
            c.run('MakeDir >NIL: ENV:Amarchy')
            c.run('Echo "amarchy-gui-theme = %s" >%s' % (n, prefs))
            got = t.wait_for(lambda: pens()[2].get('theme') == n, 6)
            rc, r, d = pens()
            draws = kv(t.send('AMARCHY', 'BAR STATUS', timeout=5)[1]).get('draws', 0)
            seen.append((n, bool(got), d.get('bg'), bg, d.get('text'), fg, d.get('recolours'), draws))
            if not (got and d.get('bg') == bg and d.get('text') == fg and int(d.get('recolours', -1)) == n0 + i + 1 and draws > draws0):
                return False, 'switch to %s: %r (before %r, draws0 %r)' % (n, r, r0, draws0)
            draws0 = draws
    finally:
        for n, _, _ in names:
            c.run('Delete >NIL: %s/%s.theme' % (drawer, n))
        c.run('Delete >NIL: ' + prefs)
        if had:
            c.run('Copy >NIL: T:p2prefs.bak ' + prefs)
    return True, 'start %r; ' % r0 + '; '.join('%s: theme seen %s bg %s (want %s) text %s (want %s) recolours %s draws %s' % x for x in seen)


def s_tray():
    """R2.12: bar.conf rows reach the live bar. `segment ram off` hides segment 3, `right system tray cpu` orders them (BAR TEXT x of 4 < 300 < 5),
    `run Mark ...` makes tray segment 300 labelled Mark, and a real left click on it runs the row's command line through proclaunch (the
    command copies bar.conf to RAM:p2tray.flag: the sentinel exists only after the click)"""
    t.send('AMTILE', 'QUIT')
    t.send('AMARCHY', 'QUIT')
    time.sleep(1.5)
    c.run('MakeDir >NIL: ENV:Amarchy')
    c.run('Delete >NIL: RAM:p2tray.flag')
    c.run('Echo "segment ram off" >ENV:Amarchy/bar.conf')
    c.run('Echo "right system tray cpu" >>ENV:Amarchy/bar.conf')
    c.run('Echo "run Mark C:Copy ENV:Amarchy/bar.conf RAM:p2tray.flag" >>ENV:Amarchy/bar.conf')
    try:
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
        t.wait_for(lambda: 4 in segs(), 8)
        sg = segs()
        before = c.run('Type RAM:p2tray.flag')[0] == 0
        order = all(k in sg for k in (4, 300, 5)) and sg[4][0] < sg[300][0] < sg[5][0]
        hidden = 3 not in sg
        label = sg.get(300, (0, 0, 0, ''))[3]
        if 300 in sg:
            click_seg(300)
        made = t.wait_for(lambda: c.run('Type RAM:p2tray.flag')[0] == 0, 8)
    finally:
        c.run('Delete >NIL: ENV:Amarchy/bar.conf')
        c.run('Delete >NIL: RAM:p2tray.flag')
        t.send('AMARCHY', 'QUIT')
        time.sleep(1.5)
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
    ok = order and hidden and label == 'Mark' and not before and bool(made)
    return ok, 'x4<x300<x5 %s (%r); ram segment gone %s; label %r; sentinel before click %s, after click %s' % (
        order, {k: v[0] for k, v in sg.items() if k in (4, 300, 5)}, hidden, label, before, bool(made))


def tabpix():
    """pixel histogram {rrggbb: count} of the front window's text area"""
    w, h, rows = ami.grab((0, 14, 640, 242))
    n = {}
    for r in rows:
        for i in range(0, len(r) - 2, 3):
            k = bytes(r[i:i + 3]).hex()
            n[k] = n.get(k, 0) + 1
    return n


def s_agenttab():
    """OMARCHY-AI F3 (owner decision 2026-10-29: an Amarchy theme switch recolours ALL open tabs): a Claude agent tab
    (dummy ENVARC:Claude/key) and a plain shell tab both take the terminal colours of the Amarchy theme picked through the
    real entry point, a keymap row `gtheme:NAME` fired by `AMARCHY RUN n` (amarchy.c do_row -> gt_dos_select -> gt_dos_term_send
    -> ENV:up-term/theme-active -> each tab's watch_worker -> theme_apply). Sentinel: the file ENV:up-term/theme-active is
    deleted before each switch and holds `[profile NAME]` after it, so only gt_dos_term_send can have made it. Per tab the new
    background must fill the text area (over 50000 pixels) and, from the second switch on, the old one must be gone"""
    import tempfile
    drawer, prefs = 'ENVARC:Amarchy/themes', 'ENV:Amarchy/prefs'
    themes = [('p2tha', '102030', 'E0E0E0'), ('p2thb', 'C0D0E0', '101010')]
    c.run('MakeDir >NIL: ' + 'ENVARC:Amarchy')
    c.run('MakeDir >NIL: ' + drawer)
    c.run('MakeDir >NIL: ENV:Amarchy')
    c.run('MakeDir >NIL: ENV:up-term')
    c.run('MakeDir >NIL: ENVARC:Claude')
    c.run('Delete >NIL: T:p2prefs.bak T:p2user.bak')
    had_prefs = c.run('Copy >NIL: %s T:p2prefs.bak' % prefs)[0] == 0
    had_user = c.run('Copy >NIL: ENV:up-term/keymap.user T:p2user.bak')[0] == 0
    seen = []
    try:
        c.run('Echo "rig-dummy-key" >ENVARC:Claude/key')
        for n, bg, fg in themes:
            with tempfile.NamedTemporaryFile('w', suffix='.theme', delete=False) as f:
                f.write('background = "#%s"\nforeground = "#%s"\naccent = "#FF8000"\nred = "#FF0000"\ngreen = "#00FF00"\n'
                        'yellow = "#FFFF00"\nblue = "#0000FF"\nmagenta = "#FF00FF"\ncyan = "#00FFFF"\n' % (bg, fg))
            ami.put(f.name, '%s/%s.theme' % (drawer, n))
        c.run('Echo "[bind]" >ENV:up-term/keymap.user')
        for k, (n, _, _) in zip(('f9', 'f10'), themes):
            c.run('Echo "super+shift+%s = gtheme:%s" >>ENV:up-term/keymap.user' % (k, n))
        t.send('AMARCHY', 'RELOAD', timeout=5)
        rows1 = kv(t.send('AMARCHY', 'STATUS', timeout=5)[1]).get('rows', 0)
        rows0 = rows1 - 2     # the two overlay rows are the last two installed rows
        c.run('Delete >NIL: ENV:up-term/keymap.user')
        t.send('AMARCHY', 'RELOAD', timeout=5)
        base = kv(t.send('AMARCHY', 'STATUS', timeout=5)[1]).get('rows', 0)
        if base != rows0:
            return False, 'keymap.user rows not loaded: rows with overlay %r, without %r' % (rows1, base)
        c.run('Echo "[bind]" >ENV:up-term/keymap.user')
        for k, (n, _, _) in zip(('f9', 'f10'), themes):
            c.run('Echo "super+shift+%s = gtheme:%s" >>ENV:up-term/keymap.user' % (k, n))
        t.send('AMARCHY', 'RELOAD', timeout=5)
        win = t.open_win()
        if not win:
            return False, 'no UP-Term window opened'
        time.sleep(3)
        tw.typ('/claude')
        tw.key(tw.RET, wait=1)
        if not t.wait_for(lambda: ami.window('Claude') is not None, 15):
            return False, 'no tab titled Claude'
        time.sleep(5)
        old = None
        for i, (n, bg, fg) in enumerate(themes):
            c.run('Delete >NIL: ENV:up-term/theme-active')
            rc = t.send('AMARCHY', 'RUN %d' % (rows0 + i), timeout=10)[0]
            made = t.wait_for(lambda: 'profile %s' % n in c.run('Type ENV:up-term/theme-active')[1], 8)
            time.sleep(4)
            hc = tabpix()
            tw.key(0x38, 0x40, wait=2)     # LAmiga+comma: previous tab = the plain shell
            hs = tabpix()
            tw.key(0x39, 0x40, wait=2)     # LAmiga+period: back to the Claude tab
            row = (n, rc, bool(made), hc.get(bg.lower(), 0), hs.get(bg.lower(), 0),
                   hc.get(old, 0) if old else 0, hs.get(old, 0) if old else 0)
            seen.append(row)
            if not (rc == 0 and made and row[3] > 50000 and row[4] > 50000 and row[5] == 0 and row[6] == 0):
                return False, 'switch to %s: (name, rc, sentinel, claude bg px, shell bg px, claude old px, shell old px) %r' % (n, seen)
            old = bg.lower()
        # owner 2026-10-29: a window opened AFTER the switch, with no colour in its spec, starts in the active Amarchy theme,
        # not the up-term profile's (apply_profile via upconf_colour_source). Its watcher only fires on a change, so these
        # pixels are the startup path. Measured on ITS box with it in front; closed by its own close gadget.
        n, bg, fg = themes[-1]
        title = 'p2new'
        c.run('Run >NIL: NewShell "XCON:0/14/640/242/%s/CLOSE"' % title)
        if not t.wait_for(lambda: ami.window(title) is not None, 15):
            return False, 'no new window opened after the switch, rows %r' % (seen,)
        time.sleep(4)
        ami.raise_window(title)
        time.sleep(1)
        box = ami.window(title)['box']
        w, h, rows = ami.grab(box)
        hn = {}
        for r in rows:
            for i in range(0, len(r) - 2, 3):
                k = bytes(r[i:i + 3]).hex()
                hn[k] = hn.get(k, 0) + 1
        top = sorted(hn.items(), key=lambda kv: -kv[1])[:3]
        gx, gy, gw, gh = ami.window(title)['sys:close']
        ami.click_px(gx + gw // 2, gy + gh // 2)
        t.wait_for(lambda: ami.window(title) is None, 8)
        seen.append(('newwin', top[0][0] if top else ''))
        if not (top and top[0][0] == bg.lower()):
            return False, 'new window after the switch to %s: box %r, top colours %r' % (n, box, top)
    finally:
        for n, _, _ in themes:
            c.run('Delete >NIL: %s/%s.theme' % (drawer, n))
        c.run('Delete >NIL: ENV:up-term/theme-active ENVARC:Claude/key')
        c.run('Delete >NIL: ' + prefs)
        if had_prefs:
            c.run('Copy >NIL: T:p2prefs.bak ' + prefs)
        c.run('Delete >NIL: ENV:up-term/keymap.user')
        if had_user:
            c.run('Copy >NIL: T:p2user.bak ENV:up-term/keymap.user')
        t.send('AMARCHY', 'RELOAD', timeout=5)
    return True, 'rows %d->%d; ' % (rows0, rows1) + '; '.join(
        ('%s: rc %s sentinel %s claude-bg %s shell-bg %s claude-old %s shell-old %s' % x if len(x) > 2 else 'new window dominant %s' % x[1]) for x in seen)


def nearest_pen(rgb):
    """The colour a 16 colour Workbench shows for rgb when no pen is free: the screen's palette entry nearest to it (the
    renderer's and console's rule, render/pen_rule.h: exact pen while one is free, else the nearest). Palette read live."""
    shutil.copyfile(pathlib.Path(__file__).resolve().parents[2] / 'build/amiga/dripens', paths.RIG / 'vtc' / 'dripens')
    pal = []
    for l in c.run('VTC:dripens Workbench')[1].splitlines():
        if l.startswith('pal:'):
            pal = [bytes.fromhex(x) for x in l.split()[1:]]
    want = bytes.fromhex(rgb)
    if want in pal:
        return rgb
    return min(pal, key=lambda p: sum((x - y) ** 2 for x, y in zip(p, want))).hex()


def xcon_top(title):
    """Open a console window with no colour in its spec, return its three most frequent pixel colours as [(hex, count)], close it"""
    c.run('Run >NIL: NewShell "XCON:0/14/640/242/%s/CLOSE"' % title)
    if not t.wait_for(lambda: ami.window(title) is not None, 15):
        return None
    time.sleep(4)
    ami.raise_window(title)
    time.sleep(1)
    w, h, rows = ami.grab(ami.window(title)['box'])
    hn = {}
    for r in rows:
        for i in range(0, len(r) - 2, 3):
            k = bytes(r[i:i + 3]).hex()
            hn[k] = hn.get(k, 0) + 1
    gx, gy, gw, gh = ami.window(title)['sys:close']
    ami.click_px(gx + gw // 2, gy + gh // 2)
    t.wait_for(lambda: ami.window(title) is None, 8)
    return sorted(hn.items(), key=lambda kv: -kv[1])[:3]


def s_themeboot():
    """F3-THEME-BOOT: on a fresh boot, before any theme switch, console windows start in the active Amarchy theme. The file
    ENV:up-term/theme-active is deleted, Amarchy restarted (amarchy.c main -> gt_dos_term_boot -> gt_dos_term_send), and the file must
    hold the theme's text (no ENVARC copy). The active theme here is the configured one, GT_DEFAULT tokyo-night when none
    is set (prefs deleted). A window opened after that, with no colour in its spec, must be dominated by that theme's bg"""
    import tempfile
    prefs = 'ENV:Amarchy/prefs'
    # tokyo-night (bg 1a1b26) cannot tell: on the 16 colour Workbench its nearest pen is 101010, which is also what a window
    # gets with no boot file (measured: control 101010). A theme file p2boot with bg 4473ff, a pen the Workbench has exactly,
    # gives an expected colour that differs from the control, so only a working boot file passes
    name, bg = 'p2boot', '4473ff'
    drawer = 'ENVARC:Amarchy/themes'
    c.run('MakeDir >NIL: ENV:Amarchy')
    c.run('MakeDir >NIL: ENV:up-term')
    c.run('Delete >NIL: T:p2prefs.bak')
    had_prefs = c.run('Copy >NIL: %s T:p2prefs.bak' % prefs)[0] == 0
    c.run('Delete >NIL: ' + prefs)
    c.run('MakeDir >NIL: ' + drawer)
    c.run('Echo "background = *"#%s*"" >%s/%s.theme' % (bg, drawer, name))
    c.run('Echo "foreground = *"#e0e0e0*"" >>%s/%s.theme' % (drawer, name))
    # the agenttab step leaves its last theme saved in ENVARC: (gt_dos_select); "none set" needs both copies gone
    c.run('Delete >NIL: T:p2arc.bak')
    had_arc = c.run('Copy >NIL: ENVARC:Amarchy/prefs T:p2arc.bak')[0] == 0
    c.run('Delete >NIL: ENVARC:Amarchy/prefs')
    title = 'p2boot'
    try:
        t.send('AMTILE', 'QUIT')
        t.send('AMARCHY', 'QUIT')
        time.sleep(1.5)
        c.run('Delete >NIL: ENV:up-term/theme-active')
        gone = c.run('Type ENV:up-term/theme-active')[0] != 0
        # control: the same window with no boot file, no prefs, Amarchy not running -> the default colour; the check
        # means something only when that colour is not the one the boot file must produce
        ctl = xcon_top('p2ctl')
        if not ctl or ctl[0][0] == nearest_pen(bg):
            return False, 'control window without the boot file already has the expected colour %s: pick another theme (%r)' % (nearest_pen(bg), ctl)
        c.run('Echo "amarchy-gui-theme = %s" >%s' % (name, prefs))
        c.run('Run >NIL: %s/Amarchy' % paths.UPTERM_BIN)
        t.wait_for(lambda: t.send('AMARCHY', 'STATUS', timeout=5)[0] == 0, 10)
        made = t.wait_for(lambda: 'profile %s' % name in c.run('Type ENV:up-term/theme-active')[1], 8)
        if not (gone and made):
            return False, 'file deleted before start %s, holds [profile %s] after start %s' % (gone, name, bool(made))
        top = xcon_top(title)
        want = nearest_pen(bg)
        if not (top and top[0][0] == want):
            return False, 'window after boot, theme %s: want the Workbench pen nearest %s = %s, top colours %r, control (no boot file) %r' % (
                name, bg, want, top, ctl)
        return True, 'file written at start; window %s dominant %s = pen nearest %s (control without the file: %s)' % (title, top[0][0], bg, ctl[0][0])
    finally:
        c.run('Delete >NIL: %s/%s.theme' % (drawer, name))
        c.run('Delete >NIL: ' + prefs)
        if had_prefs:
            c.run('Copy >NIL: T:p2prefs.bak ' + prefs)
        if had_arc:
            c.run('Copy >NIL: T:p2arc.bak ENVARC:Amarchy/prefs')


def wb_pal():
    """The live Workbench palette as a list of hex colours, one per pen (tests/amiga/dripens.c, the same read nearest_pen uses)."""
    shutil.copyfile(pathlib.Path(__file__).resolve().parents[2] / 'build/amiga/dripens', paths.RIG / 'vtc' / 'dripens')
    for l in c.run('VTC:dripens Workbench')[1].splitlines():
        if l.startswith('pal:'):
            return l.split()[1:]
    return []


def s_deskpreview():
    """DESK-PREVIEW-PALETTE: moving the highlight in the graphical theme picker previews the Workbench COLOURS on the Workbench
    screen (gt_dos_preview_send -> desk_preview); Esc puts the saved colours back. Pen 0 is read live before, after Down and after Esc.
    Covers the graphical picker only; the text picker (amarchy-menu) shares gt_dos_preview_send but is not driven here."""
    import amarchy_gui_rig as g
    if not g.open_menu():
        return False, 'Amarchy menu did not open'
    try:
        rows = g.rows_now()
        if not g.walk_to('Style', rows):
            return False, 'no Style row in %r' % rows
        rows = g.rows_now()
        if not g.walk_to('Amarchy theme', rows):
            return False, 'no Amarchy theme row in %r' % rows
        rows = g.rows_now()
        base = wb_pal()
        if not base:
            return False, 'no palette read'
        moved = []
        for _ in range(3):
            ami.key(g.DOWN)
            time.sleep(1.5)
            moved = wb_pal()
            if moved and moved[:8] != base[:8]:
                break
        changed = bool(moved) and moved[:8] != base[:8]
        for _ in range(3):
            ami.key(g.ESC)
            time.sleep(1)
        back = wb_pal()
        ok = changed and back[:8] == base[:8]
        return ok, 'rows %d, pens 0-7 before %s, after Down %s (changed %s), after Esc %s (restored %s)' % (
            len(rows), base[:8], moved[:8], changed, back[:8], back[:8] == base[:8])
    finally:
        for _ in range(4):
            ami.key(0x45)
            time.sleep(.5)


STEPS = [('start', s_start), ('tick_rises', s_tick_rises), ('bar_status', s_bar_status), ('bar_off', s_bar_off),
         ('bar_on', s_bar_on), ('status_keys', s_status_keys),
         ('clock', s_clock), ('ram', s_ram), ('cpu', s_cpu), ('columns', s_columns), ('network', s_network), ('screens', s_screens), ('system', s_system),
         ('bar_hold', s_bar_hold), ('bar_conf', s_bar_conf), ('tray', s_tray), ('recolour', s_recolour),
         ('agenttab', s_agenttab), ('themeboot', s_themeboot), ('deskpreview', s_deskpreview)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', choices=[n for n, _ in STEPS], help='run one step against the state left by earlier runs')
    ap.add_argument('--no-build', action='store_true', help='do not rebuild/stage Amarchy')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    verdict = OUT / 'verdict.json'
    done = json.loads(verdict.read_text()) if a.only and verdict.exists() else {}
    stage_rig.report(stage_rig.stage_build(names=('Amarchy', 'amarchy-gui'), build=not a.no_build))
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
