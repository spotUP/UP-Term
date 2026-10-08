#!/usr/bin/env python3
"""installer_rig.py -- REAL-INSTALLER-TEST: the kit's Install icon the way a
user starts it. The real Installer runs build/dist/UP-Term/Install (the
Installer script, dist/Install.installer) with the icon's tooltypes, visible
on the rig's screen; this script reads its pages through amiagent's UITREE
and clicks them, then checks what landed and runs Uninstall.

  default   AVERAGE (the icon's MINUSER/DEFUSER), every page answered with
            its default: the drawer SYS:UP-Term
  dest      AVERAGE, the drawer page given VTC:Apps/UP-Term (typed into the
            askdir string gadget)
  novice    NOVICE: no question asked, only the pages marked (all) -- the
            welcome and the summary -- and the default drawer

After each Install: UP-Term: names the drawer, ENVARC:up-term/Dir, the
marked UP-Term: block in S:User-Startup (before the GG: block), the files in
the drawer and in C:/L:/LIBS: (the original ixemul kept as .orig), every file
of the kit's nvim drawer copied (copyfiles (all) takes the subdrawers),
Python3: assigned, UPConsole STATUS prints the kit line. Then Uninstall:
S:User-Startup byte for byte as before, no UP-Term:, the drawer gone, the
original libraries back. (install_rig.py drives install.dos directly, with
the full set of checks; this one proves the Installer path reaches the same
result.)

Every page seen is logged to build/rig/installer/<case>.pages.txt (UITREE
text, for reading what the Installer showed); the verdict goes to
build/rig/installer/<case>.json with the fingerprint of the kit's scripts.
Resumable:
  installer_rig.py                the cases not yet passed with this kit
  installer_rig.py ONLY dest      one case (re-run even when it passed)
  installer_rig.py FAILED         only the cases that failed last time
  installer_rig.py FORCE          all, from zero
Needs `make dist` and the default rig (tools/rig/rig.py, the 3.1 machine
install_rig.py is written for; it is started fresh here). Not run by `make
test`: it needs the emulator.

Confirmed on the rig (2026-10-07, Installer 47.19): the Installer's custom gadgets have no labels, so pages
are driven by gadget id (90 Proceed, 91 Abort, 89 Make New Drawer, 92 the askdir string); the string gadget keeps
only a path that exists, so the dest drawer is made through Make New Drawer (dialog: string 1, OK 90, Cancel 91).
A page unchanged 60 s after a click is a FAIL (never a hang)."""
import hashlib, json, os, pathlib, re, shlex, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import install_rig as ir
from assign_rig import rig

ROOT = ir.ROOT
VTC = ir.VTC
OUT = paths.RIG / "installer"
OUT.mkdir(parents=True, exist_ok=True)  # pages are logged before the first verdict is written
KIT = ROOT / "build/dist/UP-Term"
CASES = {"default": ("AVERAGE", None), "dest": ("AVERAGE", "VTC:Apps/UP-Term"), "novice": ("NOVICE", None)}
RAMIGA = 0x0080
KEY_X, KEY_RETURN = 0x32, 0x44


def fingerprint():
    h = hashlib.sha256()
    for p in ("dist/Install.installer", "dist/install.dos", "dist/Uninstall", "build/dist/UP-Term/Install",
              "build/dist/UP-Term/Files/install.dos"):
        f = ROOT / p
        h.update(f.read_bytes() if f.exists() else b"missing " + p.encode())
    return h.hexdigest()[:16]


def verdict(case):
    f = OUT / (case + ".json")
    return json.loads(f.read_text()) if f.exists() else None


def record(case, ok, detail):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (case + ".json")).write_text(json.dumps(
        {"case": case, "verdict": "pass" if ok else "fail", "fp": fingerprint(), "detail": detail, "time": time.time()}))
    print("[%s] %s %s" % ("PASS" if ok else "FAIL", case, ("" if ok else detail[-400:])))


class Checks:
    def __init__(self):
        self.failed, self.total = [], 0

    def __call__(self, ok, what, seen=''):
        self.total += 1
        if not ok:
            self.failed.append('%s: %s' % (what, str(seen).strip()[-200:]))
        print('%s %d %s' % ('ok' if ok else 'FAIL', self.total, what))


def windows(tree):
    """UITREE as [(title, [(label, (x, y, w, h))])] for the front screen's windows."""
    out = []
    for line in tree.splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            out.append((f[-1], []))
        elif line.startswith('G ') and out:
            f = line.split(None, 5)
            w, h = map(int, f[4].split('x'))
            out[-1][1].append(((f[5] if len(f) > 5 else '').strip().strip('"'), (int(f[2]), int(f[3]), w, h)))
    return out


def click(box):
    # UITREE gives screen pixels, CLICK takes Intuition pointer units: the same on the 3.1 rig's RTG screen,
    # y doubled on 3.2's native non-interlaced Workbench (a raw click at Proceed's y 191 landed at y 95, in
    # the page's text: the 3.2 stall of 2026-10-08). ami.click_px scales.
    x, y, w, h = box
    ami.click_px(x + w // 2, y + h // 2)


def installer_running(inst):
    # Status COM matches the command as it was started (the full path when one was given)
    try:
        return ir.run('Status >NIL: COM "%s"' % inst)[0] == 0
    except SystemExit as e:
        if 'still running' in str(e):   # the agent is busy with an earlier command: the Installer is not gone
            return True
        raise


def find_installer():
    for p in ('Installer', 'SYS:System/Installer', 'SYS:Utilities/Installer', 'C:Installer', 'SYS:C/Installer'):
        if ir.run('Which >NIL: "%s"' % p)[0] == 0 or ir.run('List >NIL: "%s"' % p)[0] == 0:
            return p
    return None


def gadget_text(gid):
    """The string gadget `gid`'s text in the Installer's window, from UITREE (G id x y WxH string state "" "text")."""
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        f = line.split(None, 2)
        if line.startswith('G ') and f[1] == str(gid):
            return line.rsplit('"', 2)[-2] if line.count('"') >= 4 else ''
    return None


def type_into(box, gid, text, tries=6):
    """Click string gadget `gid`, clear it (RAmiga-X, retried: the qualified key is sometimes lost under load and
    then types an X), type text, Return. True when UITREE shows the text typed, before Return."""
    for _ in range(tries):
        click(box)
        time.sleep(1)
        for _ in range(tries):
            ami.key(KEY_X, RAMIGA)
            time.sleep(0.5)
            if gadget_text(gid) == '':
                break
            if 'X' in (gadget_text(gid) or ''):
                pass
        if gadget_text(gid) != '':
            continue
        ami.req(0x08, bytes([4]) + text.encode('latin-1'))
        time.sleep(0.5)
        if gadget_text(gid) == text:
            ami.key(KEY_RETURN)
            time.sleep(1.5)
            return True
    return False


def gadgets(tree):
    """UITREE as {window title: {gadget id: ((x, y, w, h), label)}}."""
    out, cur = {}, None
    for line in tree.splitlines():
        if line.startswith('W '):
            cur = out.setdefault(shlex.split(line)[-1], {})
        elif line.startswith('G ') and cur is not None:
            f = line.split(None, 5)
            w, h = map(int, f[4].split('x'))
            cur[int(f[1])] = ((int(f[2]), int(f[3]), w, h), (f[5] if len(f) > 5 else '').strip().strip('"'))
    return out


def installer_page(tree):
    """(title, {id: (box, label)}) of the Installer's window: the one with Proceed (90) or Abort Install (91)."""
    for title, gads in gadgets(tree).items():
        if 90 in gads or 91 in gads:
            return title, gads
    return None, {}


def text_faults(tree):
    """Text of the Installer's page that the user cannot read whole, from the screen's pixels (UITREE has
    no text): ([fault], [text band]). Text is the black pen with the rules taken out (a run of more than 30
    pixels across, or 16 or more down) and the gadgets' own boxes left out. Faults: text in the bottom
    strip of the window (a line cut by its border: the welcome page's last line on 3.2's 560x176 window,
    2026-10-08), text under the button row, text beside copyfiles' panel (P_COPY's lines peeking out)."""
    box, gads = None, []
    for line in tree.splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            cur = f[-1].startswith('VTC:distkit/Install')
            if cur:
                w, h = map(int, f[4].split('x'))
                box = (int(f[2]), int(f[3]), w, h)
        elif line.startswith('G ') and box and cur and 'sys:' not in line:
            f = line.split()
            gw, gh = map(int, f[4].split('x'))
            gads.append((int(f[2]), int(f[3]), gw, gh))
    if not box:
        return [], []
    x0, y0, w, h = box
    W, H, rows = ami.grab()
    px, cols = set(), {}
    for y in range(y0 + 12, min(H, y0 + h)):
        r = rows[y]
        xs = [x for x in range(x0 + 5, min(W, x0 + w - 5)) if r[x * 3:x * 3 + 3] == b'\0\0\0']
        run = [xs[0]] if xs else []
        for x in xs[1:] + [None]:
            if x is not None and x == run[-1] + 1:
                run.append(x)
                continue
            if len(run) <= 30:
                px.update((xx, y) for xx in run)
            else:
                cols.setdefault('h', []).append((y, run[0], run[-1]))
            run = [x] if x is not None else []
    vert = {}
    for x, y in px:
        vert.setdefault(x, []).append(y)
    vrules = []
    for x, ys in vert.items():
        ys.sort()
        run = [ys[0]]
        for y in ys[1:] + [None]:
            if y is not None and y == run[-1] + 1:
                run.append(y)
                continue
            if len(run) >= 16:
                px.difference_update((x, yy) for yy in run)
                vrules.append((x, run[0], run[-1]))
            run = [y] if y is not None else []
    px = {(x, y) for x, y in px
          if not any(gx - 1 <= x <= gx + gw and gy - 1 <= y <= gy + gh for gx, gy, gw, gh in gads)}
    faults = []
    low = [y for y in (y for _, y in px) if y >= y0 + h - 6]
    if low:
        faults.append('text in the bottom border strip (y %d..%d)' % (min(low) - y0, max(low) - y0))
    btop = min((gy for gx, gy, gw, gh in gads if gy + gh >= y0 + h - 40 and gh >= 12), default=None)
    if btop is not None:
        under = sorted(y for _, y in px if btop <= y < y0 + h - 6)
        if under:
            faults.append('text under the button row (y %d..%d)' % (under[0] - y0, under[-1] - y0))
    for vx, vt, vb in vrules:   # copyfiles' panel: its right edge runs >= 100 lines, its bottom rule ends there
        if vb - vt < 100:
            continue
        left = min((a for y, a, z in cols.get('h', []) if vb - 1 <= y <= vb + 2 and abs(z - vx) <= 2), default=None)
        if left is None:
            continue
        side = sorted((x, y) for x, y in px if vt <= y <= vb and (x < left - 1 or x > vx + 1))
        if side:
            faults.append('text beside the copy panel (x %s, y %d..%d)' % (
                ' '.join(sorted({str(x - x0) for x, _ in side})[:6]), min(y for _, y in side) - y0,
                max(y for _, y in side) - y0))
    bands, cur = [], None
    for y in sorted({y for _, y in px}):
        xs = [x for x, yy in px if yy == y]
        if cur and cur[1] >= y - 1:
            cur[1], cur[2], cur[3] = y, min(cur[2], min(xs)), max(cur[3], max(xs))
        else:
            cur = [y, y, min(xs), max(xs)]
            bands.append(cur)
    return faults, ['y%d-%d x%d-%d' % (a - y0, b - y0, c - x0, d - x0) for a, b, c, d in bands]


def drive(case, inst, dest, baseline_titles, timeout=5400, stall=60, work_limit=2700, poll=20):
    """Answer the Installer's pages by gadget id (its custom gadgets have no labels): 90 Proceed, 91 Abort
    (never), 89 Make New Drawer (never), 1 on the install-mode page Install for Real, 92 the askdir string.
    A page unchanged `stall` s after a click is a FAIL; a page without Proceed is the copy: waited out
    (percent logged) up to work_limit s. Returns (finished, log)."""
    log = []
    pages = OUT / (case + ".pages.txt")
    pages.write_text('')
    t0 = time.time()
    last_sig, clicked_at, retried, typed, chose = None, None, None, False, set()
    work_since, last_pct, dumped_pct = None, None, set()

    seen_faults = set()

    def dump(tree, why):
        faults, bands = text_faults(tree)
        with pages.open('a') as f:
            f.write('---- %.0f s %s\n%s\ntext: %s\n' % (time.time() - t0, why, tree, ' '.join(bands)))
            for x in faults:
                f.write('LAYOUT: %s\n' % x)
        for x in faults:
            if x not in seen_faults:
                seen_faults.add(x)
                log.append('LAYOUT %s on [%s]' % (x, installer_page(tree)[0]))

    while time.time() - t0 < timeout:
        if not installer_running(inst):
            return True, log
        tree = ami.req(0x0D).decode('latin-1')
        if any(t in ('System Request', 'Volume Request') for t, _ in windows(tree)):
            log.append('a system requester came up')
            dump(tree, 'requester')
            return False, log
        title, gads = installer_page(tree)
        sig = (title, tuple(sorted(gads)), tuple(l for _, l in gads.values()))
        if sig != last_sig:
            dump(tree, 'page')
            log.append('page [%s] ids %s' % (title, ' '.join(map(str, sorted(gads)))))
            last_sig, clicked_at, retried = sig, None, None
        m = re.findall(r'(\d+)% done', tree)
        if m and m[-1] != last_pct:
            last_pct = m[-1]
            log.append('%s%% done at %.0f s' % (last_pct, time.time() - t0))
        if 90 not in gads:
            # copying or waiting for the next page: only Abort (or nothing) on it
            if m and m[-1] == last_pct and last_pct not in dumped_pct:
                dumped_pct.add(last_pct)
                dump(tree, 'work %s%%' % last_pct)
            work_since = work_since or time.time()
            if time.time() - work_since > work_limit:
                log.append('no Proceed page for %d s: %s' % (work_limit, last_sig))
                return False, log
            time.sleep(poll)
            continue
        work_since = None
        if clicked_at and time.time() - clicked_at > stall:
            log.append('STALL: page unchanged %d s after a click, ids %s' % (stall, sorted(gads)))
            dump(tree, 'stall')
            return False, log
        if clicked_at:
            if time.time() - (retried or clicked_at) > 15:   # a click made while the page still redraws is lost
                click(gads[90][0])
                retried = time.time()
                log.append('Proceed (id 90) again: page unchanged 15 s')
            time.sleep(3)
            continue
        if dest and not typed and 89 in gads and 92 in gads:   # askdir page
            # The string gadget only keeps a path that exists; a new drawer is made through Make New Drawer
            # (89), whose dialog takes the whole path in its string gadget 1 (OK 90, Cancel 91).
            typed = True
            click(gads[89][0])
            dlg = None
            for _ in range(10):
                time.sleep(1)
                t2 = ami.req(0x0D).decode('latin-1')
                dlg = installer_page(t2)[1]
                if 1 in dlg and 89 not in dlg:
                    break
            else:
                log.append('Make New Drawer dialog did not come up')
                dump(t2, 'no dialog')
                return False, log
            dump(t2, 'dialog')
            if not type_into(dlg[1][0], 1, dest):
                log.append('could not type %s into the Make New Drawer dialog' % dest)
                return False, log
            click(dlg[90][0])
            time.sleep(3)
            t2 = ami.req(0x0D).decode('latin-1')
            got = gadget_text(92)
            log.append('made %s through Make New Drawer, askdir string shows [%s]' % (dest, got))
            dump(t2, 'after Make New Drawer')
            if got != dest:
                return False, log
            last_sig = None   # the page is redrawn: log it and go on to Proceed
            continue
        if 1 in gads and 5 in gads and title not in chose:   # install-mode page: Install for Real
            click(gads[1][0])
            chose.add(title)
            log.append('chose Install for Real (id 1)')
            time.sleep(1)
        click(gads[90][0])
        clicked_at = time.time()
        log.append('Proceed (id 90)')
        time.sleep(3)
    log.append('the Installer did not finish in %d s' % timeout)
    return False, log


def run_case(case):
    level, dest = CASES[case]
    drawer = dest or "SYS:UP-Term"
    ck = Checks()
    ir.prepare(dest)
    ir.run_long('rununinstall')  # a run that stopped half-way left things behind
    ir.run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote QUIET')
    if dest:
        ir.run('MakeDir >NIL: %s' % dest.rsplit('/', 1)[0])   # the new drawer's parent must exist
        ir.run_slow('deletedest', 'Delete "%s" ALL QUIET' % dest)
    startup_before = ir.run('Type S:User-Startup')[1]
    before = ir.lib_state()
    rig_gg = ir.run('Assign >NIL: GG: EXISTS')[0] == 0
    if rig_gg:
        ir.run('Assign GG:')   # as install_rig: Install's own GG: is what gets tested
    inst = find_installer()
    if not inst:
        return False, 'no Installer on the rig (Which Installer, SYS:System, SYS:Utilities, C:)'
    base = {t for t, _ in windows(ami.req(0x0D).decode('latin-1'))}
    # the icon: default tool Installer, tooltypes APPNAME=UP-Term MINUSER=AVERAGE DEFUSER=AVERAGE (Makefile dist)
    ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER %s DEFUSER %s NOLOG' % (inst, level, level), 30)
    time.sleep(5)
    finished, log = drive(case, inst, dest, base)
    (OUT / (case + ".log.txt")).write_text('\n'.join(log) + '\n')
    ck(finished, 'the Installer ran to its end (%s)' % level, log[-1] if log else '')
    lay = [l for l in log if l.startswith('LAYOUT')]
    ck(not lay, 'every page\'s text readable: no line cut by the border, under the buttons or beside the copy panel',
       '; '.join(lay[:4]))
    if not finished:
        if installer_running(inst):
            ir.run('Break >NIL: `Status COM "%s"` C' % inst)
        return False, '; '.join(ck.failed + log[-3:])
    # what the Installer's install.dos parts and copyfiles put there
    rc, out = ir.run('Assign LIST')
    al = [l for l in out.splitlines() if l.lower().startswith('up-term')]
    ck(al and al[0].split(None, 1)[-1].strip().lower().endswith(drawer.split(':', 1)[1].lower()),
       'UP-Term: is assigned to %s' % drawer, out)
    rc, out = ir.run('Type ENVARC:up-term/Dir')
    ck(rc == 0 and out.strip().lower() == drawer.lower(), 'ENVARC:up-term/Dir names %s' % drawer, out)
    rc, out = ir.run('Type S:User-Startup')
    ck(';BEGIN UP-Term assign' in out and ';END UP-Term assign' in out, 'the UP-Term: block is marked', out[-300:])
    # an AmigaDOS $name ends at the first non-alphanumeric: $o_dest went in
    # literally and a Replay's boot failed on it (2026-10-07)
    added = [l for l in out.splitlines() if l not in startup_before.splitlines()]
    ck(not any(re.search(r'\$[A-Za-z0-9_]', l) for l in added),
       'S:User-Startup gets no unexpanded $variable', '\n'.join(l for l in added if '$' in l))
    ck(('Assign UP-Term: "%s"' % drawer).lower() in out.lower(), 'S:User-Startup assigns UP-Term: to %s' % drawer, out[-300:])
    ck(out.find('Assign UP-Term:') >= 0 and (out.find('Assign GG:') < 0 or out.find('Assign UP-Term:') < out.find('Assign GG:')),
       'the UP-Term: block comes before the blocks that use it', out[-300:])
    if dest:
        ck(ir.run('List >NIL: SYS:UP-Term')[0] != 0, 'a chosen drawer leaves SYS:UP-Term unmade')
    for f in ('"%s/VERSIONS"' % drawer, 'UP-Term:bin/sh', 'UP-Term:bin/ls', 'UP-Term:unifont/00', 'UP-Term:emoji/1F6',
              'C:vsh', 'C:tmux', 'C:UPConsole', 'L:vtcon-handler', 'L:pty-handler', 'DEVS:DOSDrivers/XCON',
              'SYS:System/UP-Term.info', 'SYS:Prefs/UP-Term-Prefs.info', 'ENVARC:up-term/unstartup.sh'):
        ck(ir.run('List >NIL: %s' % f)[0] == 0, '%s is there' % f)
    st = ir.lib_state()
    ck(st.get('ixemul.library.orig') == before.get('ixemul.library'), 'the original ixemul kept as .orig', str(st))
    ck(ir.run('Search >NIL: LIBS:ixemul.library UP-Term')[0] == 0, 'the patched ixemul is in LIBS:')
    ck(ir.run('Assign >NIL: PTY: EXISTS DEVICES')[0] == 0, 'PTY: is mounted')
    rc, out = ir.run('C:UPConsole STATUS')
    ck('kit:' in out, 'UPConsole STATUS prints the kit line (UP-Term:VERSIONS)', out)
    ck(ir.run('Assign >NIL: Python3: EXISTS')[0] == 0 and
       ir.run('Search >NIL: S:User-Startup "Assign Python3: UP-Term:Python3"')[0] == 0,
       'Python3: assigned, and at every boot')
    want = sum(1 for p in (KIT / "Files/nvim").rglob('*') if p.is_file())
    rc, out = ir.run_slow('nvimcount', 'List UP-Term:nvim ALL FILES LFORMAT "%N"', 900)
    got = len([l for l in out.splitlines() if l.strip()])
    ck(got == want, 'copyfiles copied every file of the nvim drawer, subdrawers too (%d of %d)' % (got, want))
    rc, out = ir.run_slow('pyrun', 'Stack 1000000\nUP-Term:Python3/bin/python3 -c "print(6*7)"', 400)
    ck(out.strip().splitlines()[-1:] == ['42'], 'python3 runs from the Installer\'s copy', out[-200:])
    # Uninstall gives it all back
    rc, out = ir.run_long('rununinstall')
    ck(rc == 0, 'Uninstall runs', out)
    rc, out = ir.run('Type S:User-Startup')
    ck(out == startup_before, 'after Uninstall: S:User-Startup as it was, byte for byte',
       'differs: %r vs %r' % (out[-80:], startup_before[-80:]))
    ck(ir.run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'after Uninstall: no UP-Term: assign')
    ck(ir.run('List >NIL: "%s"' % drawer)[0] != 0, 'after Uninstall: %s is gone' % drawer)
    st = ir.lib_state()
    ck(st.get('ixemul.library') == before.get('ixemul.library') and 'ixemul.library.orig' not in st,
       'after Uninstall: the original ixemul back, no .orig', str(st))
    ck(st.get('ixnet.library') == before.get('ixnet.library') and 'ixnet.library.orig' not in st,
       'after Uninstall: the original ixnet back, no .orig', str(st))
    ir.run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote QUIET')
    if rig_gg:
        ir.run('Assign GG: VTC:gg')
    import rig as rigmod
    ir.run('Avail >NIL: FLUSH')
    for line in rigmod.RIG_LIBS:
        ir.run(line)
    print('installer_rig %s: passed %d of %d' % (case, ck.total - len(ck.failed), ck.total))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


def main():
    fx = ir.Fixtures(('ENVARC:up-term/Dir',))
    try:
        return _main(fx)
    finally:
        fx.restore()   # a killed or failing run leaves nothing planted


def _main(fx):
    args = sys.argv[1:]
    only = args[args.index("ONLY") + 1] if "ONLY" in args else None
    if only and only not in CASES:
        raise SystemExit('cases: ' + ' '.join(CASES))
    todo = [only] if only else list(CASES)
    if not (KIT / "Install").exists():
        raise SystemExit('no kit: make dist first')
    fp = fingerprint()
    booted = False
    for case in todo:
        v = verdict(case)
        if not only and "FORCE" not in args and v and v["verdict"] == "pass" and v["fp"] == fp:
            print("[SKIP] %s passed with this kit" % case)
            continue
        if "FAILED" in args and not (v and v["verdict"] == "fail"):
            continue
        if not booted:
            rig("stop")
            rig("start")
            fx.take()   # the user's files and assigns, before any case plants its own
            booted = True
        try:
            ok, detail = run_case(case)
        except SystemExit as e:  # the agent or the rig is gone: not a verdict on the kit
            print("[ABORT] %s: %s (no verdict written)" % (case, e))
            return 2
        record(case, ok, detail)
    bad = [c for c in todo if (verdict(c) or {}).get("verdict") == "fail"]
    print("installer_rig: %d of %d cases failed: %s" % (len(bad), len(todo), " ".join(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
