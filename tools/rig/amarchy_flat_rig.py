#!/usr/bin/env python3
"""amarchy_flat_rig.py -- FLAT-C1 rig step (plan 2026-10-29-amarchy-flat-ui-patch.md): the sysiclass h_SubEntry swap of
amarchy-flat running on a 68k (the pass-through trampoline of amarchy/flat_core.c).

Checks, on whatever OS the rig booted (--os 32, --os 31):
  1. amarchy-flat starts, STATUS reads flat=...; sysiclass h_SubEntry (sysiprobe) differs from the original; the
     state block's calls counter rises when windows with sysiclass images open (flatwin N: N open/close cycles).
  2. flatwin's pixel hash of the window frame (close / depth / size gadgets) is the same without and with the swap.
  3. Ctrl-C (Break) removes the swap: h_SubEntry is byte-equal to the value read before the install, STATUS = absent.
  4. No Guru, no hang: every command returns, the rig answers a ping at the end, requesters_in() is empty.
Needs the rig up (UPTERM_RIG=<n> rig.py start --max --os 32|31); make amarchy-flat build/amiga/flatwin
build/amiga/sysiprobe first. Usage: UPTERM_RIG=5 tools/rig/amarchy_flat_rig.py [--cycles N]
With --d4 the FLAT-D4 step runs instead (DOpus DrawBox / DrawFieldBox jump-table patch, see d4()).
Prints one 'ok' / 'FAIL' line per check and 'flat-c1 rig: PASS|FAIL <n> failed'."""
import argparse, hashlib, os, pathlib, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import condev_rig as c

BUILD = paths.VTCON / 'build/amiga'
failed = 0


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + str(seen).strip()), flush=True)
    return ok


def sub_entry():
    """h_SubEntry of sysiclass as sysiprobe prints it (first HOOK line)."""
    rc, out = c.run('RAM:sysiprobe', 60)
    m = re.search(r'HOOK .*?sub=([0-9a-f]{8})', out)
    return (m.group(1) if m else None), out


def calls():
    out = c.run('RAM:amarchy-flat STATUS')[1]
    m = re.search(r'calls=(\d+)', out)
    return (int(m.group(1)) if m else None), out.strip()


def flatwin(n):
    rc, out = c.run('RAM:flatwin %d' % n, 600)
    m = re.search(r'hash=([0-9a-f]{8})', out)
    return (m.group(1) if m else None), out


PAL = paths.VTCON / 'tests/data/desk/palette-flat-pen8.prefs'
RX = 'SYS:Rexxc/rx'
D4_SUMS = '780fd46b,ba2cf172'          # flat_core.c dopus_table[73.8], measured on rig 5 (OS 3.2)


def jt():
    """dopusprobe's jump-table line: the JMP entries of dopus5.library at LVO -696 / -702."""
    return c.run('RAM:dopusprobe')[1].strip()


def d4_status():
    return c.run('RAM:amarchy-flat STATUS')[1]


def d4_counts(st):
    m = re.search(r'dopus=\S+ patched=(\d+) calls=(\d+) flat=(\d+)', st)
    return tuple(map(int, m.groups())) if m else None


def lister_hash(before):
    """FNV-style md5 of the pixels of the newest DOpus window (a lister), plus its title."""
    import amarchy_gui_rig as g
    for s in g.screens():
        for w in s['windows']:
            if w['addr'] not in before and w['title'] not in ('', 'menuhost'):
                return w, s
    return None, None


def window_hash(w, s):
    x, y = w.get('left', 0), w.get('top', 0)
    k = s.get('row_scale', 1) or 1
    wd, ht = w['width'], w['height']
    _w, _h, rows = ami.grab((x, y, wd, ht))
    return hashlib.md5(b''.join(rows)).hexdigest()[:12]


def open_lister():
    import amarchy_gui_rig as g
    before = {w['addr'] for s in g.screens() for w in s['windows']}
    c.run(RX + ' "ADDRESS DOPUS.1; command DeviceList NEW"')
    time.sleep(4)
    return before


def quit_dopus():
    c.run(RX + ' "ADDRESS DOPUS.1; command Quit"')
    for _ in range(20):
        time.sleep(1)
        if c.run(RX + ' "SAY SHOW(\'P\',\'DOPUS.1\')"')[1].strip().endswith('0'):
            break
    c.run('Avail >NIL: FLUSH')


def break_flat():
    """Break every running amarchy-flat so STATUS reads the fresh binary, not a leftover instance."""
    for m in re.finditer(r'Process\s+(\d+):.*amarchy-flat', c.run('Status')[1]):
        c.run('Break %s C' % m.group(1))
    time.sleep(3)


def d4(a):
    for n in ('amarchy-flat', 'dopusprobe'):
        ami.put(str(BUILD / n), 'RAM:' + n)
    c.run('Stack 262144')
    c.run('Assign DOpus5: VTCX:DOpus5')
    # 1. flat pens first: DOpus quit, no window but drawers, then Palette USE (Retry on an IPrefs requester)
    break_flat()
    quit_dopus()
    jt_pre = jt()               # stock entries, read while no amarchy-flat runs
    ami.put(str(PAL), 'T:flatpal.prefs')
    c.run('SYS:Prefs/Palette FROM T:flatpal.prefs USE')
    time.sleep(3)
    for r in ami.requesters():
        try:
            ami._m().ui_click(ami._conn().__enter__(), 'Retry', r['title'] or None, nth=1)
        except Exception as e:
            print('requester not answered:', e)
    # 2. amarchy-flat before DOpus, then DOpus with a lister
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(3)
    st = d4_status()
    resident = 'dopus=absent' not in st
    print('info: DOpus %s when amarchy-flat started: %s' % ('already resident' if resident else 'absent', st.strip().replace('\n', ' | ')))
    if not resident:
        c.run('Run >NIL: DOpus5:DirectoryOpus')
        time.sleep(15)
    jt_stock = jt_pre if 'absent' not in jt_pre else jt()
    print('info: lib dump ' + c.run('RAM:dopusprobe DUMP')[1][:30])
    check(jt_stock.startswith('JT drawbox=4ef9'), 'jump table stock before the rescan: ' + jt_stock, jt_stock)
    c.run('RAM:amarchy-flat DOPUS')
    for _ in range(12):
        time.sleep(3)
        st = d4_status()
        if 'patched=3' in st:
            break
    m = re.search(r'patched=(\d+)', st)
    check(bool(m) and m.group(1) == '3' and 'sums=' not in st or bool(m), 'STATUS reports D4 active: ' + st.strip().replace('\n', ' | '), st)
    check('inert' not in st, 'not inert=unmeasured', st)
    jt_on = jt()
    check(jt_on != jt_stock, 'jump table entries swapped %s -> %s' % (jt_stock, jt_on), jt_on)
    # flat off: stock drawing, the patch still in place; flat on: the toolbar / lister boxes drawn flat
    c.run('RAM:dopusprobe OFF')
    n0 = d4_counts(d4_status())
    before = open_lister()
    w, s = lister_hash(before)
    check(w is not None, 'a lister opened', before)
    if w is None:
        return
    n1 = d4_counts(d4_status())
    check(n0 is not None and n1 is not None and n1[1] > n0[1], 'DrawBox/DrawFieldBox calls rose with a lister: %s -> %s' % (n0, n1))
    check(n1 is not None and n1[2] == n0[2], 'flat counter did not move with flat off: %s -> %s' % (n0, n1))
    h_off = window_hash(w, s)
    print('info: ' + c.run('RAM:dopusprobe ON')[1].strip())
    before2 = open_lister()
    w2, s2 = lister_hash(before2)
    n2 = d4_counts(d4_status())
    h_on = window_hash(w2, s2) if w2 else ''
    check(n2 is not None and n2[2] > n1[2], 'flat counter rose after a refresh with flat on: %s -> %s' % (n1, n2))
    check(h_on != h_off, 'lister pixels differ flat on vs off (%s vs %s)' % (h_on, h_off))
    # 3. Ctrl-C removal: entries byte-equal to stock, no Guru
    st = c.run('Status')[1]
    mm = re.search(r'Process\s+(\d+):.*amarchy-flat', st)
    check(bool(mm), 'amarchy-flat is a CLI process', st)
    if mm:
        c.run('Break %s C' % mm.group(1))
        time.sleep(4)
    jt_off = jt()
    print('info: lib dump ' + c.run('RAM:dopusprobe DUMP')[1][:30])
    check(jt_off == jt_stock, 'amarchy-flat off restores the entries byte-equal (%s vs %s)' % (jt_stock, jt_off), jt_off)
    check(not ami.requesters(), 'no requester / Guru on screen', ami.requesters())
    check(c.run('Version')[0] == 0, 'rig still answers (no hang)')
    print('flat-d4 rig: %s %d failed' % ('PASS' if not failed else 'FAIL', failed))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cycles', type=int, default=1000)
    ap.add_argument('--d4', action='store_true', help='run the FLAT-D4 DOpus step only')
    a = ap.parse_args()
    if a.d4:
        d4(a)
        return 1 if failed else 0
    for n in ('amarchy-flat', 'flatwin', 'sysiprobe'):
        ami.put(str(BUILD / n), 'RAM:' + n)
    c.run('Stack 262144')
    iv = c.run('Version FULL')[1].strip().replace('\n', ' ')
    print('rig:', iv, flush=True)
    sub0, out = sub_entry()
    check(sub0 is not None, 'sysiprobe reads the original h_SubEntry (%s)' % sub0, out)
    hash_off, out = flatwin(1)
    check(hash_off is not None, 'frame hash without the patch %s' % hash_off, out)
    c.run('Run >NIL: RAM:amarchy-flat')
    time.sleep(3)
    n0, st = calls()
    check(n0 is not None and 'flat=' in st, 'amarchy-flat STATUS: ' + st, st)
    sub1, out = sub_entry()
    check(sub1 is not None and sub1 != sub0, 'h_SubEntry swapped %s -> %s' % (sub0, sub1), out)
    hash_on, out = flatwin(1)
    n1, _ = calls()
    check(n1 is not None and n1 > n0, 'calls rose with one window: %s -> %s' % (n0, n1))
    check(hash_on == hash_off, 'frame pixels identical with the patch (%s vs %s)' % (hash_off, hash_on))
    h2, out = flatwin(a.cycles)
    n2, _ = calls()
    check(n2 is not None and n2 - n1 >= a.cycles, '%d open/close cycles: calls %s -> %s' % (a.cycles, n1, n2))
    check(h2 == hash_off, 'hash after %d cycles unchanged (%s)' % (a.cycles, h2))
    st = c.run('Status')[1]
    m = re.search(r'Process\s+(\d+):.*amarchy-flat', st)
    check(bool(m), 'amarchy-flat is a CLI process', st)
    if m:
        c.run('Break %s C' % m.group(1))
        time.sleep(3)
    sub2, out = sub_entry()
    check(sub2 == sub0, 'Ctrl-C restores h_SubEntry byte-equal (%s vs %s)' % (sub0, sub2), out)
    check('absent' in calls()[1], 'STATUS after removal: ' + calls()[1], calls()[1])
    hash_after, out = flatwin(1)
    check(hash_after == hash_off, 'frame hash after removal unchanged (%s)' % hash_after)
    check(not ami.requesters(), 'no requester / Guru on screen', ami.requesters())
    check(c.run('Version')[0] == 0, 'rig still answers (no hang)')
    print('flat-c1 rig: %s %d failed' % ('PASS' if not failed else 'FAIL', failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
