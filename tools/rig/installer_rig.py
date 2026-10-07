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

Not confirmed on the rig yet (written 2026-10-07, not run): the window title
and gadget labels UITREE gives for the Installer's pages (matched loosely
below: "Proceed", "Install for Real"), and the string gadget of the askdir
page taking RAmiga-X (clear) and typed text."""
import hashlib, json, os, pathlib, shlex, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import install_rig as ir
from assign_rig import rig

ROOT = ir.ROOT
VTC = ir.VTC
OUT = ROOT / "build/rig/installer"
KIT = ROOT / "build/dist/UP-Term"
CASES = {"default": ("AVERAGE", None), "dest": ("AVERAGE", "VTC:Apps/UP-Term"), "novice": ("NOVICE", None)}
# the buttons that move an Installer page on, in the order they are tried;
# never Abort, never "Skip This Part"
FORWARD = ("Install for Real", "Proceed with Install", "Proceed With Install", "Proceed with Copy", "Proceed", "OK")
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
    x, y, w, h = box
    ami.req(0x08, bytes([5]) + struct.pack('>HH', x + w // 2, y + h // 2) + bytes([0, 1]))


def installer_running(inst):
    # Status COM matches the command as it was started (the full path when one was given)
    return ir.run('Status >NIL: COM "%s"' % inst)[0] == 0


def find_installer():
    for p in ('Installer', 'SYS:System/Installer', 'SYS:Utilities/Installer', 'C:Installer', 'SYS:C/Installer'):
        if ir.run('Which >NIL: "%s"' % p)[0] == 0 or ir.run('List >NIL: "%s"' % p)[0] == 0:
            return p
    return None


def type_into(box, text):
    """Click a string gadget, clear it (RAmiga-X), type text, Return."""
    click(box)
    time.sleep(0.5)
    ami.key(KEY_X, RAMIGA)
    time.sleep(0.3)
    ami.req(0x08, bytes([4]) + text.encode('latin-1'))
    time.sleep(0.3)
    ami.key(KEY_RETURN)
    time.sleep(1)


def drive(case, inst, dest, baseline_titles, timeout=5400):
    """Answer the Installer's pages until it ends. Returns (finished, log)."""
    log = []
    pages = OUT / (case + ".pages.txt")
    pages.write_text('')
    end = time.time() + timeout
    last, typed, idle = None, False, 0
    while time.time() < end:
        if not installer_running(inst):
            return True, log
        tree = ami.req(0x0D).decode('latin-1')
        wins = [w for w in windows(tree) if w[0] not in baseline_titles]
        if any(t in ('System Request', 'Volume Request') for t, _ in windows(tree)):
            log.append('a system requester came up')
            with pages.open('a') as f:
                f.write('---- requester\n' + tree + '\n')
            return False, log
        labels = [(lab, box) for _, gads in wins for lab, box in gads]
        if tree != last:
            with pages.open('a') as f:
                f.write('---- %.0f s\n%s\n' % (timeout - (end - time.time()), tree))
            last = tree
        names = ' | '.join(lab for lab, _ in labels)
        # the askdir page (Show Drives / Parent Drawer): the chosen drawer
        if dest and not typed and any('Show Drives' in lab for lab, _ in labels):
            strings = [box for lab, box in labels if lab.strip() in ('', 'string', 'STRING')]
            if not strings:
                log.append('askdir page without a string gadget: ' + names)
                return False, log
            type_into(strings[-1], dest)
            typed = True
            log.append('typed %s into the drawer page' % dest)
            continue
        hit = None
        for want in FORWARD:
            hit = next(((lab, box) for lab, box in labels if want.lower() in lab.lower()), None)
            if hit:
                break
        if hit:
            log.append('page [%s]: %s' % (names[:160], hit[0]))
            click(hit[1])
            if 'Install for Real' in hit[0]:
                # a choice, not a page turn: Proceed on the same page next
                time.sleep(1)
                p = next(((lab, box) for lab, box in labels if lab.strip().lower().startswith('proceed')), None)
                if p:
                    click(p[1])
            time.sleep(3)
            idle = 0
        else:
            idle += 1   # copying / working: only Abort on the page
            time.sleep(5)
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
