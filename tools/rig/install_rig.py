#!/usr/bin/env python3
"""install_rig.py -- the install kits on the rig, the owner's way: unpack build/dist/<archive> into
VTC:, run its Files/install.dos (what the kit's Installer script runs), check what it did, then
Uninstall and check the rig is as before. The rig's own image is left with its original ixemul.
The rig must be up; `make dist` first.

One case per package on the base, and the combinations (K2-10, plan 2026-10-13-kit-split.md):
  base        the default drawer (SYS:UP-Term): PTY:, the patched ixemul with the original kept, the console, ...
  base-dest   a non-default drawer (DEST=VTC:Apps/UP-Term); both reach the files through the assign UP-Term:
  base-move   Install into the default drawer, then again with DEST=VTC:Apps/UP-Term (the assign moves)
  base-libs   LIBS: a multi-assign (the rig's normal boot)
  base-var    Uninstall and UP-Term:var (No keeps it, Yes removes it)
  claude pi nvim python
              (one per add-on of dist/packages.txt) the base, then the add-on's install.dos, its checks, its
              Uninstall, the base still whole, the base's Uninstall
  all         the base and every add-on, then only the base's Uninstall: every add-on goes with it
  legacy      LEGACY_KIT (the single kit of 0cd6f0b) installed, the new base over it, the nvim add-on, base Uninstall
  nobase      an add-on's install.dos with no UP-Term: returns 20 and ENV:UPTCHECK names UP-Term.lha

Resumable (RULES section 6a): the verdict of each case goes to build/<rig>/install/<case>.json when the case
ends, with a fingerprint of the archives it installs; a case that passed with the same kit is skipped.
  install_rig.py                  every case not yet passed with this kit
  install_rig.py --only nvim      one case, even when it passed (--case is the same; repeat it for more)
  install_rig.py --failed         only the cases that failed last time
  install_rig.py --force          every case, from zero
  install_rig.py --list           the recorded verdicts
  install_rig.py --show-check     the watched window (UPTERM_RIG_SHOW=<title>) alone
  --default --dest --move --var --libs   the old pass flags: --only base, base-dest, base-move, base-var, base-libs
One script per rig: a second one on the same rig stops with the first one's pid (paths.rig_lock)."""
import os, pathlib, re, shutil, signal, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import verdicts

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
# the kit under test: build/dist/UP-Term, or UPTERM_KIT=<drawer> (build/dist is
# shared with every agent's `make dist`, so a rig run takes a private copy)
KIT = pathlib.Path(os.environ.get('UPTERM_KIT') or ROOT / "build/dist/UP-Term")


def assembled_scripts(pkg='base'):
    """The bytes of the package's Install, install.dos and Uninstall as tools/mkinstaller.py
    assembles them from dist/ and dist/common/ (the templates are tools/mkkit.py's): what a kit
    build writes, so a rig verdict's fingerprint moves with every part the scripts are made of
    (K5), built kit or not."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import mkinstaller, mkkit
    return [mkinstaller.build(t, pkg).encode('latin-1') for t in mkkit.templates(pkg)[:3]]


def package_ids():
    """The ids of dist/packages.txt, the base first."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    return [r['id'] for r in pkgs.rows()]


def addon_ids():
    return [i for i in package_ids() if i != 'base']


def archive(pkg):
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    return pkgs.get(pkg, 'archive')


def kit_dir(pkg):
    """The built drawer of the package: the base is KIT, an add-on build/dist/<archive>
    (UPTERM_KITS=<drawer> names another place for the add-ons, as UPTERM_KIT does the base)."""
    if pkg == 'base':
        return KIT
    return pathlib.Path(os.environ.get('UPTERM_KITS') or ROOT / 'build/dist') / archive(pkg)


# The single kit of 0cd6f0b (before the split) the legacy and migrate cases install first: one
# copy in the main checkout's build/legacy (build/ is gitignored), UPTERM_LEGACY_KIT=<drawer> for another.
LEGACY_KIT = pathlib.Path(os.environ.get('UPTERM_LEGACY_KIT') or paths.RIG_HOME / 'legacy/UP-Term-0cd6f0b')


def kit_fingerprint(*pkgs_):
    """What a case installs: for each package its assembled scripts and the kit's MANIFEST (a line per
    file with its CRC: the payload), so a changed binary or script runs the case again."""
    chunks = []
    for pkg in pkgs_:
        chunks += assembled_scripts(pkg) + [kit_dir(pkg) / 'Files/MANIFEST']
    return verdicts.fingerprint(*chunks)


# a clean image (rig.py --os) has no DH0:: its system volume is SYS:
CLEAN = '--os' in os.environ.get('UPTERM_RIG_FLAGS', '')
SYSVOL = 'SYS:' if CLEAN else 'DH0:'

def rig_libs():
    """rig.RIG_LIBS with the system volume the rig really has (DH0: asks for a
    volume that a clean image does not have)."""
    import rig
    return [l.replace('DH0:', SYSVOL) for l in rig.RIG_LIBS]

ORIG_SIZE = 166972  # the 3.1 rig's ixemul.library 48.2 as released; main() takes the size the system has before Install (the 3.2 tree's differs)

def run(cmd, timeout=60):
    if SHOW:
        return show(cmd, timeout)
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'), timeout + 30)
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')

# UPTERM_RIG_SHOW=<title>: the commands run in a window on the Workbench
# screen the owner can watch, titled <title>, instead of inside the agent: a
# Shell (a ROM CON: window, there with UP-Term installed or not) executes
# <drawer>/loop, which takes each command the host writes to <drawer>/cmd
# (renamed to run, so the next one cannot be lost), types the command into
# the window and starts it in a Shell of its own with no console (NewShell
# NIL:), then types its output into the window when it is done. Not in the
# loop's Shell: an Execute there splices the loop's rest into a new script
# file, and its Skip top BACK then finds no label ("object not found", "Skip
# failed returncode 10" on 3.1, 2026-10-09). Not in a Run'd Shell on the
# window either: that Shell stopped after C:UPConsole CON ON switched the
# window's handler (2026-10-10). Each line's output is appended to out<n>
# (a line that redirects its own keeps it; an Execute'd script's lines print
# nowhere: run_long has the agent run them and types their output here), and rc<n> is
# written last; the host waits for rc<n>. <drawer> is a new VTC:show-<time> for each window,
# and only the Amiga side deletes in it: FS-UAE's VTC: keeps names the Mac
# deleted, and a Rename onto such a name fails. Commands run at FailAt 1000:
# a failing line (Execute of a missing file returns 10) does not end the
# wrapper, and the last line's return code comes back.
SHOW = os.environ.get('UPTERM_RIG_SHOW', '')
SHOW_LOOP = """FailAt 1000
Lab top
If EXISTS {d}/stop
  Skip done
EndIf
If EXISTS {d}/done
  Type {d}/last
  Delete {d}/done QUIET
EndIf
If EXISTS {d}/cmd
  Delete {d}/run QUIET
  Rename {d}/cmd {d}/run
  Type {d}/show
  Run Execute {d}/run
EndIf
Wait 1
Skip top BACK
Lab done
CD VTC:
Delete {d} ALL QUIET
EndCLI >NIL:
"""
SHOW_KEEP = ('IF', 'ELSE', 'ENDIF', 'LAB', 'SKIP', 'QUIT', 'FAILAT')   # script keywords: left as they are
_shown = None   # the window's drawer name on VTC:, while it is open
_seq = 0

def show_open():
    """Open the watched window (again after a reboot: installer_rig resets _shown)."""
    global _shown, _seq
    name = 'show-%d' % int(time.time())
    (VTC / name).mkdir()
    d = 'VTC:' + name
    (VTC / name / "loop").write_text(SHOW_LOOP.replace('{d}', d))
    (VTC / name / "start").write_text('Execute %s/loop\n' % d)   # Skip ... BACK works in an Execute script only
    title = re.sub(r'[/"]', ' ', SHOW)
    b = ami.req(0x02, struct.pack('>H', 20) +
                ('NewShell "CON:0/12/640/200/%s/CLOSE" FROM %s/start' % (title, d)).encode('latin-1'), 50)
    if struct.unpack('>I', b[:4])[0] != 0:
        raise SystemExit('show: NewShell failed: %r' % b[4:])
    _shown, _seq = name, 0

def show_close():
    global _shown
    if _shown:
        (VTC / _shown / "stop").write_text('')
    _shown = None

def show(cmd, timeout):
    """run() in the watched window: (return code, output)."""
    global _seq
    if not _shown:
        show_open()
    _seq += 1
    n, h, d = _seq, VTC / _shown, 'VTC:' + _shown
    # the wrapper: the command's lines, each with its output appended to out<n>
    # (not its input: Search fails with <NIL:), its output copied to last for
    # the loop to type, done, and rc<n> last
    out = '%s/out%d' % (d, n)
    lines = ['FailAt 1000', 'Echo >%s "" NOLINE' % out]
    for line in cmd.split('\n'):
        w = line.split(None, 1)
        if w and w[0].upper() not in SHOW_KEEP and not w[0].startswith(';') and not (len(w) > 1 and w[1][:1] in '<>'):
            line = '%s >>%s%s' % (w[0], out, (' ' + w[1]) if len(w) > 1 else '')
        lines.append(line)
    lines += [x.replace('{d}', d).replace('{n}', str(n)) for x in
              ('Echo >{d}/rcw{n} "$RC"', 'Copy {d}/out{n} {d}/last QUIET', 'Echo >{d}/done ""',
               'Rename {d}/rcw{n} {d}/rc{n}')]
    (h / "show").write_text('-------- %d\n%s\n' % (n, cmd))
    (h / "cmd").write_text('\n'.join(lines) + '\n')
    rcf, outf = h / ("rc%d" % n), h / ("out%d" % n)
    end = time.time() + timeout + 30
    while time.time() < end:
        try:
            rc = rcf.read_text(errors='replace').strip()
        except FileNotFoundError:
            time.sleep(0.5)
            continue
        if rc:
            out = outf.read_text(errors='replace') if outf.exists() else ''
            return (int(rc) if rc.isdigit() else 0), out
        time.sleep(0.5)
    raise SystemExit('ERR: show: still running after %d s: %r' % (timeout, cmd[:80]))

def ports():
    """Every tool of the Unix ports (plan items 1.2-1.9, 2.1-2.5) once, through
    vsh's $PATH, with a result only the right program gives; a vsh script, so
    no AmigaDOS quoting stands between (VTC:ports.sh). xz compresses at -0
    (about 3 MB): its default -6 needs about 94 MB and an 8 MB A1200 answers
    "Cannot allocate memory" (rig 1, 2026-10-10). vsh's command -v names the
    Amiga path."""
    ports = [('sed', 'echo abc | sed s/b/X/', lambda o: o == 'aXc'),
             ('awk', "echo 3 4 | awk '{print $1+$2}'", lambda o: o == '7'),
             ('less', 'less --version | head -1', lambda o: 'less 710' in o),
             ('nano', 'nano --version | head -1', lambda o: '9.2' in o),
             ('find', 'find --version | head -1', lambda o: 'findutils' in o and '4.11' in o),
             ('xargs', 'echo a b | xargs echo x', lambda o: o == 'x a b'),
             ('diff', 'diff --version | head -1', lambda o: '3.12' in o),
             ('cmp', 'cmp --version | head -1', lambda o: '3.12' in o),
             ('patch', 'patch --version | head -1', lambda o: '2.8' in o),
             ('man', 'man -w sed', lambda o: o.endswith('share/man/man1/sed.1')),
             ('gzip', 'echo hi | gzip | gzip -dc', lambda o: o == 'hi'),
             ('bzip2', 'echo hi | bzip2 | bzip2 -dc', lambda o: o == 'hi'),
             ('xz', 'echo hi | xz -0 | xz -dc', lambda o: o == 'hi'),
             ('tar', 'tar --version | head -1', lambda o: '3.8.9' in o),
             ('zip', 'zip -h | grep -c "Zip 3.0"', lambda o: o.isdigit() and int(o) > 0),
             ('path', 'command -v sed', lambda o: o in ('/UP-Term/bin/sed', 'UP-Term:bin/sed'))]
    (VTC / "ports.sh").write_text(''.join('echo "%s:$(%s)"\n' % (t, c) for t, c, _ in ports))
    rc, out = run('Stack 200000\nC:vsh VTC:ports.sh', 120)
    got = dict(l.split(':', 1) for l in out.splitlines() if ':' in l)
    for t, c, ok in ports:
        o = got.get(t, '').strip()
        check(ok(o), 'PORTS: %s through vsh\'s $PATH (%s)' % (t, c), o or out[-200:])

def show_check():
    """--show-check: the watched window survives what broke its first version
    (2026-10-09: the owner saw "object not found", "Skip failed returncode
    10" in it on 3.1, read as an Installer error): after a failing command
    (Execute of a missing file, 10) and a second one, each command's return
    code and output still come back. Needs UPTERM_RIG_SHOW."""
    global passed, total
    passed = total = 0
    if not SHOW:
        raise SystemExit('--show-check needs UPTERM_RIG_SHOW=<title>')
    rc, out = run('Echo one')
    check(rc == 0 and out.strip() == 'one', 'show: a first command and its output', out)
    rc, out = run('Execute T:show-check-nosuch')
    check(rc == 10 and 'object not found' in out, 'show: Execute of a missing file returns 10, its message comes back', out)
    rc, out = run('Echo two\nEcho three')
    check(rc == 0 and out.split() == ['two', 'three'], 'show: the window takes commands after the failing one (two lines)', out)
    (VTC / "showscript").write_text('Echo from-the-script\n')
    rc, out = run_long('showscript', 120)
    check(rc == 0 and 'from-the-script' in out, 'show: run_long returns the words of an Execute\'d script', out)
    rc, out = run('Delete >NIL: T:show-check-nosuch#? QUIET')
    check(rc == 5, 'show: a line that redirects its own output keeps it, its return code comes back', out)
    # a command that switches the console handler of every new window (C:UPConsole
    # CON ON stopped the Run'd Shell on the window, 2026-10-10): the next still runs
    if run('List >NIL: C:UPConsole')[0] == 0:
        was = 'CON: UP-Term' in run('C:UPConsole STATUS')[1]
        run('C:UPConsole >NIL: CON OFF')
        run('C:UPConsole >NIL: CON ON')
        rc, out = run('Echo four')
        check(rc == 0 and out.strip() == 'four', 'show: a command after C:UPConsole CON ON', out)
        if not was:
            run('C:UPConsole CON OFF')
    # the fixtures: a line a case appends to S:User-Startup does not stay
    fx = Fixtures()
    fx.take()
    before = run('Type S:User-Startup')[1]
    run('Echo >>S:User-Startup "; fixture probe"')
    fx.restore()
    check(run('Type S:User-Startup')[1] == before, 'fixtures: S:User-Startup is put back after a case appended a line', '')
    check(signal.getsignal(signal.SIGTERM) not in (signal.SIG_DFL, signal.SIG_IGN), 'fixtures: a SIGTERM ends the run through its finally (the fixtures are put back)', '')
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

def run_long(script, timeout=1500):
    """Execute VTC:<script>, however long it takes, as run() returns it. The
    agent gives a command 120 s and then answers ERR "still running", leaving it
    running (its output is lost then: a script that finishes inside the 120 s
    returns its output, one that does not returns '' and its return code).
    The script runs inside a wrapper that writes its return code to
    VTC:longdone, and this waits for that file."""
    (VTC / "longdone").unlink(missing_ok=True)
    # FailAt 1000: a script that ends with 10 or 20 (a refused install) must not stop the wrapper before
    # it writes the return code (then this waited out its timeout and reported nothing)
    (VTC / "longwrap").write_text("FailAt 1000\nExecute VTC:%s\nEcho >VTC:longdone \"$RC\"\n" % script)
    out = ''
    end = time.time() + timeout
    if SHOW:
        # An Execute in the watched window prints into the window and nowhere
        # else: no redirection reaches an Execute'd script's lines, and Run
        # hands the new CLI the window, not the redirection (rig 1,
        # 2026-10-10: the checks on Install's words saw ''). So the agent runs
        # the wrapper and captures its words; the window gets the command now
        # and the words when it is done.
        show('Echo "run_long: Execute VTC:%s"' % script, 60)
    while True:
        try:
            out = ami.req(0x02, struct.pack('>H', 120) + b'Execute VTC:longwrap', 150)[4:].decode('latin-1')
        except SystemExit as e:
            # "a command is still running ... runs one at a time": the agent
            # is busy with an earlier command and did NOT start this one
            # (taken for "started" before, the wait below ran its full
            # timeout for a script that never ran, 2026-10-07). Wait for it.
            if 'one at a time' in str(e) and time.time() < end:
                time.sleep(5)
                continue
            if 'still running' not in str(e):
                raise
        break
    while not (VTC / "longdone").exists():
        if time.time() > end:
            return 1, 'run_long: %s did not finish in %d s' % (script, timeout)
        time.sleep(3)
    rc = ''
    for _ in range(20):  # the host drawer can show the file a moment before it can be read
        time.sleep(1)
        try:
            rc = (VTC / "longdone").read_text(errors='replace').strip()
            break
        except FileNotFoundError:
            continue
    if not rc:
        return 1, 'run_long: VTC:longdone vanished or empty after %s' % script
    if SHOW:
        (VTC / (script + ".runout")).write_text(out)
        show('Type VTC:%s.runout' % script, 60)
    return (int(rc) if rc.isdigit() else 0), out

def run_slow(name, cmd, timeout=600):
    """One command that can pass the agent's limit (a cold nvim start; Delete
    ALL of an installed drawer, which took over 60 s on the 68020 and ended
    the --move pass with ERR "still running", 2026-10-07): written to
    VTC:<name>, its output to VTC:<name>.out, and run through run_long."""
    (VTC / (name + ".out")).unlink(missing_ok=True)
    (VTC / name).write_text("%s >VTC:%s.out\n" % (cmd, name))
    rc, out = run_long(name, timeout)
    f = VTC / (name + ".out")
    return rc, (f.read_text(errors='replace') if f.exists() else out)

passed = total = 0
failures = []   # the reasons of the failed checks of the pass that ran last (a case's verdict detail)
def check(ok, what, seen=''):
    global passed, total
    total += 1
    passed += bool(ok)
    if not ok:
        failures.append('%s: %s' % (what, str(seen).strip()[-200:]))
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', total, what, (': ' + seen.strip()) if seen and not ok else ''))

def lib_state():
    rc, out = run('List LIBS:(ixemul|ixnet).library#? LFORMAT "%N %L"')
    return dict(l.split() for l in out.splitlines() if l.strip())

def prepare(dest, addons=()):
    """The kit unpacked into VTC:distkit, the helper programs and the run scripts beside it, LIBS: as the rig boots it.
    Each add-on in `addons` is unpacked too, into VTC:kit-<id>, with runinstall-<id> and rununinstall-<id>."""
    destarg = (" DEST=%s" % dest) if dest else ""
    shutil.rmtree(VTC / "distkit", ignore_errors=True)
    shutil.copytree(KIT, VTC / "distkit")
    for pkg in addons:
        shutil.rmtree(VTC / ("kit-" + pkg), ignore_errors=True)
        shutil.copytree(kit_dir(pkg), VTC / ("kit-" + pkg))
        (VTC / ("runinstall-" + pkg)).write_text(addon_install_line(pkg) + "\n")
        (VTC / ("rununinstall-" + pkg)).write_text("Set upvardelete YES\nCD VTC:kit-%s\nExecute Uninstall\n" % pkg)
    for name in ("ptytest", "iconprobe", "wbrun", "conwho", "UPConsole"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    if dest:
        (VTC / "Apps").mkdir(exist_ok=True)   # the drawer's parent must exist
    (VTC / "runinstall").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files%s NOCONSOLE NODEVICE\n" % destarg)
    (VTC / "runinstallcon").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files%s CONSOLE DEVICE SHELLICON\n" % destarg)
    # the rig's Uninstall answers YES to the var question: UP-Term:var holds only
    # what earlier rig steps (amiga-pi, Claude sessions) wrote, and a kept var
    # leaves the drawer behind for the next pass (K3). var_check() covers the
    # default No with its own scripts.
    (VTC / "rununinstall").write_text("Set upvardelete YES\nCD VTC:distkit\nExecute Uninstall\n")
    # LIBS: as the rig boots it (ixpty_rig.use_ixemul puts VTC:ixp6 first,
    # and Install would then replace and keep the copy there)
    run('Assign LIBS: %sLibs' % SYSVOL)
    run('Assign LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD')

# What a run plants or removes on the Amiga that a user may own: each is saved
# before the run and put back in a finally, so a failing or killed run (the
# killed one that left 127.0.0.1 2399 in ENVARC:Claude/remote) leaves nothing.
FIXTURE_FILES = ('ENVARC:Claude/remote', 'ENV:Claude/remote', 'ENVARC:amiga-pi/settings.json', 'ENVARC:UP-Term.prefs', 'S:User-Startup')   # the move pass appends a user line to the last (ten were left by 2026-10-10)
FIXTURE_ASSIGNS = ('GG', 'UP-Term')

def strip_upterm_blocks(data):
    """data without the ;BEGIN UP-Term ... ;END UP-Term blocks (every flavour:
    assign, python, tmp, console, device ...) that Install writes."""
    out, skip = [], False
    for line in data.decode('latin-1').splitlines(True):
        if line.startswith(';BEGIN UP-Term'): skip = True
        if not skip: out.append(line)
        if skip and line.startswith(';END UP-Term'): skip = False
    return ''.join(out).encode('latin-1')

class Fixtures:
    """take() once the rig is up (a reboot keeps the disk, so any time after the
    first boot is the same), restore() in a finally. restore() without take() does nothing."""
    def __init__(self, files=()):
        self.paths = FIXTURE_FILES + tuple(files)
        self.files = self.assigns = None

    def take(self):
        if self.files is not None: return
        # a killed run (pkill, the Monitor's timeout) is SIGTERM: Python then
        # skips the finally, and the run left an UP-Term block in S:User-Startup
        # whose drawer the next run deleted -- "Please insert volume UP-Term:"
        # at every boot (rig 1, 2026-10-10)
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
        self.files = {p: ami.read_file(p) for p in self.paths}
        # the "as it was" of every check is a startup without UP-Term blocks;
        # restore() puts the file back whole (K3: the rig image carried the
        # blocks of an install whose drawer is gone, so Uninstall could not
        # strip them and NOCONSOLE/NODEVICE found them already written)
        saved = self.files.get('S:User-Startup')
        if saved is not None:
            clean = strip_upterm_blocks(saved)
            if clean != saved:
                print('install_rig: S:User-Startup held UP-Term blocks from before the run; the run starts without them')
                import tempfile
                with tempfile.NamedTemporaryFile(delete=False) as t: t.write(clean)
                try: ami.put(t.name, 'S:User-Startup')
                finally: os.unlink(t.name)
        rc, out = run('Assign LIST')
        self.assigns = {}
        for l in out.splitlines():
            f = l.split(None, 1)
            if len(f) == 2 and f[0].rstrip(':').upper() in FIXTURE_ASSIGNS:
                self.assigns[f[0].rstrip(':').upper()] = f[1].strip()

    def restore(self):
        if self.files is None: return
        for p, saved in self.files.items():
            try: ami.restore_file(p, saved)
            except BaseException as e: print('[WARN] fixture %s not restored: %s' % (p, e))
        for name in FIXTURE_ASSIGNS:
            try:
                if name in self.assigns: run('Assign %s: "%s"' % (name, self.assigns[name]))
                else: run('Assign >NIL: %s:' % name)
            except BaseException as e: print('[WARN] assign %s: not restored: %s' % (name, e))
        # LIBS: is the rig's normal boot order again, whatever a case assigned
        # (a rig without LIBS: or with a missing drawer stops every library open)
        try:
            for line in rig_libs(): run(line)
        except BaseException as e: print('[WARN] LIBS: not restored: %s' % e)
        print('install_rig: fixtures put back (%s)' % ' '.join(self.paths))

def main(dest=None):
    fx = Fixtures()
    try:
        fx.take()
        return _main(dest)
    finally:
        fx.restore()

def _main(dest=None):
    """One pass: dest None = the default drawer, else Install is given DEST=<dest>."""
    global passed, total
    passed = total = 0
    drawer = dest or "SYS:UP-Term"   # where the files must land
    prepare(dest)
    rc, terminfo_before = run('GetEnv TERMINFO')  # the rig's boot sets /VTC/terminfo
    terminfo_before = terminfo_before.strip()
    run_long('rununinstall')  # a run that stopped half-way left things behind
    startup_before = run('Type S:User-Startup')[1]
    sseq_before = run('Type S:Startup-Sequence')[1]  # no part of UP-Term edits it (UPTERM-VOLUME-REQUESTER)
    run('Delete >NIL: ENVARC:UP-Term.prefs QUIET')  # an earlier run's kept preferences
    ls_before = run('List >NIL: C:ls')[0] == 0  # Install puts no Unix command in C:
    shell_before = run('VTC:iconprobe SYS:System/Shell')[1]  # SHELLICON must give it back
    tmp_before = run('Assign >NIL: TMP: EXISTS')[0] == 0
    # the rig's boot assigns GG: (VTC:gg); take it away so Install's own
    # GG: set-up is what gets tested, and put it back at the end
    rig_gg = run('Assign >NIL: GG: EXISTS')[0] == 0
    if rig_gg:
        run('Assign GG:')
    gg_before = False
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == terminfo_before and terminfo_before != '/ENV/up-term/terminfo',
          'Uninstall with nothing installed leaves the user\'s TERMINFO', out)
    before = lib_state()
    global ORIG_SIZE
    ORIG_SIZE = int(before.get('ixemul.library') or ORIG_SIZE)
    check('ixemul.library.orig' not in before,
          'before: no .orig of ixemul (the system\'s own ixemul, when it has one)', str(before))
    ixnet_before = before.get('ixnet.library')  # the system's own, if any (Install keeps it as .orig)
    check('ixnet.library.orig' not in before, 'before: no .orig of ixnet', str(before))
    rc, out = run_long('runinstall')
    check(rc == 0 and 'Unknown command' not in out, 'Install runs (no line taken for a command)', out)
    # UP-Term: (UPTERM-ASSIGN): the drawer is where Install was told, reached
    # through the assign, and at every boot (a block before the others)
    rc, out = run('Assign LIST')
    al = [l for l in out.splitlines() if l.lower().startswith('up-term')]
    want_tail = drawer.split(':', 1)[1].lower()
    check(al and al[0].split(None, 1)[-1].strip().lower().endswith(want_tail),
          'UP-Term: is assigned to %s' % drawer, out)
    check(run('List >NIL: "%s/VERSIONS"' % drawer)[0] == 0 and run('List >NIL: UP-Term:VERSIONS')[0] == 0 and
          run('List >NIL: UP-Term:bin/sh')[0] == 0,
          'the files are in %s and UP-Term: reaches them' % drawer, '')
    if dest:
        check(run('List >NIL: SYS:UP-Term')[0] != 0, 'a non-default DEST leaves SYS:UP-Term unmade', '')
    check(run('Search >NIL: S:User-Startup ";BEGIN UP-Term assign"')[0] == 0 and
          run('Search >NIL: S:User-Startup ";END UP-Term assign"')[0] == 0, 'the UP-Term: block is marked', '')
    rc, out = run('Search S:User-Startup "Assign UP-Term:"')
    check('Assign UP-Term:' in out and drawer.split(':', 1)[1] in out.replace('"', ''),
          'S:User-Startup assigns UP-Term: to %s at boot (marked block)' % drawer, out)
    rc, out = run('Type S:User-Startup')
    check(out.find('Assign UP-Term:') >= 0 and (out.find('Assign GG:') < 0 or out.find('Assign UP-Term:') < out.find('Assign GG:')),
          'the UP-Term: block comes before the blocks that use it', out[-300:])
    # UPTERM-VOLUME-REQUESTER: the drawer is also in ENVARC:up-term/Dir, so
    # what runs before S:User-Startup can make the assign itself. Lose the
    # assign, run a real consumer (UPConsole STATUS reads UP-Term:VERSIONS):
    # the assign is back, the kit line is printed, and no requester is up.
    rc, out = run('Type ENVARC:up-term/Dir')
    check(rc == 0 and out.strip().lower() == drawer.lower(), 'ENVARC:up-term/Dir names %s' % drawer, out)
    run('Assign UP-Term:')
    check(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'the rig lost the UP-Term: assign', '')
    rc, out = run('VTC:UPConsole STATUS')
    check('kit:' in out, 'UPConsole STATUS finds UP-Term:VERSIONS with no assign (it made it from Dir)', out)
    check(run('Assign >NIL: UP-Term: EXISTS')[0] == 0, 'a consumer made the UP-Term: assign from ENVARC:up-term/Dir', '')
    tree = ami.req(0x0D).decode('latin-1')
    check('System Request' not in tree, 'no requester on screen (UITREE has no System Request)', tree[-200:])
    rc, out = run('Assign PTY: EXISTS DEVICES')
    check(rc == 0, 'Install mounted PTY:', out)
    rc, out = run('Assign IXPIPE: EXISTS DEVICES')
    check(rc == 0, 'Install mounted IXPIPE:', out)
    rc, out = run('VTC:ptytest', 120)
    check(rc == 0 and 'FAIL' not in out, 'the installed PTY: passes ptytest', out[-300:])
    st = lib_state()
    check(st.get('ixemul.library.orig') == before.get('ixemul.library'), 'the original ixemul kept as .orig (none when the system had none)', '%s; %s; %s' % (st, run('List ENVARC:up-term-orig')[1], run('Type ENVARC:up-term-orig/ixemul.path')[1]))
    kit_ixnet = (VTC / "distkit/Files/libs/ixnet.library").stat().st_size
    check(st.get('ixnet.library.orig') == ixnet_before and st.get('ixnet.library') == str(kit_ixnet),
          'the kit\'s ixnet in LIBS:, the original kept as .orig', str(st))
    rc, out = run('Search LIBS:ixemul.library UP-Term')
    check('UP-Term' in out, 'the patched ixemul is in LIBS:', out)
    rc, out = run('C:UPConsole STATUS')
    check('CON: ROM' in out, 'Install NOCONSOLE leaves CON: the ROM\'s', out)
    check(run('Search >NIL: S:User-Startup ";BEGIN UP-Term console"')[0] != 0,
          'Install NOCONSOLE writes no console block', '')
    check('console.device: ROM' in out, 'Install NODEVICE leaves console.device the ROM\'s', out)
    check(run('Search >NIL: S:User-Startup ";BEGIN UP-Term device"')[0] != 0,
          'Install NODEVICE writes no device block', '')
    # a second Install must not take ours for theirs; this one says yes to CON:
    rc, out = run_long('runinstallcon')
    check(rc == 0 and 'Unknown command' not in out, 'Install CONSOLE again over it runs', out)
    # SHELLICON: the Shell icon's window on XCON:, nothing else changed
    rc, out = run('VTC:iconprobe SYS:System/Shell')
    want = [l if not l.startswith('tooltype WINDOW=') else
            'tooltype WINDOW=XCON:' + l.split(':', 1)[1] for l in shell_before.splitlines()]
    check(out.splitlines() == want and 'WINDOW=XCON:' in out,
          'SHELLICON: the Shell icon opens on XCON:, its window and other tooltypes kept',
          '%r vs %r' % (out, want))
    check(run('List >NIL: SYS:System/UP-Term.info')[0] == 0, 'the UP-Term icon is in SYS:System, beside Shell', '')
    rc, out = run('C:UPConsole STATUS')
    check('CON: UP-Term' in out and 'RAW: UP-Term' in out, 'Install CONSOLE: CON: and RAW: are UP-Term now', out)
    rc, vout = run('Type UP-Term:VERSIONS')
    vl = vout.strip().splitlines()
    check(rc == 0 and vl and vl[0].startswith('UP-Term ') and len(vl) > 1,
          'Install copies the kit\'s VERSIONS to UP-Term:VERSIONS', vout[:200])
    check(vl and ('kit: ' + vl[0]) in out, 'C:UPConsole STATUS prints the first line of VERSIONS', out)
    check('console.device: UP-Term' in out, 'Install DEVICE: console.device is UP-Term\'s now', out)
    rc, out = run('Search S:User-Startup "C:UPConsole >NIL: DEVICE ON"')
    check('DEVICE ON' in out, 'S:User-Startup switches console.device at boot', out)
    run('C:UPConsole DEVICE OFF')
    run('C:UPConsole >NIL: DEVICE ON')  # the block's own line, as a boot runs it
    rc, out = run('C:UPConsole STATUS')
    check('console.device: UP-Term' in out, 'the device block\'s line switches it again', out)
    rc, out = run('VTC:iconprobe SYS:System/UP-Term')
    check('tool UP-Term:bin/vsh' in out and 'WINDOW=XCON:' in out, 'the UP-Term icon is in SYS:System, its tool UP-Term:bin/vsh (nothing in C:)', out)
    check(run('List >NIL: UP-Term:bin/vsh')[0] == 0 and run('List >NIL: C:vsh')[0] == 0,
          'Install put vsh in UP-Term:bin and left C:vsh for scripts that name it', '')
    # the icon's own tool (read from the iconprobe line), with C:vsh renamed away
    mt = re.search(r'tool (\S+)', out)
    wbtool = mt.group(1) if mt else 'UP-Term:bin/vsh'
    run('Rename C:vsh C:vsh.away')
    rc, out = run('VTC:wbrun %s SYS:System/UP-Term' % wbtool, 30)
    time.sleep(3)
    tree = ami.req(0x0D).decode('latin-1')
    check(rc == 0 and any(l.startswith('W ') and 'UP-Term' in l for l in tree.splitlines()),
          'a Workbench start of the icon opens the UP-Term window', out)
    ami.req(0x08, bytes([4]) + b'exit'); time.sleep(0.4); ami.key(0x44); time.sleep(2)
    tree = ami.req(0x0D).decode('latin-1')
    check(not any(l.startswith('W ') and 'UP-Term' in l for l in tree.splitlines()),
          'exit in it closes the window')
    run('Rename C:vsh.away C:vsh')
    if os.environ.get('INSTALL_RIG_SKIP_CONCON'):  # a rig whose CON: windows are broken (41 at 776b8f7): the later checks still run
        print('install_rig: CON: window and Shell icon checks skipped (INSTALL_RIG_SKIP_CONCON)')
    else:
        import concon_rig
        who = concon_rig.shell_conwho('kit', 'RAM:who-kit.txt')
        check(who.startswith('UP-Term '), 'a NewShell CON: window runs the kit\'s handler (%s)' % who, who)
        rc, out = run('Search S:User-Startup "C:UPConsole >NIL: CON ON"')
        check('CON ON' in out, 'S:User-Startup switches CON: at boot', out)
        run('C:UPConsole CON OFF')
        # the Shell icon from Workbench, CON: the ROM's again: its window is UP-Term's
        run('Delete RAM:who-shell.txt QUIET')
        run('VTC:wbrun SYS:System/CLI SYS:System/Shell', 30)
        time.sleep(4)
        concon_rig.typeline('VTC:conwho >RAM:who-shell.txt', 3)
        concon_rig.typeline('endcli', 2)
        who = run('Type RAM:who-shell.txt')[1].strip()
        check(who.startswith('UP-Term '), 'the Shell icon opens an UP-Term window (%s)' % who, who)
    run('C:UPConsole >NIL: CON ON')  # the block's own line, as a boot runs it
    rc, out = run('C:UPConsole STATUS')
    check('CON: UP-Term' in out, 'the block\'s line switches CON: again', out)
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == '/ENV/up-term/terminfo', 'TERMINFO is set (and can be)', out)
    rc, out = run('List >NIL: ENV:up-term/terminfo/v/vtcon')
    check(rc == 0, 'the vtcon entry is where TERMINFO points', out)
    rc, out = run('Type ENVARC:TERMINFO')  # a file now: the old ENVARC:terminfo drawer was the same name
    check(rc == 0 and out.strip() == '/ENV/up-term/terminfo', 'TERMINFO is kept in ENVARC: (no drawer by that name)', out)
    # the Unix userland: coreutils in UP-Term:bin, reached through
    # vshrc's $PATH, nothing in C:
    rc, out = run('C:vsh -c "echo $PATH"')
    check(rc == 0 and out.strip().startswith('/UP-Term/bin:'), 'vsh\'s default $PATH names the assign (/UP-Term/bin)', out)
    rc, out = run('C:vsh -c "ls --version"')
    check(rc == 0 and 'coreutils' in out and '5.2.1' in out, 'vsh\'s ls is GNU coreutils 5.2.1 (through $PATH)', out)
    check((run('List >NIL: C:ls')[0] == 0) == ls_before, 'Install put no ls in C:', '')
    # the reachability test of the Unix tool ports (unix-tool-ports plan 1.10):
    # Install put upterm-ports' grep in UP-Term:bin and vsh's $PATH reaches it
    rc, out = run('C:vsh -c "grep --version"')
    check(rc == 0 and 'GNU grep' in out and '3.12' in out, 'vsh\'s grep is the ports\' GNU grep 3.12 (through $PATH)', out)
    ports()
    # .. is the parent (on RAM:, a real volume -- VTC:'s root is its own
    # parent, an FS-UAE quirk). rm -r runs from outside: AmigaDOS keeps the
    # current directory locked, so a shell can never delete the drawer it is in
    rc, out = run('C:vsh -c "mkdir -p RAM:fu/a/b && touch RAM:fu/a/b/t && cd RAM:fu/a/b && '
                  'cp t ../u && mv ../u ../../v && cd RAM: && rm -r fu/a && ls RAM:fu"', 30)
    check(rc == 0 and out.split() == ['v'], 'cp, mv, rm, mkdir and touch are coreutils\' and .. is the parent', out)
    run('Delete >NIL: RAM:fu ALL QUIET')
    # a pipe reads as a pipe (ixemul fstat): wc -c counted 0 when it took
    # PIPE: for an empty file
    rc, out = run('C:vsh -c "echo hello | wc -c"', 30)
    check(rc == 0 and out.strip() == '6', 'echo hello | wc -c counts 6 (a pipe is not an empty file)', out)
    # sort is GNU sort in vsh (its temporary file in /tmp, TMP:), C:Sort in
    # the AmigaDOS Shell
    rc, out = run('C:vsh -c "seq 5 | sort -rn | head -2"', 30)
    check(rc == 0 and out.split() == ['5', '4'], 'seq 5 | sort -rn | head -2 (GNU sort, /tmp works)', out)
    rc, out = run('Which Sort')
    check(out.strip().lower().endswith('c/sort'), 'the AmigaDOS Shell keeps C:Sort', out)
    if not tmp_before:
        rc, out = run('Search S:User-Startup "Assign TMP: T:"')
        check('Assign TMP: T:' in out, 'S:User-Startup assigns TMP: (/tmp) at boot', out)
    check(run('List >NIL: "C:UP-Term Prefs"')[0] == 0 and
          run('List >NIL: SYS:Prefs/UP-Term-Prefs.info')[0] == 0,
          'the preferences editor is in C: (the menu\'s Preferences runs it) and in SYS:Prefs', '')
    check(run('List >NIL: ENVARC:up-term/up-term')[0] == 0, 'the window preferences file is in place', '')
    rc, out = run('C:tmux -V')
    check(rc == 0 and 'tmux 3.6a' in out, 'C:tmux runs', out)
    if not gg_before:
        rc, out = run('List >NIL: GG:bin/sh')
        check(rc == 0, 'no GG: before: vsh is GG:bin/sh (/gg/bin/sh for ixemul programs)', out)
        rc, out = run('Search S:User-Startup ";BEGIN UP-Term"')
        check(';BEGIN UP-Term' in out, 'S:User-Startup assigns GG: at boot', out)
    rc, out = run_long('rununinstall')
    check(rc == 0, 'Uninstall runs', out)
    rc, out = run('VTC:iconprobe SYS:System/Shell')
    check(out == shell_before, 'after Uninstall: the Shell icon as it was', '%r vs %r' % (out, shell_before))
    check(run('List >NIL: SYS:System/UP-Term.info')[0] != 0, 'after Uninstall: no UP-Term icon in SYS:System', '')
    check(run('List >NIL: "%s/VERSIONS"' % drawer)[0] != 0, 'after Uninstall: %s/VERSIONS is gone' % drawer, '')
    check(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'after Uninstall: no UP-Term: assign', '')
    check(run('List >NIL: ENVARC:up-term/Dir')[0] != 0, 'after Uninstall: no ENVARC:up-term/Dir', '')
    check(run('Type S:Startup-Sequence')[1] == sseq_before, 'after Uninstall: S:Startup-Sequence as it was, byte for byte', '')
    rc, out = run('VTC:UPConsole STATUS')
    check('CON: ROM' in out and 'RAW: ROM' in out, 'after Uninstall: CON: and RAW: are the ROM\'s', out)
    check('console.device: ROM' in out, 'after Uninstall: console.device is the ROM\'s', out)
    st = lib_state()
    check(st.get('ixemul.library') == before.get('ixemul.library') and 'ixemul.library.orig' not in st,
          'after Uninstall: the original ixemul back, no .orig', 'now %s, before %s' % (st, before))
    check(st.get('ixnet.library') == ixnet_before and 'ixnet.library.orig' not in st,
          'after Uninstall: the original ixnet back, no .orig', 'now %s, before %s' % (st, before))
    rc, out = run('GetEnv TERMINFO')
    check(rc == 0 and out.strip() == terminfo_before, 'after Uninstall: the TERMINFO from before Install is back', out)
    rc, out = run('Type S:User-Startup')
    check(out == startup_before, 'after Uninstall: S:User-Startup as it was, byte for byte',
          'differs: %r vs %r' % (out[-80:], startup_before[-80:]))
    if not gg_before:
        check(run('Assign >NIL: GG: EXISTS')[0] != 0, 'after Uninstall: no GG: (Install made it)')
    left = [f for f in ('DEVS:DOSDrivers/PTY', 'DEVS:DOSDrivers/XCON', 'L:pty-handler',
                        'L:vtcon-handler', 'L:ixpipe-handler', 'DEVS:DOSDrivers/IXPIPE', 'C:vsh', 'C:tmux', '"%s"' % drawer, 'ENVARC:tmux.conf', 'C:ixkill', 'ENVARC:up-term', 'ENVARC:up-term-orig', 'ENVARC:TERMINFO', 'ENVARC:vsh', 'ENV:vsh',
                        'SYS:System/UP-Term', 'SYS:System/UP-Term.info', 'C:UPConsole', 'DEVS:up-console.device',
                        '"C:UP-Term Prefs"', 'SYS:Prefs/UP-Term-Prefs', 'SYS:Prefs/UP-Term-Prefs.info')
            if run('List >NIL: %s' % f)[0] == 0]
    if not tmp_before:
        check(run('Assign >NIL: TMP: EXISTS')[0] != 0, 'after Uninstall: no TMP: (Install made it)')
    # the rig's rununinstall answers YES to the var question, so the drawer goes whole
    inside = run('List "%s" ALL' % drawer)[1][:600] if left else ''
    check(not left, 'Uninstall removed the files', ' '.join(left) + ' | ' + inside)
    check(run('List >NIL: ENVARC:UP-Term.prefs')[0] == 0, 'Uninstall kept the user\'s preferences', '')
    run('Delete >NIL: ENVARC:UP-Term.prefs QUIET')
    if rig_gg:
        run('Assign GG: VTC:gg')
    # LIBS: as the rig runs (the patched ixemul first): this test set it to
    # the stock library, and whatever runs next on the rig needs ours
    run('Avail >NIL: FLUSH')
    for line in rig_libs():
        run(line)
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

def libs():
    fx = Fixtures()
    try:
        fx.take()
        return _libs()
    finally:
        run_slow('deletelibsA', 'Delete VTC:libsA ALL QUIET')
        fx.restore()

def _libs():
    """Install and Uninstall with LIBS: a multi-assign whose first drawer holds
    no ixemul (a normal boot: Copy and Open of LIBS:<name> look only in the
    first drawer, Which finds the real one). Checked: the original is kept as
    .orig beside the real library, the kit's library replaces it there, and
    Uninstall puts the original back byte for byte, no .orig left, the
    recorded path gone."""
    global passed, total
    passed = total = 0
    prepare(None)
    run_slow('deletelibsA', 'Delete VTC:libsA ALL QUIET')
    run('MakeDir VTC:libsA')
    run_long('rununinstall')
    # DH0:Libs is where the rig's own ixemul.library and ixnet.library live
    run('Assign LIBS: VTC:libsA')
    run('Assign LIBS: %sLibs ADD' % SYSVOL)
    rc, real = run('Which LIBS:ixemul.library')
    real = real.strip()
    rc, realnet = run('Which LIBS:ixnet.library')
    realnet = realnet.strip()
    check(real.lower() == 'system:libs/ixemul.library' and realnet.lower() == 'system:libs/ixnet.library',
          'libs: the originals are in a later drawer of LIBS:, not the first', real + ' ' + realnet)
    before_em, before_net = ami.read_file(real), ami.read_file(realnet)   # compared on the Mac: the rig has no Compare
    rc, out = run_long('runinstall')
    check(rc == 0, 'libs: Install runs', out)
    check(run('List >NIL: "%s.orig"' % real)[0] == 0, 'libs: the original ixemul is kept as .orig beside the real library', '')
    check(run('List >NIL: "%s.orig"' % realnet)[0] == 0, 'libs: the original ixnet is kept as .orig beside the real library', '')
    check(ami.read_file(real) != before_em, 'libs: the kit\'s ixemul replaced the original', '')
    rc, out = run_long('rununinstall')
    check(rc == 0 and "Can't open" not in out, 'libs: Uninstall runs and does not fail to open the .orig', out)
    check(ami.read_file(real) == before_em, 'libs: the original ixemul is back, byte for byte', '')
    check(ami.read_file(realnet) == before_net, 'libs: the original ixnet is back, byte for byte', '')
    check(run('List >NIL: "%s.orig"' % real)[0] != 0 and run('List >NIL: "%s.orig"' % realnet)[0] != 0,
          'libs: no .orig left', '')
    check(run('List >NIL: ENVARC:up-term-orig')[0] != 0, 'libs: no ENVARC:up-term-orig left', '')
    check(run('List >NIL: VTC:libsA/ixemul.library')[0] != 0, 'libs: nothing was put in the first drawer', '')
    run('Delete >NIL: ENVARC:UP-Term.prefs QUIET')
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

def var_check():
    """--var: Uninstall keeps UP-Term:var on No (the script answers with
    Set upvardelete NO), a reinstall keeps the old history, Uninstall on Yes
    removes the drawer with it."""
    global passed, total
    passed = total = 0
    fx = Fixtures()
    # a clean image has a 12 MB system partition: the kit goes to its Work: drive
    D = 'Work:Apps/UP-Term' if '--os' in os.environ.get('UPTERM_RIG_FLAGS', '') else 'SYS:UP-Term'
    try:
        fx.take()
        if D.startswith('Work:'):
            run('MakeDir >NIL: Work:Apps')
        prepare(D if D.startswith('Work:') else None)
        (VTC / "rununinstall_no").write_text("Set upvardelete NO\nCD VTC:distkit\nExecute Uninstall\n")
        (VTC / "rununinstall_yes").write_text("Set upvardelete YES\nCD VTC:distkit\nExecute Uninstall\n")
        run_long('rununinstall')   # whatever an earlier run left
        run('Delete >NIL: "%s" ALL QUIET' % D)
        rc, out = run_long('runinstall')
        check(rc == 0, 'var: Install runs', out)
        run('MakeDir UP-Term:var UP-Term:var/Claude')
        run('Echo >UP-Term:var/Claude/history "old prompt"')
        rc, out = run_long('rununinstall_no')
        check(run('Type %s/var/Claude/history' % D)[1].strip() == 'old prompt', 'var: Uninstall answered No keeps var/Claude/history', out)
        check(run('List >NIL: %s/VERSIONS' % D)[0] != 0 and run('List >NIL: %s/bin' % D)[0] != 0,
              'var: ... and removes the rest of the drawer', '')
        check(('stayed in %s/var' % D) in out, 'var: ... and says where the data stayed', out)
        check(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'var: ... and removes the UP-Term: assign', '')
        rc, out = run_long('runinstall')
        check(rc == 0 and run('Type %s/var/Claude/history' % D)[1].strip() == 'old prompt',
              'var: a reinstall keeps the old history', out)
        rc, out = run_long('rununinstall_yes')
        check(run('List >NIL: %s' % D)[0] != 0, 'var: Uninstall answered Yes removes the drawer with var', out)
    finally:
        run('Delete >NIL: "%s" ALL QUIET' % D)
        fx.restore()
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

OLD = "SYS:UP-Term"
NEW = "VTC:Apps/UP-Term"

def move():
    fx = Fixtures()
    try:
        fx.take()
        return _move()
    finally:
        fx.restore()

def _move():
    """Install into the default drawer, Install again with DEST=NEW. Checked:
    the assign now and the one Assign line in S:User-Startup name NEW; exactly
    one marked UP-Term: block, still before the GG: block; a user's own line
    kept; the message names the old drawer, which stays; the same DEST again
    changes nothing and says nothing; Uninstall gives S:User-Startup back
    byte for byte (and the backup .before-UP-Term is that same file, never made
    from a file that held our block). Not verified: reboot (the assign is
    checked as the block's line would run it, not by booting)."""
    global passed, total
    passed = total = 0
    prepare(None)
    (VTC / "Apps").mkdir(exist_ok=True)
    (VTC / "runinstall_old").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE\n")
    (VTC / "runinstall_new").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files DEST=%s NOCONSOLE NODEVICE\n" % NEW)
    run_long('rununinstall')
    run('Delete >NIL: ENVARC:UP-Term.prefs ENVARC:Claude/remote S:User-Startup.before-UP-Term QUIET')
    run_slow('deleteold', 'Delete "%s" ALL QUIET' % OLD)
    rig_gg = run('Assign >NIL: GG: EXISTS')[0] == 0
    if rig_gg:
        run('Assign GG:')
    # the user's own lines, one before and one after where our blocks go
    run('Echo >>S:User-Startup "; user line, kept"')
    startup_before = run('Type S:User-Startup')[1]
    rc, out = run_long('runinstall_old')
    check(rc == 0 and 'Unknown command' not in out, 'move: Install into %s runs' % OLD, out)
    rc, aout = run('Assign LIST')
    al = [l for l in aout.splitlines() if l.lower().startswith('up-term')]
    check(al and al[0].split(None, 1)[-1].strip().lower().endswith('up-term') and 'apps' not in al[0].lower(),
          'move: UP-Term: is assigned to %s first' % OLD, aout)
    backup = run('Type S:User-Startup.before-UP-Term')[1]
    check(backup == startup_before, 'move: S:User-Startup.before-UP-Term is the file as it was', backup[-100:])
    rc, out = run_long('runinstall_new')
    check(rc == 0 and 'Unknown command' not in out, 'move: Install again with DEST=%s runs' % NEW, out)
    check('S:User-Startup now assigns UP-Term: to %s' % NEW in out, 'move: Install says the block moved', out)
    check(('old drawer %s is left in place: delete it or run Uninstall of that kit first' % OLD) in out,
          'move: the message names the old drawer and what to do', out)
    rc, aout = run('Assign LIST')
    al = [l for l in aout.splitlines() if l.lower().startswith('up-term')]
    check(len(al) == 1 and al[0].split(None, 1)[-1].strip().lower().endswith('apps/up-term'),
          'move: UP-Term: points to %s now' % NEW, aout)
    check(run('List >NIL: UP-Term:VERSIONS')[0] == 0 and run('List >NIL: "%s/VERSIONS"' % NEW)[0] == 0 and
          run('List >NIL: UP-Term:bin/sh')[0] == 0, 'move: the files are in %s, reached as UP-Term:' % NEW, '')
    check(run('List >NIL: "%s/VERSIONS"' % OLD)[0] == 0, 'move: the old drawer %s is left in place' % OLD, '')
    rc, out = run('Search S:User-Startup ";BEGIN UP-Term assign"')
    n_begin = len([l for l in out.splitlines() if ';BEGIN UP-Term assign' in l])
    rc, out2 = run('Search S:User-Startup "Assign UP-Term:"')
    lines = [l for l in out2.splitlines() if 'Assign UP-Term:' in l]
    check(n_begin == 1 and len(lines) == 1, 'move: exactly one UP-Term: block and one Assign UP-Term: line', out + out2)
    check(len(lines) == 1 and 'Apps/UP-Term' in lines[0].replace('apps/up-term', 'Apps/UP-Term') and OLD not in lines[0].upper().replace('UP-TERM', 'UP-Term'),
          'move: that line names %s, not %s' % (NEW, OLD), out2)
    rc, startup_moved = run('Type S:User-Startup')
    i_as, i_gg = startup_moved.find('Assign UP-Term:'), startup_moved.find('Assign GG:')
    check(i_as >= 0 and i_gg > i_as and '; user line, kept' in startup_moved,
          'move: the block kept its place before the GG: block, the user line is still there', startup_moved[-300:])
    check(run('Type S:User-Startup.before-UP-Term')[1] == startup_before,
          'move: the backup .before-UP-Term is still the file as it was (not made from our block)', '')
    rc, aout = run('Assign LIST')
    gg = [l for l in aout.splitlines() if l.lower().startswith('gg ')]
    check(gg and gg[0].lower().rstrip().endswith('apps/up-term'),
          'move: GG: (ours) follows UP-Term: to %s' % NEW, aout)
    # the block's own line, as a boot runs it
    run('Assign UP-Term:')
    line = [l for l in startup_moved.splitlines() if l.startswith('Assign UP-Term:')][0]
    (VTC / "upline").write_text(line + "\n")   # the block's own line ...
    run('Execute VTC:upline', 30)   # ... run as a boot runs it
    rc, aout = run('Assign LIST')
    al = [l for l in aout.splitlines() if l.lower().startswith('up-term')]
    check(al and al[0].lower().rstrip().endswith('apps/up-term') and run('List >NIL: UP-Term:bin/sh')[0] == 0,
          'move: the Assign line of the block, run as a boot runs it, assigns UP-Term: to %s' % NEW, aout)
    # the same DEST again: nothing changes, nothing is said
    rc, out = run_long('runinstall_new')
    check(rc == 0 and 'left in place' not in out and 'now assigns' not in out,
          'move: the same DEST again says nothing about the block', out)
    check(run('Type S:User-Startup')[1] == startup_moved, 'move: the same DEST again leaves S:User-Startup byte for byte', '')
    ports()   # the tools through vsh's $PATH from the moved drawer
    rc, out = run_long('rununinstall')
    check(rc == 0, 'move: Uninstall runs', out)
    check(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'move: after Uninstall no UP-Term: assign', '')
    check(run('List >NIL: "%s"' % NEW)[0] != 0, 'move: after Uninstall %s is gone' % NEW, '')
    rc, out = run('Type S:User-Startup')
    check(out == startup_before, 'move: after Uninstall S:User-Startup as it was, byte for byte (user line kept)',
          'differs: %r vs %r' % (out[-100:], startup_before[-100:]))
    check(run('List >NIL: "%s/VERSIONS"' % OLD)[0] == 0,
          'move: Uninstall of the new kit leaves the old drawer %s (the message said so)' % OLD, '')
    run_slow('deleteold', 'Delete "%s" ALL QUIET' % OLD)   # the test's own clean-up of the left drawer
    run('Delete >NIL: ENVARC:UP-Term.prefs S:User-Startup.before-UP-Term QUIET')
    if rig_gg:
        run('Assign GG: VTC:gg')
    run('Avail >NIL: FLUSH')
    for line in rig_libs():
        run(line)
    print('install_rig: passed %d of %d' % (passed, total))
    return 0 if passed == total else 1

# ---------------------------------------------------------------------------
# The cases (kit-split K2-10): one per package on the base, all of them together, the single
# kit migrated, an add-on refused without the base. Each is a function of a verdicts.Checks;
# the verdict of each is written when it ends (build/<rig>/install/<case>.json) and a case that
# passed with the same kit is skipped (RULES section 6a).

# the Install lines' keywords beyond DIR: Claude's server page, as the base's runinstallcon had it
ADDON_OPTIONS = {'claude': 'REMOTE="127.0.0.1 2399"'}
# what each add-on's Install puts down: the files its Uninstall must take away again
ADDON_FILES = {'claude': ('C:Claude', 'C:ClaudeCode'), 'pi': ('C:amiga-pi',), 'nvim': ('UP-Term:nvim/bin/nvim',),
               'python': ('UP-Term:Python3/bin/python3',)}


def addon_install_line(pkg):
    """The Shell line that installs the add-on from its unpacked archive VTC:kit-<id> (what its Installer runs part by part)."""
    opts = ADDON_OPTIONS.get(pkg)
    return 'Execute VTC:kit-%s/Files/install.dos VTC:kit-%s/Files%s' % (pkg, pkg, (' ' + opts) if opts else '')


def legacy_install_line():
    """The single kit's install.dos with the extras that moved out of the base (its .KEY still has them)."""
    return ('Execute VTC:legacykit/Files/install.dos VTC:legacykit/Files NOCONSOLE NODEVICE PYTHON NVIM '
            'REMOTE="127.0.0.1 2399"')


def prepare_legacy():
    """The single kit unpacked into VTC:legacykit with its run script."""
    if not (LEGACY_KIT / 'Files/install.dos').exists():
        raise SystemExit('install_rig: no legacy kit at %s (UPTERM_LEGACY_KIT=<drawer>; the single kit of 0cd6f0b)' % LEGACY_KIT)
    shutil.rmtree(VTC / "legacykit", ignore_errors=True)
    shutil.copytree(LEGACY_KIT, VTC / "legacykit")
    (VTC / "runinstall-legacy").write_text(legacy_install_line() + "\n")


class Before:
    """What the rig looked like before a case: the facts the end of the case compares against."""
    def __init__(self):
        run_long('rununinstall')   # a run that stopped half-way left things behind (Uninstall also runs every add-on's)
        self.startup = run('Type S:User-Startup')[1]
        self.sseq = run('Type S:Startup-Sequence')[1]
        self.libs = lib_state()
        run('Delete >NIL: ENVARC:UP-Term.prefs QUIET')
        if run('Assign >NIL: GG: EXISTS')[0] == 0:
            run('Assign GG:')   # Install's own GG: set-up is what gets tested; Fixtures puts the rig's back


def base_install(ck, line='runinstall'):
    rc, out = run_long(line)
    ck(rc == 0 and 'Unknown command' not in out, 'base Install runs', out)
    ck(run('List >NIL: UP-Term:UP-Term.version')[0] == 0 and '$VER: UP-Term' in run('Type UP-Term:UP-Term.version')[1],
       'the base wrote UP-Term:UP-Term.version (the add-ons\' base check reads it)')


def base_whole(ck, what):
    """The base still answers: vsh's ls is coreutils, and its VERSIONS is there."""
    rc, out = run('C:vsh -c "ls --version"')
    ck(rc == 0 and 'coreutils' in out, 'the base is whole %s: vsh runs GNU ls' % what, out)
    ck(run('List >NIL: UP-Term:VERSIONS')[0] == 0, 'the base is whole %s: UP-Term:VERSIONS' % what)


def base_uninstall(ck, before):
    """Uninstall of the base (which runs every add-on's own) gives the rig back byte for byte."""
    rc, out = run_long('rununinstall')
    ck(rc == 0, 'base Uninstall runs', out)
    ck(run('Type S:User-Startup')[1] == before.startup, 'after base Uninstall: S:User-Startup as it was, byte for byte',
       'differs: %r vs %r' % (run('Type S:User-Startup')[1][-80:], before.startup[-80:]))
    ck(run('Type S:Startup-Sequence')[1] == before.sseq, 'after base Uninstall: S:Startup-Sequence as it was')
    ck(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'after base Uninstall: no UP-Term: assign')
    ck(run('List >NIL: SYS:UP-Term')[0] != 0, 'after base Uninstall: SYS:UP-Term is gone', run('List SYS:UP-Term ALL')[1][:400])
    st = lib_state()
    ck(st.get('ixemul.library') == before.libs.get('ixemul.library') and 'ixemul.library.orig' not in st,
       'after base Uninstall: the original ixemul back, no .orig', 'now %s, before %s' % (st, before.libs))


def addon_absent(ck, pkg, what):
    for f in ADDON_FILES[pkg]:
        ck(run('List >NIL: %s' % f)[0] != 0, '%s: %s is not there %s' % (pkg, f, what))


def remote_banner(ck):
    """Plain Claude, no API key, connects to the computer in ENVARC:Claude/remote (W49): a banner on this Mac."""
    import socket, threading
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('127.0.0.1', 2399)); srv.listen(1); srv.settimeout(60)
    seen = []
    def banner():
        try:
            c, _ = srv.accept(); seen.append(1); c.sendall(b'UPTEST-BANNER\r\n'); time.sleep(2); c.close()
        except OSError:
            pass
    th = threading.Thread(target=banner); th.start()
    # in a window, as a user types it (uptelnet needs a console)
    ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/700/300/remote/CLOSE"')
    time.sleep(4)
    for line in ('Stack 32768', 'C:Claude'):
        ami.req(0x08, bytes([4]) + line.encode()); time.sleep(0.4); ami.key(0x44); time.sleep(2)
    th.join(30); srv.close()
    ck(seen == [1], 'claude: plain Claude connects to the computer in ENVARC:Claude/remote')
    ami.req(0x08, bytes([4]) + b'endcli'); time.sleep(0.4); ami.key(0x44); time.sleep(2)


def plant(pkg):
    """A user's file the add-on's Install and Uninstall must not touch (D5)."""
    if pkg == 'pi':
        run('MakeDir >NIL: ENVARC:amiga-pi')
        run('Echo >ENVARC:amiga-pi/settings.json "userfile-keepme"')


def addon_verify(ck, pkg, remote=True):
    """After the add-on's Install: its record, and its program answering from where Install put it (the sentinel).
    `remote`: Claude's Install was given REMOTE (the Shell line has it; the Installer's page is left empty)."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    row = {r['id']: r for r in pkgs.rows()}[pkg]
    rec = row['record']
    for f in ('MANIFEST', 'VERSIONS', 'Uninstall'):
        ck(run('List >NIL: %s/%s' % (rec, f))[0] == 0, '%s: its record has %s' % (pkg, f))
    first = run('Type %s/VERSIONS' % rec)[1].splitlines()[:1]
    ck(first and first[0].startswith('%s %s ' % (row['archive'], row['version'])),
       '%s: its VERSIONS names the archive and version' % pkg, ' '.join(first))
    if pkg == 'claude':
        rc, out = run('List C:ClaudeCode LFORMAT "%A"')
        ck(rc == 0 and out.strip()[1:2].lower() == 's', 'claude: C:ClaudeCode is a script command (s bit)', out)   # protection bits hsparwed, s second
        rc, out = run('Stack 32768\nC:Claude --version')
        ck(rc == 0 and out.strip(), 'claude: C:Claude answers --version', out)
        if remote:
            rc, out = run('Type ENVARC:Claude/remote')
            ck(rc == 0 and '127.0.0.1 2399' in out, 'claude: Install wrote ENVARC:Claude/remote', out)
            remote_banner(ck)
    elif pkg == 'pi':
        rc, out = run('C:amiga-pi --version')
        ck(rc == 0 and '1.1.0' in out, 'pi: C:amiga-pi prints --version 1.1.0', out)
        ck(run('Type ENVARC:amiga-pi/settings.json')[1].strip() == 'userfile-keepme',
           'pi: Install kept the user\'s ENVARC:amiga-pi/settings.json')
    elif pkg == 'nvim':
        # a cold nvim start can pass the agent's 120 s limit: run it through run_long
        rc, out = run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
        ck(rc == 0 and 'NVIM v0.12' in out, 'nvim: nvim runs from UP-Term:nvim', out[-300:])
    elif pkg == 'python':
        rc, out = run('Stack 1000000\nUP-Term:Python3/bin/python3 -c "print(6*7)"', 120)
        ck(rc == 0 and out.strip().splitlines()[-1:] == ['42'], 'python: python3 runs from UP-Term:Python3', out[-300:])
        # through vsh a quoted ** reaches python as typed (vsh hands its argv over out of band,
        # ixemul 39f6760/2cda2f6); the Shell's "**" is its escape for one *, so a vsh script
        (VTC / "py8.sh").write_text("python3 -c 'print(2**3)'\n")
        rc, out = run('Stack 1000000\nC:vsh VTC:py8.sh', 120)
        ck(rc == 0 and out.strip().splitlines()[-1:] == ['8'], 'python: vsh runs python3 -c \'print(2**3)\' as typed (8)', out[-300:])
        ck(run('Assign >NIL: Python3: EXISTS')[0] == 0 and
           run('Search >NIL: S:User-Startup "Assign Python3: UP-Term:Python3"')[0] == 0,
           'python: Python3: assigned, and at every boot')


def addon_gone(ck, pkg, base_stays=True, remote=True):
    """After the add-on's Uninstall (or the base's): its files, its record and its block are gone, what is yours stays."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    rec = pkgs.get(pkg, 'record')
    addon_absent(ck, pkg, 'after its Uninstall')
    if base_stays:
        ck(run('List >NIL: %s' % rec)[0] != 0, '%s: its record %s is gone' % (pkg, rec))
    if pkg == 'python':
        ck(run('Assign >NIL: Python3: EXISTS')[0] != 0, 'python: no Python3: assign')
        ck(run('Search >NIL: S:User-Startup ";BEGIN UP-Term python"')[0] != 0, 'python: its block is out of S:User-Startup')
        if base_stays:
            ck(run('Search >NIL: S:User-Startup ";BEGIN UP-Term assign"')[0] == 0,
               'python: the base\'s UP-Term: block is still in S:User-Startup (only its own block went)')
    if pkg == 'pi':
        ck(run('Type ENVARC:amiga-pi/settings.json')[1].strip() == 'userfile-keepme',
           'pi: Uninstall kept the user\'s ENVARC:amiga-pi/settings.json')
    if pkg == 'claude' and remote:
        ck(run('List >NIL: ENVARC:Claude/remote')[0] == 0, 'claude: Uninstall kept ENVARC:Claude (the user\'s settings)')
        run('Delete >NIL: ENVARC:Claude/remote ENV:Claude/remote QUIET')


def addon_install(ck, pkg):
    rc, out = run_long('runinstall-' + pkg, 3600)
    ck(rc == 0 and 'Unknown command' not in out, '%s: its Install runs' % pkg, out)
    ck(run('GetEnv UPTCHECK')[0] != 0 or not run('GetEnv UPTCHECK')[1].strip(), '%s: its check part left no reason' % pkg)
    addon_verify(ck, pkg)


def case_addon(ck, pkg):
    """The base on its own, then the add-on: its Install, the base still whole, its Uninstall, the base still whole, base Uninstall."""
    prepare(None, [pkg])
    before = Before()
    plant(pkg)
    base_install(ck)
    addon_absent(ck, pkg, 'before its Install')
    addon_install(ck, pkg)
    base_whole(ck, 'with %s installed' % pkg)
    rc, out = run_long('rununinstall-' + pkg)
    ck(rc == 0, '%s: its Uninstall runs' % pkg, out)
    addon_gone(ck, pkg)
    base_whole(ck, 'after %s\'s Uninstall' % pkg)
    base_uninstall(ck, before)


def case_all(ck):
    """The base and the four add-ons, then only the base's Uninstall: every add-on goes with it. The archives'
    own scripts run; the sentinel is each add-on's program answering before the base is removed."""
    ids = addon_ids()
    prepare(None, ids)
    before = Before()
    for pkg in ids:
        plant(pkg)
    base_install(ck)
    for pkg in ids:
        addon_install(ck, pkg)
    base_whole(ck, 'with every add-on installed')
    base_uninstall(ck, before)
    for pkg in ids:
        addon_gone(ck, pkg, base_stays=False)
    ck(run('List >NIL: UP-Term:pkg')[0] != 0, 'all: no UP-Term:pkg left')


def case_legacy(ck):
    """The single kit (LEGACY_KIT) installed, then the new base over it, then the nvim add-on: the handed files stay
    and are recorded (UP-Term:pkg/legacy), nvim still answers, and the base Uninstall takes Claude and amiga-pi
    away by the legacy block."""
    prepare(None, ['nvim'])
    prepare_legacy()
    try:
        before = Before()
        rc, out = run_long('runinstall-legacy', 3600)
        ck(rc == 0, 'legacy: the single kit\'s install.dos runs', out)
        ck(run('List >NIL: UP-Term:pkg')[0] != 0, 'legacy: the single kit leaves no UP-Term:pkg')
        for f in ('UP-Term:nvim/bin/nvim', 'UP-Term:Python3/bin/python3', 'C:Claude', 'C:amiga-pi'):
            ck(run('List >NIL: %s' % f)[0] == 0, 'legacy: the single kit installed %s' % f)
        nvim_date = run('List UP-Term:nvim/bin/nvim LFORMAT "%D %T"')[1].strip()
        base_install(ck)   # the new base over it
        rc, out = run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
        ck(rc == 0 and 'NVIM v0.12' in out, 'legacy: after the new base nvim still answers', out[-300:])
        ck(run('List UP-Term:nvim/bin/nvim LFORMAT "%D %T"')[1].strip() == nvim_date,
           'legacy: the base left the single kit\'s nvim alone (its date is unchanged)', nvim_date)
        for f in ('MANIFEST', 'VERSIONS'):
            ck(run('List >NIL: UP-Term:pkg/legacy/%s' % f)[0] == 0, 'legacy: UP-Term:pkg/legacy/%s was kept' % f)
        ck(ami.read_file('UP-Term:MANIFEST') == (kit_dir('base') / 'Files/MANIFEST').read_bytes(),
           'legacy: UP-Term:MANIFEST is the new base\'s')
        ck(run('List >NIL: C:Claude')[0] == 0 and run('List >NIL: C:amiga-pi')[0] == 0,
           'legacy: Claude and amiga-pi are still installed (handed over, not deleted)')
        rc, out = run_long('runinstall-nvim', 3600)
        ck(rc == 0 and 'Unknown command' not in out, 'legacy: the nvim add-on\'s Install runs over the single kit\'s files', out)
        ck(run('List >NIL: UP-Term:pkg/UP-Term-nvim/MANIFEST')[0] == 0, 'legacy: the nvim add-on made its record')
        rc, out = run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
        ck(rc == 0 and 'NVIM v0.12' in out, 'legacy: nvim answers after its add-on', out[-300:])
        base_uninstall(ck, before)
        for f in ('C:Claude', 'C:ClaudeCode', 'C:amiga-pi', 'UP-Term:nvim'):
            ck(run('List >NIL: %s' % f)[0] != 0, 'legacy: after the base Uninstall %s is gone (the legacy block)' % f)
        ck(run('Assign >NIL: Python3: EXISTS')[0] != 0, 'legacy: after the base Uninstall no Python3: assign')
    finally:
        run_slow('deletelegacy', 'Delete VTC:legacykit ALL QUIET')


def case_nobase(ck):
    """An add-on's install.dos with no UP-Term: assign stops with 20, names UP-Term.lha in ENV:UPTCHECK, and writes nothing."""
    ids = addon_ids()
    prepare(None, ids)
    before = Before()
    run('Assign UP-Term:')
    for pkg in ids:
        run('Delete >NIL: ENV:UPTCHECK QUIET')
        rc, out = run_long('runinstall-' + pkg)
        ck(rc == 20, '%s: install.dos without the base returns 20' % pkg, 'rc %d: %s' % (rc, out))
        ck('UP-Term.lha' in run('Type ENV:UPTCHECK')[1], '%s: ENV:UPTCHECK names UP-Term.lha' % pkg, run('Type ENV:UPTCHECK')[1])
        addon_absent(ck, pkg, 'after the refused Install')
    ck(run('Type S:User-Startup')[1] == before.startup, 'nobase: S:User-Startup as it was, byte for byte')
    ck(run('List >NIL: UP-Term:pkg')[0] != 0, 'nobase: no UP-Term:pkg')
    run('Delete >NIL: ENV:UPTCHECK QUIET')


def _global_pass(fn, *args):
    """A pass written with the module's own check(): its failures become the case's."""
    def run_(ck):
        del failures[:]
        fn(*args)
        ck.total, ck.failed = total, list(failures)
    run_.own_fixtures = True
    return run_


def _libs_case(ck):
    _global_pass(libs)(ck)
    out = run('Assign LIBS: EXISTS')[1]
    ck('ixp6' in out and 'ncurses' in out, 'base-libs: the rig\'s LIBS: is assigned again after the case', out)


def case_table():
    """{case: (packages it installs, extra files its fingerprint reads, function(ck))}, in the order they run.
    The add-on cases come from dist/packages.txt: a new row gets a case."""
    t = {'base': (['base'], [], _global_pass(main)),
         'base-dest': (['base'], [], _global_pass(main, 'VTC:Apps/UP-Term')),
         'base-move': (['base'], [], _global_pass(move)),
         'base-libs': (['base'], [], _libs_case),
         'base-var': (['base'], [], _global_pass(var_check))}
    for pkg in addon_ids():
        t[pkg] = (['base', pkg], [], (lambda ck, pkg=pkg: case_addon(ck, pkg)))
    t['all'] = (package_ids(), [], case_all)
    t['legacy'] = (['base', 'nvim'], [LEGACY_KIT / 'Files/MANIFEST', LEGACY_KIT / 'Files/install.dos'], case_legacy)
    t['nobase'] = (addon_ids(), [], case_nobase)
    return t


def case_fingerprint(entry):
    pkgs_, extra, _ = entry
    return verdicts.fingerprint(kit_fingerprint(*pkgs_).encode(), *extra)


def parse_args(argv, names):
    """(only, failed, force, show_check, list): --only/--case <case> (repeatable), the old pass flags as cases."""
    old = {'--default': 'base', '--dest': 'base-dest', '--move': 'base-move', '--var': 'base-var', '--libs': 'base-libs'}
    only, failed, force, show, lst = [], False, False, False, False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--only', '--case'):
            i += 1
            if i >= len(argv):
                raise SystemExit('%s needs a case: %s' % (a, ' '.join(names)))
            only.append(argv[i])
        elif a in old:
            only.append(old[a])
        elif a == '--failed':
            failed = True
        elif a == '--force':
            force = True
        elif a == '--show-check':
            show = True
        elif a == '--list':
            lst = True
        else:
            raise SystemExit('install_rig: unknown argument %s (see the top of the file)' % a)
        i += 1
    return only, failed, force, show, lst


def run_case(name, fn):
    """(ok, detail) of one case, its fixtures put back whatever happens."""
    ck = verdicts.Checks()
    fx = None if getattr(fn, 'own_fixtures', False) else Fixtures()
    try:
        if fx:
            fx.take()
        fn(ck)
    finally:
        if fx:
            fx.restore()
    print('install_rig %s: passed %d of %d' % (name, ck.total - len(ck.failed), ck.total))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


def cli(argv):
    table = case_table()
    only, failed, force, show, lst = parse_args(argv, list(table))
    if show:
        print('install_rig: the watched window (UPTERM_RIG_SHOW) itself')
        with paths.rig_lock():
            try:
                return show_check()
            finally:
                show_close()
    store = verdicts.Store(paths.RIG / "install")
    todo, skip = verdicts.select(store, list(table), lambda c: case_fingerprint(table[c]), only, failed, force)
    if lst:
        for c in table:
            v = store.verdict(c)
            print('%-10s %s' % (c, v['verdict'] if v else 'not run'))
        return 0
    for c, why in skip:
        print('[SKIP] %s: %s' % (c, why))
    for c, _ in todo:
        for pkg in table[c][0]:
            if not (kit_dir(pkg) / 'Install').exists():
                raise SystemExit('install_rig: no kit %s: make dist first' % kit_dir(pkg))
    with paths.rig_lock():
        try:
            for c, why in todo:
                print('install_rig: case %s (%s)' % (c, why))
                try:
                    ok, detail = run_case(c, table[c][2])
                except SystemExit as e:   # the agent or the rig is gone: not a verdict on the kit
                    print('[ABORT] %s: %s (no verdict written)' % (c, e))
                    return 2
                store.record(c, ok, detail, case_fingerprint(table[c]))
        finally:
            show_close()
    bad = store.failed([c for c, _ in todo])
    print('install_rig: %d of %d cases failed: %s' % (len(bad), len(todo), ' '.join(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(cli(sys.argv[1:]))
