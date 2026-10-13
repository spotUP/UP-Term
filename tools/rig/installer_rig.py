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
  space     AVERAGE on a System: partition made too full for the default choices (K7: a filler file leaves
            about 12 MB; the kit needs more): the drawer page is answered by default, and the room message
            (the choices need N KB, the drawer's disk has M KB; Proceed asks for another drawer) comes up
            before anything is copied. The case Aborts on the drawer page that follows it, and checks that
            nothing was installed. Every page is saved as build/<rig>/installer/space-<n>.png to read.
  novice    NOVICE: no question asked, only the pages marked (all) -- the
            welcome and the summary -- and the default drawer
  update    AVERAGE as default, then the kit with one file changed
            (Files/unifont/SOURCE.txt, its MANIFEST made again) and Install
            again: the first page offers Update (the default), and after it
            only that file's date in UP-Term: has changed, UP-Term:MANIFEST is
            the new kit's, S:User-Startup and the recorded options are as
            they were; both times are logged (installer update plan U8)

After each Install: UP-Term: names the drawer, ENVARC:up-term/Dir, the
marked UP-Term: block in S:User-Startup (before the GG: block), the files in
the drawer and in C:/L:/LIBS: (the original ixemul kept as .orig), every file
of the kit's unifont drawer copied (copyfiles (all) takes the subdrawers),
UPConsole STATUS prints the kit line. Then Uninstall:
S:User-Startup byte for byte as before, no UP-Term:, the drawer gone, the
original libraries back. (install_rig.py drives install.dos directly, with
the full set of checks; this one proves the Installer path reaches the same
result.)

  addon-<id>  (one per add-on of dist/packages.txt, K2-11) the base by its Installer, then the add-on's own
            Install icon, both AVERAGE with every page answered by its default; the add-on's files, its
            record and its program answering; its Uninstall; the base Uninstall
  migrate     the single kit of 0cd6f0b (LEGACY_KIT) by its Installer, then the new base's Install over it
            (the Update path: no drawer page, an edit planted in an unchanged file survives, the handed
            nvim keeps its date, UP-Term:pkg/legacy is kept), then the nvim add-on's Install (Update from
            "installed by the single kit", the planted edit survives, the changed files are the new ones);
            the base Uninstall takes everything away

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
import os, pathlib, re, shlex, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import verdicts
import install_rig as ir
from assign_rig import rig

ROOT = ir.ROOT
VTC = ir.VTC
OUT = paths.RIG / "installer"
OUT.mkdir(parents=True, exist_ok=True)  # pages are logged before the first verdict is written
KIT = ir.KIT
# a clean image (--os) has a 12 MB system partition: the kit goes to its Work: drive
CLEAN = "--os" in os.environ.get("UPTERM_RIG_FLAGS", "")
WORKDEST = "Work:Apps/UP-Term"
CASES = {"default": ("AVERAGE", None), "dest": ("AVERAGE", WORKDEST if CLEAN else "VTC:Apps/UP-Term"),
         "novice": ("NOVICE", None), "update": ("AVERAGE", WORKDEST if CLEAN else None), "space": ("AVERAGE", None)}
for _pkg in ir.addon_ids():
    CASES["addon-" + _pkg] = ("AVERAGE", None)
CASES["migrate"] = ("AVERAGE", None)
UPDATED = "Files/unifont/SOURCE.txt"   # the update case changes this file of a copy part (copy-unifont)
KITWINDOW = re.compile(r'VTC:(distkit|kit-\w+|legacykit)/Install')   # the Installer window's title: the kit it runs
RAMIGA = 0x0080
KEY_X, KEY_RETURN = 0x32, 0x44


def case_packages(case):
    """([packages the case installs], [more files its fingerprint reads])."""
    if case and case.startswith("addon-"):
        return ["base", case[len("addon-"):]], []
    if case == "migrate":
        return ["base", "nvim"], [ir.LEGACY_KIT / "Files/MANIFEST", ir.LEGACY_KIT / "Install"]
    return ["base"], []


def fingerprint(case=None):
    pk, extra = case_packages(case)
    built = [f for p in pk for f in (ir.kit_dir(p) / "Install", ir.kit_dir(p) / "Files/install.dos")]
    return verdicts.fingerprint(ir.kit_fingerprint(*pk).encode(), *built, *extra)


STORE = verdicts.Store(OUT)
verdict, record_verdict = STORE.verdict, STORE.record


def record(case, ok, detail):
    STORE.record(case, ok, detail, fingerprint(case))


Checks = verdicts.Checks


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
            cur = bool(KITWINDOW.match(f[-1]))
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


def drive(case, inst, dest, baseline_titles, timeout=5400, stall=60, work_limit=2700, poll=20, expect_abort=False):
    """Answer the Installer's pages by gadget id (its custom gadgets have no labels): 90 Proceed, 91 Abort
    (never), 89 Make New Drawer (never), 1 on the install-mode page Install for Real, 92 the askdir string.
    A page unchanged `stall` s after a click is a FAIL; a page without Proceed is the copy: waited out
    (percent logged) up to work_limit s. Returns (finished, log)."""
    log = []
    pages = OUT / (case + ".pages.txt")
    pages.write_text('')
    t0 = time.time()
    last_sig, clicked_at, retried, typed, chose = None, None, None, False, set()
    askdirs, shots = 0, 0
    last_page_at = time.time()
    work_since, last_pct, dumped_pct = None, None, set()

    seen_faults = set()

    def dump(tree, why):
        # the Make New Drawer dialog is the Installer's own, drawn over the askdir page (its frames read as a
        # panel, the drawer list under it as stray text: 3.1 dest, 2026-10-08): only our pages' text is judged
        own = why in ('dialog', 'no dialog', 'after Make New Drawer')
        faults, bands = ([], []) if own else text_faults(tree)
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
            last_page_at = time.time()
            if expect_abort:
                # K7: every page as an image (to read the room message), and the drawer page counted: a second
                # one is the room message's Proceed (ask for another drawer)
                shots += 1
                w, h, rows = ami.grab()
                ami.png(str(OUT / ('%s-%02d.png' % (case, shots))), w, h, rows)
                if 89 in gads and 92 in gads:
                    askdirs += 1
        m = re.findall(r'(\d+)% done', tree)
        if m and m[-1] != last_pct:
            last_pct = m[-1]
            log.append('%s%% done at %.0f s' % (last_pct, time.time() - t0))
        if expect_abort and askdirs >= 2 and 91 in gads:
            # the room message was answered with Proceed and the drawer page came back: Abort Install ends it
            log.append('room message seen: the drawer page came a second time (page %d); Abort Install (id 91)' % shots)
            click(gads[91][0])
            time.sleep(5)
            continue
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


def dates():
    """{file: 'date time'} of everything in UP-Term: and of the files Install puts elsewhere."""
    out = ir.run_slow('dates', 'List UP-Term: ALL FILES LFORMAT "%P%N|%D %T"', 900)[1]
    out += ir.run('List C:vsh C:UPConsole L:vtcon-handler L:pty-handler LIBS:ixemul.library LIBS:ixnet.library '
                  'DEVS:DOSDrivers/XCON S:User-Startup LFORMAT "%P%N|%D %T"')[1]
    return dict(l.rsplit('|', 1) for l in out.splitlines() if '|' in l)


def update_phase(ck, inst, level, base, t_install):
    """The update case's second Install: the kit with UPDATED changed, Update chosen (the default)."""
    import subprocess
    before, startup = dates(), ir.run('Type S:User-Startup')[1]
    opts = ir.run('List ENVARC:up-term/opts LFORMAT "%N"')[1]
    f = VTC / "distkit" / UPDATED
    f.write_bytes(f.read_bytes() + b"\n(installer_rig update case)\n")
    subprocess.run([sys.executable, str(ROOT / "tools/mkmanifest.py"), str(VTC / "distkit")], check=True)
    t = time.time()
    ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER %s DEFUSER %s NOLOG' % (inst, level, level), 30)
    time.sleep(5)
    finished, log = drive('update-2', inst, None, base)
    t = time.time() - t
    (OUT / "update-2.log.txt").write_text('\n'.join(log) + '\n')
    print('installer_rig update: full install %.0f s, update %.0f s' % (t_install, t))
    ck(finished, 'the Update ran to its end (%.0f s; the full install %.0f s)' % (t, t_install), log[-1] if log else '')
    after = dates()
    changed = sorted(k for k in set(before) | set(after)
                     if before.get(k) != after.get(k) and k.lower() != 'up-term:manifest')
    ck(changed == ['UP-Term:unifont/SOURCE.txt'], 'the Update copied only the changed file', ' '.join(changed[:8]))
    got = ami.read_file('UP-Term:' + UPDATED.split('/', 1)[1]) or b''
    ck(got.endswith(b"(installer_rig update case)\n"), 'the changed file is the new kit\'s')
    ck(ami.read_file('UP-Term:MANIFEST') == (VTC / "distkit/Files/MANIFEST").read_bytes(),
       'UP-Term:MANIFEST is the new kit\'s (the record part ran)')
    ck(ir.run('Type S:User-Startup')[1] == startup, 'S:User-Startup as it was')
    ck(ir.run('List ENVARC:up-term/opts LFORMAT "%N"')[1] == opts, 'the recorded options as they were', opts)
    lay = [l for l in log if l.startswith('LAYOUT')]
    ck(not lay, 'the Update\'s pages readable', '; '.join(lay[:4]))
    pct = [int(x) for l in log for x in re.findall(r'(\d+)% done', l)]
    ck(pct and max(pct) <= 100, 'the bar counts only the Update\'s parts (at most 100%%: %s)' % (max(pct) if pct else '-'))


SPACE_LEFT = 12 * 1024 * 1024    # what the space case leaves free on SYS:
FILLER = 'SYS:space-filler'


def sys_free_bytes():
    """The free bytes of SYS: as the rig's Info prints them (blocks of 512)."""
    out = ir.run('Info')[1]
    for line in out.splitlines():
        f = line.split()
        if len(f) > 5 and f[0].upper() == 'DH0:':
            return int(f[3]) * 512
    raise SystemExit('installer_rig: cannot read the free space of SYS: from Info: %r' % out)


def fill_sys():
    """A file on SYS: that leaves SPACE_LEFT free (the kit's default choices need more): written on the host in VTC: and
    copied, then the host copy removed."""
    ir.run('Delete >NIL: %s QUIET' % FILLER)
    n = sys_free_bytes() - SPACE_LEFT
    if n <= 0:
        return
    host = VTC / 'space-filler'
    with open(host, 'wb') as f:
        f.truncate(n)
    try:
        ir.run_slow('fillsys', 'Copy VTC:space-filler %s' % FILLER, 900)
    finally:
        host.unlink(missing_ok=True)
    print('installer_rig space: SYS: has %d KB free after the filler' % (sys_free_bytes() // 1024))


def run_case(case):
    if case == "space":
        try:
            fill_sys()
            return run_base_case(case)
        finally:
            ir.run_slow('unfillsys', 'Delete >NIL: %s QUIET' % FILLER, 300)
    if case.startswith("addon-"):
        return run_addon_case(case[len("addon-"):])
    if case == "migrate":
        return run_migrate()
    return run_base_case(case)


def run_base_case(case):
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
    t_install = time.time()
    ir.run('Run >NIL: "%s" VTC:distkit/Install APPNAME UP-Term MINUSER %s DEFUSER %s NOLOG' % (inst, level, level), 30)
    time.sleep(5)
    finished, log = drive(case, inst, dest, base, expect_abort=(case == 'space'))
    t_install = time.time() - t_install
    (OUT / (case + ".log.txt")).write_text('\n'.join(log) + '\n')
    ck(finished, 'the Installer ran to its end (%s)' % level, log[-1] if log else '')
    lay = [l for l in log if l.startswith('LAYOUT')]
    # the room message is the Installer's own abort dialog drawn over the page, not one of our pages
    ck(not lay or case == 'space', 'every page\'s text readable: no line cut by the border, under the buttons or beside the copy panel',
       '; '.join(lay[:4]))
    if not finished:
        if installer_running(inst):
            ir.run('Break >NIL: `Status COM "%s"` C' % inst)
        return False, '; '.join(ck.failed + log[-3:])
    if case == 'space':
        # the room check: Install on a volume with too little room says so and changes nothing
        ck(any(l.startswith('room message seen') for l in log), 'the room message came up and led back to the drawer page', '; '.join(log[-3:]))
        ck(not any('% done' in l for l in log), 'the Installer ended without a copy phase', '; '.join(log[-3:]))
        ck(ir.run('List >NIL: SYS:UP-Term')[0] != 0, 'no drawer was made', '')
        ck(ir.run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'no UP-Term: assign', '')
        ck(ir.run('Type S:User-Startup')[1] == startup_before, 'S:User-Startup as it was, byte for byte', '')
        st = ir.lib_state()
        ck(st == before, 'LIBS: as it was', 'now %s, before %s' % (st, before))
        print('installer_rig %s: passed %d of %d' % (case, ck.total - len(ck.failed), ck.total))
        return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total
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
              'SYS:System/UP-Term.info', 'SYS:Prefs/UP-Term-Prefs.info', 'ENVARC:up-term/unstartup.sh',
              'UP-Term:MANIFEST', 'ENVARC:up-term/opts/UPTOPTDEST'):
        ck(ir.run('List >NIL: %s' % f)[0] == 0, '%s is there' % f)
    st = ir.lib_state()
    ck(st.get('ixemul.library.orig') == before.get('ixemul.library'), 'the original ixemul kept as .orig', str(st))
    ck(ir.run('Search >NIL: LIBS:ixemul.library vtcon-2')[0] == 0, 'the patched ixemul is in LIBS:')
    ck(ir.run('Assign >NIL: PTY: EXISTS DEVICES')[0] == 0, 'PTY: is mounted')
    rc, out = ir.run('C:UPConsole STATUS')
    ck('kit:' in out, 'UPConsole STATUS prints the kit line (UP-Term:VERSIONS)', out)
    if case == 'update':
        update_phase(ck, inst, level, base, t_install)
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
       'after Uninstall: the original ixemul back, no .orig', 'now %s, before %s' % (st, before))
    ck(st.get('ixnet.library') == before.get('ixnet.library') and 'ixnet.library.orig' not in st,
       'after Uninstall: the original ixnet back, no .orig', 'now %s, before %s' % (st, before))
    ir.run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote QUIET')
    if rig_gg:
        ir.run('Assign GG: VTC:gg')
    ir.run('Avail >NIL: FLUSH')
    for line in ir.rig_libs():
        ir.run(line)
    print('installer_rig %s: passed %d of %d' % (case, ck.total - len(ck.failed), ck.total))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


def manifest_entries(text):
    """{kit path: (size, crc)} of the files in a MANIFEST text (its install.dos#, copy# and handed# lines left out)."""
    out = {}
    for line in text.splitlines():
        f = line.split(None, 3)
        if len(f) == 4 and "#" not in f[3]:
            out[f[3]] = (f[1], f[2])
    return out


def unchanged_file(old, new, prefix):
    """A kit path under `prefix` that both manifests list with the same size and CRC, a text file by preference:
    an Update leaves it alone, a full Install copies it again. None when there is none."""
    both = sorted(p for p in new if p.startswith(prefix) and old.get(p) == new[p] and new[p][0] != "0")
    texty = [p for p in both if p.lower().endswith((".txt", ".md", ".vim", ".lua"))]
    return (texty or both or [None])[0]


def changed_files(old, new, prefix):
    """The kit paths under `prefix` an Update copies: new, or listed with another size or CRC."""
    return sorted(p for p in new if p.startswith(prefix) and old.get(p) != new[p])


def asked_drawer(log):
    """True when the Installer showed the drawer page (the askdir string, id 92): a full Install. An Update asks nothing."""
    return any(l.startswith("page [") and re.search(r"\bids\b.*\b92\b", l) for l in log)


def installer_run(tag, inst, kitdir, appname, level, base, dest=None):
    """Start the kit's Install icon (VTC:<kitdir>/Install) in the real Installer and answer its pages:
    (finished, log, seconds)."""
    t = time.time()
    ir.run('Run >NIL: "%s" VTC:%s/Install APPNAME %s MINUSER %s DEFUSER %s NOLOG' % (inst, kitdir, appname, level, level), 30)
    time.sleep(5)
    finished, log = drive(tag, inst, dest, base)
    (OUT / (tag + ".log.txt")).write_text("\n".join(log) + "\n")
    return finished, log, time.time() - t


def installer_done(ck, inst, tag, finished, log, readable=True):
    """The checks every Installer run has: it ran to its end, its pages are readable. False when it did not finish
    (the Installer is then ended, the case stops)."""
    ck(finished, "%s: the Installer ran to its end" % tag, log[-1] if log else "")
    if readable:
        lay = [l for l in log if l.startswith("LAYOUT")]
        ck(not lay, "%s: every page\'s text readable" % tag, "; ".join(lay[:4]))
    if not finished and installer_running(inst):
        ir.run("Break >NIL: `Status COM \"%s\"` C" % inst)
    return finished


def read_installer():
    inst = find_installer()
    if not inst:
        raise SystemExit("installer_rig: no Installer on the rig (Which Installer, SYS:System, SYS:Utilities, C:)")
    return inst, {t for t, _ in windows(ami.req(0x0D).decode("latin-1"))}


def run_addon_case(pkg):
    """The base by its Installer, then the add-on's own Install icon, both with every page answered by its default."""
    case, ck = "addon-" + pkg, Checks()
    ir.prepare(None, [pkg])
    before = ir.Before()
    ir.plant(pkg)
    inst, base = read_installer()
    ok = installer_done(ck, inst, case + " base", *installer_run(case + "-base", inst, "distkit", "UP-Term", "AVERAGE", base)[:2])
    if ok:
        ck(ir.run("List >NIL: UP-Term:UP-Term.version")[0] == 0, "the base Installer wrote UP-Term:UP-Term.version")
        ir.addon_absent(ck, pkg, "before its Install")
        ok = installer_done(ck, inst, case, *installer_run(case, inst, "kit-" + pkg, ir.archive(pkg), "AVERAGE", base)[:2])
    if ok:
        ir.addon_verify(ck, pkg, remote=False)
        ir.base_whole(ck, "with %s installed" % pkg)
        rc, out = ir.run_long("rununinstall-" + pkg)
        ck(rc == 0, "%s: its Uninstall runs" % pkg, out)
        ir.addon_gone(ck, pkg, remote=False)
        ir.base_whole(ck, "after %s\'s Uninstall" % pkg)
    if not ok:
        ir.run_long("rununinstall")
    else:
        ir.base_uninstall(ck, before)
    print("installer_rig %s: passed %d of %d" % (case, ck.passed, ck.total))
    return not ck.failed, "; ".join(ck.failed) or "all %d checks" % ck.total


def plant_edit(path):
    """Append a line to an installed file; (it survives an Update that leaves the file alone, a full Install overwrites it)."""
    ir.run("Echo >>%s \"installer_rig planted\"" % path)


def edit_survived(path):
    return ir.run("Type %s" % path)[1].rstrip().endswith("installer_rig planted")


def run_migrate():
    """The single kit's Installer, then the new base over it, then the nvim add-on: the Update path both times."""
    case, ck = "migrate", Checks()
    ir.prepare(None, ["nvim"])
    ir.prepare_legacy()
    try:
        return _migrate(case, ck)
    finally:
        ir.run_slow("deletelegacy", "Delete VTC:legacykit ALL QUIET")


def _migrate(case, ck):
    legacy = manifest_entries((ir.LEGACY_KIT / "Files/MANIFEST").read_text(errors="replace"))
    newbase = manifest_entries((ir.kit_dir("base") / "Files/MANIFEST").read_text(errors="replace"))
    newnvim = manifest_entries((ir.kit_dir("nvim") / "Files/MANIFEST").read_text(errors="replace"))
    p_base = unchanged_file(legacy, newbase, "Files/unifont/")
    p_nvim = unchanged_file(legacy, newnvim, "Files/nvim/")
    ck(p_base and p_nvim, "the single kit and the new kits share an unchanged file to plant an edit in", "%s %s" % (p_base, p_nvim))
    if not (p_base and p_nvim):
        return False, "; ".join(ck.failed)
    at = lambda p: "UP-Term:" + p.split("/", 1)[1]
    before = ir.Before()
    inst, base = read_installer()
    # The single kit with Python and Neovim ticked (its defaults) is 75 MB: more than the 3.1 rig's System volume
    # has free (65 MB), so Neovim never got copied and the Installer's last requester was clicked away. The drawer
    # goes to the host drawer VTC: as in the dest case; the new kits find it through the UP-Term: assign (Update).
    legacy_dest = "VTC:Apps/UP-Term"
    ir.run("MakeDir >NIL: VTC:Apps")
    ir.run_slow("deletedest", 'Delete "%s" ALL QUIET' % legacy_dest)
    if not installer_done(ck, inst, "migrate legacy", *installer_run("migrate-legacy", inst, "legacykit", "UP-Term", "AVERAGE", base, legacy_dest)[:2], readable=False):
        ir.run_long("rununinstall")
        return False, "; ".join(ck.failed)
    for f in ("UP-Term:nvim/bin/nvim", "UP-Term:Python3/bin/python3", "C:Claude", "C:amiga-pi"):
        ck(ir.run("List >NIL: %s" % f)[0] == 0, "the single kit installed %s" % f)
    ck(ir.run("List >NIL: UP-Term:pkg")[0] != 0, "the single kit leaves no UP-Term:pkg")
    nvim_date = ir.run('List UP-Term:nvim/bin/nvim LFORMAT "%D %T"')[1].strip()
    plant_edit(at(p_base))
    # the new base over the single kit: the Installer offers Update (no drawer page), hands nvim over
    finished, log, _ = installer_run("migrate-base", inst, "distkit", "UP-Term", "AVERAGE", base)
    if installer_done(ck, inst, "migrate base", finished, log):
        ck(not asked_drawer(log), "the base Install over the single kit is the Update path (no drawer page)", "; ".join(log[:6]))
        ck(edit_survived(at(p_base)), "the Update left an unchanged file alone (%s kept the planted edit)" % at(p_base))
        ck(ir.run('List UP-Term:nvim/bin/nvim LFORMAT "%D %T"')[1].strip() == nvim_date, "the handed nvim keeps its date", nvim_date)
        rc, out = ir.run_slow("nvimverrun", "UP-Term:nvim/bin/nvim --version", 400)
        ck(rc == 0 and "NVIM v0.12" in out, "nvim still answers after the new base", out[-200:])
        for f in ("MANIFEST", "VERSIONS"):
            ck(ir.run("List >NIL: UP-Term:pkg/legacy/%s" % f)[0] == 0, "UP-Term:pkg/legacy/%s was kept" % f)
        ck(ami.read_file("UP-Term:MANIFEST") == (ir.kit_dir("base") / "Files/MANIFEST").read_bytes(), "UP-Term:MANIFEST is the new base\'s")
        ck(ir.run("List >NIL: UP-Term:UP-Term.version")[0] == 0, "the base wrote UP-Term:UP-Term.version")
        # the nvim add-on over the single kit's files: Update "installed by the single kit"
        plant_edit(at(p_nvim))
        finished, log, _ = installer_run("migrate-nvim", inst, "kit-nvim", ir.archive("nvim"), "AVERAGE", base)
        if installer_done(ck, inst, "migrate nvim", finished, log):
            ck(edit_survived(at(p_nvim)), "the add-on\'s Update left an unchanged file alone (%s kept the planted edit)" % at(p_nvim))
            changed = changed_files(legacy, newnvim, "Files/nvim/")[:20]
            if changed:
                sizes = [ir.run('List "%s" LFORMAT "%%L"' % at(p))[1].strip() for p in changed]
                want = [str(newnvim[p][0]) for p in changed]
                ck(sizes == want, "the files that differ are the add-on kit\'s now (%d)" % len(changed), "%s vs %s" % (sizes, want))
            ck(ir.run("List >NIL: UP-Term:pkg/UP-Term-nvim/MANIFEST")[0] == 0 and
               ami.read_file("UP-Term:pkg/UP-Term-nvim/MANIFEST") == (ir.kit_dir("nvim") / "Files/MANIFEST").read_bytes(),
               "the nvim add-on made its record (its MANIFEST)")
            rc, out = ir.run_slow("nvimverrun", "UP-Term:nvim/bin/nvim --version", 400)
            ck(rc == 0 and "NVIM v0.12" in out, "nvim answers after its add-on", out[-200:])
    ir.base_uninstall(ck, before)
    for f in ("C:Claude", "C:ClaudeCode", "C:amiga-pi", "UP-Term:nvim"):
        ck(ir.run("List >NIL: %s" % f)[0] != 0, "after the base Uninstall %s is gone (the legacy block)" % f)
    ck(ir.run("Assign >NIL: Python3: EXISTS")[0] != 0, "after the base Uninstall no Python3: assign")
    print("installer_rig %s: passed %d of %d" % (case, ck.passed, ck.total))
    return not ck.failed, "; ".join(ck.failed) or "all %d checks" % ck.total


def main():
    fx = ir.Fixtures(('ENVARC:up-term/Dir',))
    with paths.rig_lock():
        try:
            return _main(fx)
        finally:
            fx.restore()   # a killed or failing run leaves nothing planted
            ir.show_close()


def _main(fx):
    args = sys.argv[1:]
    only = [args[args.index("ONLY") + 1]] if "ONLY" in args else []
    # space fills SYS: with a file first (K7); it is not part of the default run
    names = [c for c in CASES if only or c != 'space']
    for case in (c for c in names if not only or c in only):
        for pkg in case_packages(case)[0]:
            if not (ir.kit_dir(pkg) / "Install").exists():
                raise SystemExit('no kit %s: make dist first' % ir.kit_dir(pkg))
    todo, skip = verdicts.select(STORE, names, fingerprint, only, "FAILED" in args, "FORCE" in args)
    for c, why in skip:
        print("[SKIP] %s: %s" % (c, why))
    booted = False
    for case, _ in todo:
        if not booted:
            rig("stop")
            rig("start")
            ir._shown = None   # a fresh boot: the watched window (UPTERM_RIG_SHOW) opens again
            fx.take()   # the user's files and assigns, before any case plants its own
            booted = True
        try:
            ok, detail = run_case(case)
        except SystemExit as e:  # the agent or the rig is gone: not a verdict on the kit
            print("[ABORT] %s: %s (no verdict written)" % (case, e))
            return 2
        record(case, ok, detail)
    bad = STORE.failed([c for c, _ in todo])
    print("installer_rig: %d of %d cases failed: %s" % (len(bad), len(todo), " ".join(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
