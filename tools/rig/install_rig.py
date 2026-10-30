#!/usr/bin/env python3
"""install_rig.py -- the install kits on the rig, the owner's way: unpack build/dist/<archive> into
VTC:, run its Files/install.dos (what the kit's Installer script runs), check what it did, then
Uninstall and check the rig is as before. The rig's own image is left with its original ixemul.
The rig must be up; `make dist` first.

One case per package on the base, and the combinations (K2-10, plan 2026-10-13-kit-split.md):
  base        the default drawer (SYS:UP-Term): PTY:, the patched ixemul with the original kept, the console, ...
  base-dest   a non-default drawer (DEST=VTC:KitTest/UP-Term); both reach the files through the assign UP-Term:
  base-move   Install into the default drawer, then again with DEST=VTC:KitTest/UP-Term (the assign moves)
  base-libs   LIBS: a multi-assign (the rig's normal boot)
  base-var    Uninstall and UP-Term:var (No keeps it, Yes removes it)
  claude pi nvim python
              (one per add-on of dist/packages.txt) the base, then the add-on's install.dos, its checks, its
              Uninstall, the base still whole, the base's Uninstall (the stand-alone ones, Claude amiga-pi
              Python3, work over UP-Term too)
  claude-bare pi-bare python-bare
              (one per stand-alone archive: needs-base "-") on a Workbench with no UP-Term: assign: its
              install.dos, its record in ENVARC:upkg, its program answering, its Uninstall, S:User-Startup and
              ENVARC:upkg as before
  python-ixemul
              Python3 with an ixemul.library of 47 or newer in LIBS: (the rig's patched one): left as it is,
              nothing recorded as installed by Python3, its need recorded and taken away again (the branch with
              no library there is K2-12's, see the plan)
  all         the base and every add-on; Python3 uninstalled with the base there; the base's Uninstall takes
              nvim out and leaves Claude and amiga-pi (stand-alone); then their own Uninstall, bare
  legacy      LEGACY_KIT (the single kit of 0cd6f0b) installed, the new base over it, the nvim add-on, base Uninstall
  net         Install SSH BEBBOGET: BebboSSH and bebboget at the kit's versions, ssh/scp run them, a closed port, the Mac's sshd (if one
              runs) reaches the host key check, Uninstall takes it all away (K6)
  wasabi      Install WASABI (wasabid 0.11b2): C:wasabid, key, startup block, started, the Mac's client pings it (rig 3); our old
              amiagent is removed and our wasabid 0.2 replaced; Uninstall takes it away (WM6)
  net-upgrade Install SSH over a 1.45 libcryptossh.library: with our marker it is replaced by 1.46, without it (the user's own) it is kept
              with a note and the replace command; Uninstall removes only ours (K6 fix)
  nobase      an add-on that needs the base (nvim): install.dos with no UP-Term: returns 20, ENV:UPTCHECK names up-term.lha

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
from user_startup import strip_upterm_blocks
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import verdicts

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
# the kit under test: build/dist/up-term, or UPTERM_KIT=<drawer> (build/dist is
# shared with every agent's `make dist`, so a rig run takes a private copy)
WASABI_PORT = 1233 + paths.RIG_N     # the rig's own wasabid port (tools/rig/wasabi.py: 1234 + N - 1)
KIT_WASABI_PORT = 1283 + paths.RIG_N  # case_wasabi's restart of the kit's wasabid: WASABI_PORT is the rig's own daemon
KIT = pathlib.Path(os.environ.get('UPTERM_KIT') or ROOT / "build/dist/up-term")


def assembled_scripts(pkg='base'):
    """The bytes of the package's Install, install.dos and Uninstall as tools/mkinstaller.py
    assembles them from dist/ and dist/common/ (the templates are tools/mkkit.py's): what a kit
    build writes, so a rig verdict's fingerprint moves with every part the scripts are made of
    (K5), built kit or not."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import mkinstaller, mkkit
    return [mkinstaller.build(t, pkg).encode('latin-1') for t in mkkit.templates(pkg)[:3]]


def package_ids():
    """The ids of dist/packages.txt published as an .lha, the base first (git, perl, vim are upkg-only: no Installer case)."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    return [r['id'] for r in pkgs.lha_rows()]


def addon_ids():
    return [i for i in package_ids() if i != 'base']


def standalone_ids():
    """The add-ons that need no base (needs-base "-"): Claude, amiga-pi, Python3."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    return [i for i in addon_ids() if pkgs.get(i, 'needs-base') == '-']


def based_ids():
    """The add-ons that need the base (up-nvim)."""
    return [i for i in addon_ids() if i not in standalone_ids()]


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
# The base plus every add-on is 67 MB + up-nvim 33 MB + Python3 on SYS:, and DH0 is 106 MB (217222 blocks): the
# `all` case puts the base's drawer on DH1 (Rushhours, 519 MB), as installer_rig's ROOMDEST does for the default case.
ALLDEST = "DH1:KitTest-all"

def rig_libs():
    """rig.RIG_LIBS with the system volume the rig really has (DH0: asks for a
    volume that a clean image does not have)."""
    import rig
    return [l.replace('DH0:', SYSVOL) for l in rig.RIG_LIBS]

def amissl_libs():
    """The rig's own LIBS: lines that put AmiSSL on LIBS: (rig.RIG_LIBS: the system disk's AmiSSL/Libs and the flat
    VTC:amissl): a machine with AmiSSL installed has it on LIBS:, and the kit's curl cannot start without it."""
    return [l for l in rig_libs() if 'amissl' in l.lower()]

def put_vsh():
    """build/amiga/vsh as VTC:vsh and as GG:bin/sh, the shell ixemul programs run (/gg/bin/sh: system(),
    popen(), opkg's maintainer scripts). The kit puts vsh there only when the system has no GG: yet, and the
    rig's GG: is the fixture VTCX:gg, so a rig keeps whichever vsh was copied last until this runs. A rig
    without the fixture gets none (rig.py assigns GG: at boot only when VTC:gg/bin/sh is there)."""
    import rig
    shutil.copyfile(ROOT / "build/amiga/vsh", VTC / "vsh")
    rig.refresh_kit_shadows(VTC)   # GG:bin/sh among them

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
    # FailAt 1000 covers a script that fails with 10 or 20; a `Quit 20` in it ends the wrapper anyway (the
    # agent's own return code is used below)
    (VTC / "longwrap").write_text("FailAt 1000\nExecute VTC:%s\nEcho >VTC:longdone \"$RC\"\n" % script)
    out = ''
    agent_rc = 0
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
            b = ami.req(0x02, struct.pack('>H', 120) + b'Execute VTC:longwrap', 150)
            out = b[4:].decode('latin-1')
            agent_rc = struct.unpack('>I', b[:4])[0]
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
    if agent_rc != 0 and not (VTC / "longdone").exists():
        # the command ended inside the agent's window and the wrapper never reached its Echo: a `Quit 20` in
        # the Execute'd script ends the wrapper too, whatever FailAt says (rig 2, 2026-10-13: FailAt 1000
        # did not help). The agent's own return code is the script's.
        return agent_rc, out
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
def kit_ixemul_tag():
    """The patch tag (vtcon-N) of the kit's ixemul.library, as its version string carries it."""
    return re.search(rb'\[(vtcon-\d+)', (VTC / "distkit/Files/libs/ixemul.library").read_bytes()).group(1).decode()


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

def stage_addon(pkg):
    """The add-on's archive unpacked into VTC:kit-<id>, with its runinstall-<id> and rununinstall-<id> scripts."""
    shutil.rmtree(VTC / ("kit-" + pkg), ignore_errors=True)
    shutil.copytree(kit_dir(pkg), VTC / ("kit-" + pkg))
    (VTC / ("runinstall-" + pkg)).write_text(addon_install_line(pkg) + "\n")
    (VTC / ("rununinstall-" + pkg)).write_text("Set upvardelete YES\nCD VTC:kit-%s\nExecute Uninstall\n" % pkg)


# the helper programs prepare() copies into VTC:; the Makefile's rig-helpers target builds exactly these
HELPERS = ("ptytest", "iconprobe", "wbrun", "conwho", "UPConsole")


def missing_helpers(root=None):
    """The refusal for helpers not built under root/build/amiga (make dist-base builds only some), or None."""
    root = root or ROOT
    gone = [n for n in HELPERS if not (root / "build/amiga" / n).is_file()]
    if not gone:
        return None
    return "install_rig: build/amiga/%s not built; run `make rig-helpers` first" % ", ".join(gone)


def stale_kit(kit=None):
    """The refusal for a kit that lacks a path dist/parts.txt names, or None. A make dist that stops before it
    restages leaves the old kit in place; installing it fails on the rig with "Can't open Agent" (RIG10-DISTKIT-MIXED)."""
    kit = kit or KIT
    sys.path.insert(0, str(ROOT / 'tools'))
    import mkmanifest
    gone = mkmanifest.absent(kit, mkmanifest.read_parts((ROOT / 'dist/parts.txt').read_text()))
    if not gone:
        return None
    return "install_rig: %s lacks %s from dist/parts.txt; rebuild it with make dist-base" % (kit, ", ".join(gone[:8]))


def prepare(dest, addons=()):
    """The kit unpacked into VTC:distkit, the helper programs and the run scripts beside it, LIBS: as the rig boots it.
    Each add-on in `addons` is unpacked too, into VTC:kit-<id>, with runinstall-<id> and rununinstall-<id>."""
    refusal = missing_helpers() or stale_kit()
    if refusal:  # before VTC:distkit is touched: a half-prepared VTC: mixes two kits
        raise SystemExit(refusal)
    destarg = (" DEST=%s" % dest) if dest else ""
    shutil.rmtree(VTC / "distkit", ignore_errors=True)
    shutil.copytree(KIT, VTC / "distkit")
    for pkg in addons:
        stage_addon(pkg)
    for name in HELPERS:
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    # the boot after the install puts VTC:'s copies over the kit's (rig.KIT_SHADOWS): they must be this build
    import rig
    rig.refresh_kit_shadows(VTC)
    if dest:
        # the drawer's parent must exist (VTC:KitTest/UP-Term: VTC:KitTest)
        (VTC / dest.split(':', 1)[1].rpartition('/')[0]).mkdir(parents=True, exist_ok=True)
    (VTC / "runinstall").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files%s NOCONSOLE NODEVICE\n" % destarg)
    (VTC / "runinstallcon").write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files%s CONSOLE DEVICE SHELLICON\n" % destarg)
    # the rig's Uninstall answers YES to the var question: UP-Term:var holds only
    # what earlier rig steps (amiga-pi, Claude sessions) wrote, and a kept var
    # leaves the drawer behind for the next pass (K3). var_check() covers the
    # default No with its own scripts.
    (VTC / "rununinstall").write_text("Set upvardelete YES\nCD VTC:distkit\nExecute Uninstall\n")
    stock_libs()

def stock_libs():
    """LIBS: as a machine without the rig's VTC:ixp6 has it: the rig's boot puts VTC:ixp6 first (and
    ixpty_rig.use_ixemul does), and Install would then replace and keep the copy there. prepare() sets it, and
    a case that reboots sets it again: the boot assigned VTC:ixp6 first once more, and fresh-all's Uninstall
    checks then listed VTC:ixp6's stale 7 Oct .orig files as the system's (FRESH-ALL-AFTER-END, 2026-10-29)."""
    run('Assign LIBS: %sLibs' % SYSVOL)
    run('Assign LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD')
    for line in amissl_libs():   # the AmiSSL of a machine that has it: UP-Term:bin/curl opens amisslmaster.library
        run(line.replace('Assign >NIL: ', 'Assign ', 1))

# What a run plants or removes on the Amiga that a user may own: each is saved
# before the run and put back in a finally, so a failing or killed run (the
# killed one that left 127.0.0.1 2399 in ENVARC:Claude/remote) leaves nothing.
FIXTURE_FILES = ('ENVARC:Claude/remote', 'ENV:Claude/remote', 'ENVARC:amiga-pi/settings.json', 'ENVARC:UP-Term.prefs', 'S:User-Startup')   # the move pass appends a user line to the last (ten were left by 2026-10-10)
FIXTURE_ASSIGNS = ('GG', 'UP-Term')

# the add-ons that write a ;BEGIN block to S:User-Startup, and the block's name
STARTUP_BLOCK = {'python': 'Python3'}

# what an add-on's Uninstall leaves on purpose although the run installed it (UHC stays: owner decision 2026-10-27,
# dist/addons/uhc/uninstall-body.dos): the file that shows it is there, and the lines that take it out. restore()
# puts S:User-Startup back without the ;BEGIN UHC block, so a SYS:UHC the run made must go with it: a rig that kept
# SYS:UHC but lost its block made the next run's up-uhc reuse UHC and write no block (ALL-CASE-RERUN-STALE).
# The Workbench path holds a lock on UHC:ExtBin (WBPath ADD at install), so it goes first.
LEFT_INSTALLS = {'UHC': ('SYS:UHC/UHC-Startup', (
    'WBPath REMOVE UHC:ExtBin',
    'Delete >NIL: SYS:UHC ALL QUIET FORCE',
    'Delete >NIL: ENVARC:UHC ENV:UHC ALL QUIET FORCE',
    'Delete >NIL: ENVARC:UHCBIN ENV:UHCBIN QUIET FORCE',
    'Assign >NIL: UHC:'))}


class Fixtures:
    """take() once the rig is up, restore() in a finally. restore() without take() does nothing.
    Since RIG-FRESH-SYS (2026-10-24) every rig start boots a fresh clone of the master system disk, so a killed run
    can no longer leave dirt for the NEXT run. restore() is kept only for the cases that share ONE boot (run_case
    runs them one after another on the same disk): each case starts from what the previous one left, and the master's
    own UP-Term blocks are stripped by take() (K3). It is not a cross-run safety net any more."""
    def __init__(self, files=()):
        self.paths = FIXTURE_FILES + tuple(files)
        self.files = self.assigns = None
        self.absent = ()

    def take(self):
        if self.files is not None: return
        # a killed run (pkill, the Monitor's timeout) is SIGTERM: exit through the finally so the rest of
        # THIS boot's cases see a restored disk (the next boot is fresh from the master anyway)
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
        self.files = {p: ami.read_file(p) for p in self.paths}
        self.absent = tuple(n for n, (probe, _) in LEFT_INSTALLS.items() if run('List >NIL: %s' % probe)[0] != 0)
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
            if len(f) == 2 and f[0].rstrip(':').upper() in [n.upper() for n in FIXTURE_ASSIGNS]:
                self.assigns[f[0].rstrip(':').upper()] = f[1].strip()

    def restore(self):
        if self.files is None: return
        for p, saved in self.files.items():
            try: ami.restore_file(p, saved)
            except BaseException as e: print('[WARN] fixture %s not restored: %s' % (p, e))
        for name in self.absent:
            probe, lines = LEFT_INSTALLS[name]
            try:
                if run('List >NIL: %s' % probe)[0] == 0:
                    for line in lines: run(line)
                    print('install_rig: %s was not there before the run; taken out with its block' % name)
            except BaseException as e: print('[WARN] %s not taken out: %s' % (name, e))
        for name in FIXTURE_ASSIGNS:
            try:
                if name.upper() in self.assigns: run('Assign %s: "%s"' % (name, self.assigns[name.upper()]))
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
    tag = kit_ixemul_tag()
    rc, out = run('Search LIBS:ixemul.library %s' % tag)
    check(tag in out, 'the patched ixemul (%s) is in LIBS:' % tag, out)
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
    # SHELLICON: the Shell icon starts vsh (a project's default tool) in a window on XCON:,
    # nothing else changed
    rc, out = run('VTC:iconprobe SYS:System/Shell')
    want = [re.sub(r'^type \d+', 'type 4', re.sub(r' tool \S+', ' tool UP-Term:bin/vsh', l)) if i == 0 else
            'tooltype WINDOW=XCON:' + l.split(':', 1)[1] if l.startswith('tooltype WINDOW=') else l
            for i, l in enumerate(shell_before.splitlines())]
    check(out.splitlines() == want and 'WINDOW=XCON:' in out and 'tool UP-Term:bin/vsh' in out,
          'SHELLICON: the Shell icon starts UP-Term:bin/vsh on XCON:, its window and other tooltypes kept',
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
    # its text too: the A1200's icon opened the window with "wrong number of arguments" (118) in it
    import typewin, wbtext
    shutil.copyfile(ROOT / "build/amiga/snipprobe", VTC / "snipprobe")
    typewin._last_title = 'UP-Term'
    bad = wbtext.error_lines(typewin.screen_text('RAM:icon.txt'))
    check(not bad, 'the icon\'s UP-Term window shows no error line', '; '.join(bad))
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
        # the Shell icon from Workbench, CON: the ROM's again: vsh in an UP-Term window
        # ($((1+1)) is 2 only in vsh; the AmigaDOS Echo prints it as it is)
        run('Delete RAM:who-shell.txt RAM:sh-shell.txt QUIET')
        run('VTC:wbrun UP-Term:bin/vsh SYS:System/Shell', 30)
        time.sleep(4)
        concon_rig.typeline('VTC:conwho >RAM:who-shell.txt', 3)
        concon_rig.typeline('echo $((1+1)) >RAM:sh-shell.txt', 2)
        concon_rig.typeline('exit', 2)
        who = run('Type RAM:who-shell.txt')[1].strip()
        check(who.startswith('UP-Term '), 'the Shell icon opens an UP-Term window (%s)' % who, who)
        sh = run('Type RAM:sh-shell.txt')[1].strip()
        check(sh == '2', 'the Shell icon\'s window runs vsh', sh)
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
    check(rc == 0 and 'coreutils' in out and '9.12' in out, 'vsh\'s ls is GNU coreutils 9.12 (through $PATH)', out)
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
    check(run('List >NIL: "C:UP-Term-Prefs"')[0] == 0 and
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
                        'L:vtcon-handler', 'L:ixpipe-handler', 'DEVS:DOSDrivers/IXPIPE', 'C:vsh', 'C:tmux', '"%s"' % drawer, 'ENVARC:tmux.conf', 'C:ixkill', 'ENVARC:up-term', 'ENVARC:up-term-orig', 'ENVARC:TERMINFO', 'ENVARC:vsh',
                        'SYS:System/UP-Term', 'SYS:System/UP-Term.info', 'C:UPConsole', 'DEVS:up-console.device',
                        '"C:UP-Term-Prefs"', 'SYS:Prefs/UP-Term-Prefs', 'SYS:Prefs/UP-Term-Prefs.info')
            if run('List >NIL: %s' % f)[0] == 0]
    if not tmp_before:
        check(run('Assign >NIL: TMP: EXISTS')[0] != 0, 'after Uninstall: no TMP: (Install made it)')
    # the rig's rununinstall answers YES to the var question, so the drawer goes whole
    inside = run('List "%s" ALL' % drawer)[1][:600] if left else ''
    check(not left, 'Uninstall removed the files', ' '.join(left) + ' | ' + inside)
    # ENV:vsh is emptied, not deleted (c1e66aff: 3.2's linked ENV: keeps the drawer)
    rc, out = run('List ENV:vsh')
    check(rc != 0 or 'is empty' in out, 'after Uninstall: ENV:vsh holds no files', out)
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
        # the script prints the drawer under its volume name (System:UP-Term/var), D is spelt SYS:...
        check(re.search(r'stayed in \S+:%s/var' % re.escape(D.split(':', 1)[1]), out) is not None,
              'var: ... and says where the data stayed', out)
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
NEW = "VTC:KitTest/UP-Term"
# Assign LIST prints the volume (VTCX:), not the VTC: assign: compare the path after the colon
NEW_TAIL = NEW.split(':', 1)[1].lower()

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
    check(len(al) == 1 and al[0].split(None, 1)[-1].strip().lower().endswith(NEW_TAIL),
          'move: UP-Term: points to %s now' % NEW, aout)
    check(run('List >NIL: UP-Term:VERSIONS')[0] == 0 and run('List >NIL: "%s/VERSIONS"' % NEW)[0] == 0 and
          run('List >NIL: UP-Term:bin/sh')[0] == 0, 'move: the files are in %s, reached as UP-Term:' % NEW, '')
    check(run('List >NIL: "%s/VERSIONS"' % OLD)[0] == 0, 'move: the old drawer %s is left in place' % OLD, '')
    rc, out = run('Search S:User-Startup ";BEGIN UP-Term assign"')
    n_begin = len([l for l in out.splitlines() if ';BEGIN UP-Term assign' in l])
    rc, out2 = run('Search S:User-Startup "Assign UP-Term:"')
    lines = [l for l in out2.splitlines() if 'Assign UP-Term:' in l]
    check(n_begin == 1 and len(lines) == 1, 'move: exactly one UP-Term: block and one Assign UP-Term: line', out + out2)
    check(len(lines) == 1 and NEW.lower() in lines[0].lower() and OLD.lower() not in lines[0].lower(),
          'move: that line names %s, not %s' % (NEW, OLD), out2)
    rc, startup_moved = run('Type S:User-Startup')
    i_as, i_gg = startup_moved.find('Assign UP-Term:'), startup_moved.find('Assign GG:')
    check(i_as >= 0 and i_gg > i_as and '; user line, kept' in startup_moved,
          'move: the block kept its place before the GG: block, the user line is still there', startup_moved[-300:])
    check(run('Type S:User-Startup.before-UP-Term')[1] == startup_before,
          'move: the backup .before-UP-Term is still the file as it was (not made from our block)', '')
    rc, aout = run('Assign LIST')
    gg = [l for l in aout.splitlines() if l.lower().startswith('gg ')]
    check(gg and gg[0].lower().rstrip().endswith(NEW_TAIL),
          'move: GG: (ours) follows UP-Term: to %s' % NEW, aout)
    # the block's own line, as a boot runs it
    run('Assign UP-Term:')
    line = [l for l in startup_moved.splitlines() if l.startswith('Assign UP-Term:')][0]
    (VTC / "upline").write_text(line + "\n")   # the block's own line ...
    run('Execute VTC:upline', 30)   # ... run as a boot runs it
    rc, aout = run('Assign LIST')
    al = [l for l in aout.splitlines() if l.lower().startswith('up-term')]
    check(al and al[0].lower().rstrip().endswith(NEW_TAIL) and run('List >NIL: UP-Term:bin/sh')[0] == 0,
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
ADDON_FILES = {'claude': ('C:Claude',), 'pi': ('C:amiga-pi',), 'nvim': ('UP-Term:nvim/bin/nvim',),
               'python': ('SYS:Python3/bin/python3',), 'uhc': ('SYS:UHC/C/aget',),
               'dopus': ('DOpus5:DirectoryOpus',)}

# add-ons whose Uninstall leaves their files on purpose
LEFT_BY_UNINSTALL = {'dopus': 'Directory Opus', 'uhc': 'UHC'}

def addon_install_line(pkg, options=True):
    """The Shell line that installs the add-on from its unpacked archive VTC:kit-<id> (what its Installer runs part by part).
    options=False leaves ADDON_OPTIONS out (the put-back of an owner's add-on writes no test REMOTE)."""
    opts = ADDON_OPTIONS.get(pkg) if options else None
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


PREINSTALLED = paths.RIG / 'preinstalled.json'   # survives a run that dies between the Uninstall and the put-back


def installed_addons():
    """The add-ons the rig holds now, in dist/packages.txt order: those whose record (packages.txt `record`, where
    their install.dos leaves MANIFEST, VERSIONS and options) has a MANIFEST. A record under UP-Term: counts only
    while that assign exists (asking for it otherwise puts up a requester). The leftover of a run that died
    (PREINSTALLED) is counted too: those add-ons were the owner's."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import json, pkgs
    found = []
    for pkg in addon_ids():
        rec = pkgs.get(pkg, 'record')
        if rec.upper().startswith('UP-TERM:') and run('Assign >NIL: UP-Term: EXISTS')[0] != 0:
            continue
        if run('List >NIL: %s/MANIFEST' % rec)[0] == 0:
            found.append(pkg)
    if PREINSTALLED.exists():
        found = [p for p in addon_ids() if p in found or p in json.loads(PREINSTALLED.read_text())]
    return found


class OwnersAddons:
    """The rig as its owner keeps it: Claude, amiga-pi, Python3 installed on purpose. A case checks that an add-on
    is not there before its Install, so take them out first with each archive's own Uninstall (stand-alone ones
    first, then those that need the base) and put them back from the same archive through its install.dos."""
    def __init__(self):
        self.pkgs = []

    def remove(self):
        import json
        self.pkgs = installed_addons()
        if not self.pkgs:
            return
        PREINSTALLED.write_text(json.dumps(self.pkgs))
        print('install_rig: the rig holds %s; their own Uninstall first, put back after the run' % ' '.join(self.pkgs))
        for pkg in reversed(self.pkgs):
            stage_addon(pkg)
            rc, out = run_long('rununinstall-' + pkg)
            if rc != 0:
                raise SystemExit('install_rig: Uninstall of %s failed (rc %d): %s' % (pkg, rc, out[-200:]))
        self.strip_blocks()

    def strip_blocks(self):
        """Python3's Uninstall leaves its ;BEGIN Python3 block when S:User-Startup is not what its Install kept
        (a block from an older install is in the file and no User-Startup.before was kept): "delete them by
        hand". That block is the rig's leftover, not the kit's under test; it goes the way Fixtures.take
        takes UP-Term's blocks, and the put-back writes a fresh one with its record."""
        data = ami.read_file('S:User-Startup')
        clean = data
        for pkg in self.pkgs:
            # the ;BEGIN marker is the product's name (kept across the up-* package rename), not the archive's
            if pkg in STARTUP_BLOCK:
                clean = strip_upterm_blocks(clean, STARTUP_BLOCK[pkg])
        if clean != data:
            print('install_rig: S:User-Startup held a block of an uninstalled add-on; removed')
            import tempfile
            with tempfile.NamedTemporaryFile(delete=False) as t: t.write(clean)
            try: ami.put(t.name, 'S:User-Startup')
            finally: os.unlink(t.name)

    def restore(self):
        if self.pkgs:
            # the rig with no kit has UP-Term: assigned to VTC:Apps by its boot script; the cases' base Uninstall
            # removed it, and an add-on's Install puts its launchers there only through that assign
            import rig
            (VTC / 'restoreassign').write_text(rig.UPTERM_ASSIGN + '\n')
            run_long('restoreassign')
        for pkg in self.pkgs:
            stage_addon(pkg)
            (VTC / ('runinstall-' + pkg)).write_text(addon_install_line(pkg, options=False) + '\n')
            rc, out = run_long('runinstall-' + pkg, 3600)
            if rc != 0 or 'Unknown command' in out:
                print('[WARN] %s not put back (rc %d): %s' % (pkg, rc, out[-200:]))
        if self.pkgs:
            PREINSTALLED.unlink(missing_ok=True)
            print('install_rig: put back %s' % ' '.join(self.pkgs))


class Before:
    """What the rig looked like before a case: the facts the end of the case compares against."""
    def __init__(self):
        run_long('rununinstall')   # a run that stopped half-way left things behind (Uninstall also runs every add-on's)
        self.startup = run('Type S:User-Startup')[1]
        self.sseq = run('Type S:Startup-Sequence')[1]
        self.libs = lib_state()
        # the master image (nyhd2.hdf) has an empty DH0:UP-Term from 2026-10-11 that no Install made: Uninstall must
        # give back what was there, not take that drawer away too
        self.sys_upterm = run('List SYS:UP-Term ALL')
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


def strip_block(text, name):
    """text without the lines from ';BEGIN <name>' to ';END <name>' (a block another installer wrote) and the one
    empty line that installer puts after ';END <name>' (UHC's uhcinstall writes the block and a blank before the
    file's own first line)."""
    out, skip, after_end = [], False, False
    for line in text.split('\n'):
        if after_end and line == '':
            after_end = False
            continue
        after_end = False
        if line.strip() == ';BEGIN ' + name:
            skip = True
        if not skip:
            out.append(line)
        if skip and line.strip() == ';END ' + name:
            skip = False
            after_end = True
    return '\n'.join(out)


def startup_leaves(pkgs):
    """The S:User-Startup blocks the Uninstall of these add-ons leaves in place on purpose: DOpus stays (owner rule),
    and so does its ;BEGIN DOpus5 block. UHC stays too (it is UHC's own install in SYS:UHC, owner decision
    2026-10-27, dist/addons/uhc/uninstall-body.dos), with its ;BEGIN UHC block."""
    return tuple(b for p, b in (('dopus', 'DOpus5'), ('uhc', 'UHC')) if p in pkgs)


def first_difference(now, before):
    """The first line where two texts differ, with its number, for a failed byte-for-byte check."""
    n, b = now.split('\n'), before.split('\n')
    for i in range(max(len(n), len(b))):
        x = n[i] if i < len(n) else '<end of text>'
        y = b[i] if i < len(b) else '<end of text>'
        if x != y:
            return 'first difference at line %d: now %r, before %r; now starts %r, before starts %r' % (
                i + 1, x, y, n[:4], b[:4])
    return 'identical'


def check_startup_after_uninstall(ck, before_text, now_text, leaves=(), what='after base Uninstall'):
    """S:User-Startup after the base Uninstall is as it was, byte for byte. leaves: names of blocks another
    installer wrote that Uninstall leaves in place on purpose (DOpus5: DOpus stays, owner rule); they are
    taken out of both sides for the comparison and must still be there when the run installed them."""
    a, b = before_text, now_text
    for name in leaves:
        a, b = strip_block(a, name), strip_block(b, name)
    ck(b == a, '%s: S:User-Startup as it was, byte for byte' % what, first_difference(b, a))
    for name in leaves:
        if (';BEGIN ' + name) not in before_text:
            ck((';BEGIN ' + name) in now_text and (';END ' + name) in now_text,
               '%s: the %s block is still in S:User-Startup (it still boots)' % (what, name))


def base_uninstall(ck, before, expect_user_startup_clean=True, leaves=()):
    """Uninstall of the base (which runs the Uninstall of the add-ons that need it) gives the rig back byte for byte
    (not when a stand-alone archive's block is still there: it is not the base's to take out)."""
    rc, out = run_long('rununinstall')
    ck(rc == 0, 'base Uninstall runs', out)
    if expect_user_startup_clean:
        check_startup_after_uninstall(ck, before.startup, run('Type S:User-Startup')[1], leaves)
    ck(run('Type S:Startup-Sequence')[1] == before.sseq, 'after base Uninstall: S:Startup-Sequence as it was')
    ck(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, 'after base Uninstall: no UP-Term: assign')
    now = run('List SYS:UP-Term ALL')
    ck(now == before.sys_upterm, 'after base Uninstall: SYS:UP-Term as it was before Install',
       'now rc %s %s, before rc %s %s' % (now[0], now[1][:300], before.sys_upterm[0], before.sys_upterm[1][:300]))
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


OLD_CLAUDECODE = ('C:ClaudeCode', 'UP-Term:ClaudeCode', 'UP-Term:ClaudeCode.info')   # what Claude.lha 1.0 left (CL1/NL4)


def plant_old_claude(with_base=True):
    """A 1.0 install of Claude.lha: C:ClaudeCode (a script with the s bit) and, over UP-Term, the launcher ClaudeCode
    with its icon. The new install.dos must delete them (NL4). Without the base there is no UP-Term: to put them in."""
    for f in OLD_CLAUDECODE:
        if f.startswith('UP-Term:') and not with_base:
            continue
        run('Echo >%s "old ClaudeCode of 1.0"' % f)
    run('Protect C:ClaudeCode +s')


def addon_verify(ck, pkg, remote=True, with_base=True):
    """After the add-on's Install: its record, and its program answering from where Install put it (the sentinel).
    `remote`: Claude's Install was given REMOTE (the Shell line has it; the Installer's page is left empty).
    `with_base`: UP-Term is installed (vsh answers); the bare cases of the stand-alone archives have none."""
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
        for f in OLD_CLAUDECODE:
            if f.startswith('UP-Term:') and not with_base:
                continue
            ck(run('List >NIL: %s' % f)[0] != 0, 'claude: no %s (one app since CL1; the 1.0 copy is deleted by Install)' % f)
        rc, out = run('Stack 32768\nC:Claude --version')
        ck(rc == 0 and out.strip(), 'claude: C:Claude answers --version', out)
        if remote:
            rc, out = run('Type ENVARC:Claude/remote')
            ck(rc == 0 and '127.0.0.1 2399' in out, 'claude: Install wrote ENVARC:Claude/remote', out)
            remote_banner(ck)
    elif pkg == 'pi':
        rc, out = run('C:amiga-pi --version')
        ck(rc == 0 and '1.2.0' in out, 'pi: C:amiga-pi prints --version 1.2.0', out)
        ck(run('Type ENVARC:amiga-pi/settings.json')[1].strip() == 'userfile-keepme',
           'pi: Install kept the user\'s ENVARC:amiga-pi/settings.json')
    elif pkg == 'nvim':
        # a cold nvim start can pass the agent's 120 s limit: run it through run_long
        rc, out = run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
        ck(rc == 0 and 'NVIM v0.12' in out, 'nvim: nvim runs from UP-Term:nvim', out[-300:])
    elif pkg == 'dopus':
        # the build upstream's Installer put down is the one for the CPU; the program answers on DOPUS.1 once started
        ck(run('Assign >NIL: DOpus5: EXISTS')[0] == 0, 'dopus: DOpus5: is assigned')
        rc, out = run('Version DOpus5:DirectoryOpus')
        ck(rc == 0 and 'DirectoryOpus' in out, 'dopus: DOpus5:DirectoryOpus has a version string', out)
        cpu = run('Version DOpus5:DirectoryOpus FULL')[1].strip()
        print('install_rig: dopus: Version FULL: %s' % cpu)
        run('Run >NIL: DOpus5:DirectoryOpus')
        import ami
        m = ami._m()
        res = ''
        for _ in range(20):
            time.sleep(3)
            try:
                with ami._conn() as conn:
                    res = m.arexx(conn, 'DOPUS.1', 'dopus version')[2].strip()
                break
            except m.WasabiError:
                pass
        ck(res.split()[:1] == ['5'], 'dopus: DOPUS.1 answers dopus version (first token 5)', res)
        print('install_rig: dopus version: %s' % res)
    elif pkg == 'python':
        rc, out = run('Stack 1000000\nSYS:Python3/bin/python3 -c "print(6*7)"', 120)
        ck(rc == 0 and out.strip().splitlines()[-1:] == ['42'], 'python: python3 runs from SYS:Python3', out[-300:])
        if with_base:
            # through vsh a quoted ** reaches python as typed (vsh hands its argv over out of band,
            # ixemul 39f6760/2cda2f6); the Shell's "**" is its escape for one *, so a vsh script
            (VTC / "py8.sh").write_text("python3 -c 'print(2**3)'\n")
            rc, out = run('Stack 1000000\nC:vsh VTC:py8.sh', 120)
            ck(rc == 0 and out.strip().splitlines()[-1:] == ['8'],
               'python: vsh runs python3 -c \'print(2**3)\' as typed (8; vsh\'s path names /SYS/Python3/bin)', out[-300:])
        ck(run('Assign >NIL: Python3: EXISTS')[0] == 0 and
           run('Search >NIL: S:User-Startup "Assign Python3: SYS:Python3"')[0] == 0,
           'python: Python3: assigned, and at every boot')
        ck(run('List >NIL: ENVARC:upkg/ixemul/needs/Python3')[0] == 0, 'python: its need of ixemul.library is recorded')


def addon_absent_not(ck, pkg, what):
    """The add-on's files are still there (a stand-alone archive after the base's Uninstall)."""
    for f in ADDON_FILES[pkg]:
        ck(run('List >NIL: %s' % f)[0] == 0, '%s: %s is still there %s' % (pkg, f, what))


def addon_gone(ck, pkg, base_stays=True, remote=True, with_base=True):
    """After the add-on's Uninstall (or the base's): its files, its record and its block are gone, what is yours stays."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import pkgs
    rec = pkgs.get(pkg, 'record')
    if pkg in LEFT_BY_UNINSTALL:   # left in place by its Uninstall (owner rules, dist/addons/<pkg>/uninstall-body.dos)
        for f in ADDON_FILES[pkg]:
            ck(run('List >NIL: %s' % f)[0] == 0, '%s: %s is still there after its Uninstall (%s stays)' % (pkg, f, LEFT_BY_UNINSTALL[pkg]))
    else:
        addon_absent(ck, pkg, 'after its Uninstall')
    if base_stays:
        ck(run('List >NIL: %s' % rec)[0] != 0, '%s: its record %s is gone' % (pkg, rec))
    if pkg == 'python':
        ck(run('Assign >NIL: Python3: EXISTS')[0] != 0, 'python: no Python3: assign')
        ck(run('Search >NIL: S:User-Startup ";BEGIN Python3"')[0] != 0, 'python: its block is out of S:User-Startup')
        ck(run('List >NIL: ENVARC:upkg/ixemul/needs/Python3')[0] != 0, 'python: its need of ixemul.library is taken away')
        if base_stays and with_base:
            ck(run('Search >NIL: S:User-Startup ";BEGIN UP-Term assign"')[0] == 0,
               'python: the base\'s UP-Term: block is still in S:User-Startup (only its own block went)')
    if pkg == 'pi':
        ck(run('Type ENVARC:amiga-pi/settings.json')[1].strip() == 'userfile-keepme',
           'pi: Uninstall kept the user\'s ENVARC:amiga-pi/settings.json')
    if pkg == 'claude' and remote:
        ck(run('List >NIL: ENVARC:Claude/remote')[0] == 0, 'claude: Uninstall kept ENVARC:Claude (the user\'s settings)')
        run('Delete >NIL: ENVARC:Claude/remote ENV:Claude/remote QUIET')


def addon_install(ck, pkg, with_base=True):
    rc, out = run_long('runinstall-' + pkg, 3600)
    ck(rc == 0 and 'Unknown command' not in out, '%s: its Install runs' % pkg, out)
    ck('insert volume' not in out.lower(), '%s: no requester for a volume' % pkg, out)
    ck(run('GetEnv UPTCHECK')[0] != 0 or not run('GetEnv UPTCHECK')[1].strip(), '%s: its check part left no reason' % pkg)
    addon_verify(ck, pkg, with_base=with_base)


def case_addon(ck, pkg):
    """The base on its own, then the add-on: its Install, the base still whole, its Uninstall, the base still whole, base Uninstall."""
    prepare(None, [pkg])
    before = Before()
    plant(pkg)
    base_install(ck)
    addon_absent(ck, pkg, 'before its Install')
    if pkg == 'claude':
        plant_old_claude()
    addon_install(ck, pkg)
    base_whole(ck, 'with %s installed' % pkg)
    rc, out = run_long('rununinstall-' + pkg)
    ck(rc == 0, '%s: its Uninstall runs' % pkg, out)
    addon_gone(ck, pkg)
    base_whole(ck, 'after %s\'s Uninstall' % pkg)
    base_uninstall(ck, before, leaves=startup_leaves([pkg]))


def case_bare(ck, pkg):
    """A stand-alone archive on a Workbench with no UP-Term: assign and no base: its install.dos, its record in
    ENVARC:upkg, its program answering, its Uninstall, and the rig as before (S:User-Startup byte for byte, no
    record left). No `UP-Term:` object is touched by the scripts (that would put up a requester: the run_long
    wrapper's output has none)."""
    prepare(None, [pkg])
    before = Before()
    plant(pkg)
    run('Assign UP-Term:')
    ck(run('Assign >NIL: UP-Term: EXISTS')[0] != 0, '%s-bare: there is no UP-Term: assign' % pkg)
    addon_absent(ck, pkg, 'before its Install')
    if pkg == 'claude':
        plant_old_claude(with_base=False)
    addon_install(ck, pkg, with_base=False)
    rc, out = run_long('rununinstall-' + pkg)
    ck(rc == 0, '%s: its Uninstall runs' % pkg, out)
    addon_gone(ck, pkg, with_base=False)
    check_startup_after_uninstall(ck, before.startup, run('Type S:User-Startup')[1], startup_leaves([pkg]),
                                  '%s-bare' % pkg)
    ck(run('List >NIL: ENVARC:upkg/%s' % archive(pkg))[0] != 0, '%s-bare: its record is gone' % pkg)
    ck(run('List >NIL: ENVARC:upkg')[0] != 0, '%s-bare: ENVARC:upkg is gone with its last record' % pkg)


def case_python_ixemul(ck):
    """THE IXEMUL.LIBRARY RULE, the branch the rig's LIBS: can show: a library of 47 or newer is there (the rig boots
    with the patched 80): Python3's Install leaves it as it is (same size and date), records only its need, and its
    Uninstall leaves the library alone. The branch with none or an older library there (Python3 installs its own,
    keeps the older as .before-Python3, removes it again unless UP-Term or another package needs it) needs a LIBS:
    without ixemul and is a hand step of the plan (K2-12)."""
    prepare(None, ['python'])
    before = Before()
    run('Assign UP-Term:')
    sig = run('List LIBS:ixemul.library LFORMAT "%L %D %T"')[1].strip()
    ck(bool(sig), 'python-ixemul: the rig has an ixemul.library in LIBS:')
    addon_install(ck, 'python', with_base=False)
    ck(run('List LIBS:ixemul.library LFORMAT "%L %D %T"')[1].strip() == sig, 'python-ixemul: Install left the ixemul.library as it was')
    ck(run('List >NIL: ENVARC:upkg/ixemul/installed-by-Python3')[0] != 0, 'python-ixemul: nothing recorded as installed by Python3')
    ck(run('List >NIL: LIBS:ixemul.library.before-Python3')[0] != 0, 'python-ixemul: no .before-Python3 made')
    rc, out = run_long('rununinstall-python')
    ck(rc == 0, 'python: its Uninstall runs', out)
    ck(run('List LIBS:ixemul.library LFORMAT "%L %D %T"')[1].strip() == sig, 'python-ixemul: Uninstall left the ixemul.library alone')
    ck(run('List >NIL: ENVARC:upkg/ixemul')[0] != 0, 'python-ixemul: ENVARC:upkg/ixemul is gone')
    ck(run('Type S:User-Startup')[1] == before.startup, 'python-ixemul: S:User-Startup as it was, byte for byte')


def case_all(ck):
    """The base and every add-on. Python3 is uninstalled while the base is there (UP-Term's unstartup.sh takes its
    block out); the base's Uninstall then removes up-nvim by its own Uninstall and leaves Claude and amiga-pi
    (stand-alone: record in ENVARC:upkg); their own Uninstall, with no base, ends it. The archives' own scripts
    run; the sentinel is each add-on's program answering before the base is removed."""
    ids = addon_ids()
    prepare(ALLDEST, ids)
    before = Before()
    for pkg in ids:
        plant(pkg)
    base_install(ck)
    for pkg in ids:
        addon_install(ck, pkg)
    base_whole(ck, 'with every add-on installed')
    rc, out = run_long('rununinstall-python')
    ck(rc == 0, 'python: its Uninstall runs with the base there', out)
    addon_gone(ck, 'python', remote=False)
    base_uninstall(ck, before, expect_user_startup_clean=False)
    for pkg in based_ids():
        addon_gone(ck, pkg, base_stays=False)
    rest = [p for p in standalone_ids() if p != 'python']
    for pkg in rest:
        addon_absent_not(ck, pkg, 'after the base Uninstall (stand-alone: the base leaves it)')
        ck(run('List >NIL: ENVARC:upkg/%s/MANIFEST' % archive(pkg))[0] == 0, '%s: its record survives the base Uninstall' % pkg)
    ck(run('List >NIL: C:uptelnet')[0] == 0, 'all: C:uptelnet stays while Claude\'s record is there')
    ck(run('List >NIL: UP-Term:pkg')[0] != 0, 'all: no UP-Term:pkg left')
    for pkg in rest:
        rc, out = run_long('rununinstall-' + pkg)
        ck(rc == 0, '%s: its Uninstall runs with no base' % pkg, out)
        addon_gone(ck, pkg, base_stays=False, with_base=False)
    ck(run('List >NIL: C:uptelnet')[0] != 0, 'all: C:uptelnet goes with the last of Claude and the base')
    check_startup_after_uninstall(ck, before.startup, run('Type S:User-Startup')[1], startup_leaves(ids), 'all')
    ck(run('List >NIL: ENVARC:upkg')[0] != 0, 'all: ENVARC:upkg is gone with the last record')


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
        ck(run('List >NIL: UP-Term:pkg/up-nvim/MANIFEST')[0] == 0, 'legacy: the nvim add-on made its record')
        rc, out = run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
        ck(rc == 0 and 'NVIM v0.12' in out, 'legacy: nvim answers after its add-on', out[-300:])
        base_uninstall(ck, before)
        for f in ('C:Claude', 'C:ClaudeCode', 'C:amiga-pi', 'UP-Term:nvim'):
            ck(run('List >NIL: %s' % f)[0] != 0, 'legacy: after the base Uninstall %s is gone (the legacy block)' % f)
        ck(run('Assign >NIL: Python3: EXISTS')[0] != 0, 'legacy: after the base Uninstall no Python3: assign')
    finally:
        run_slow('deletelegacy', 'Delete VTC:legacykit ALL QUIET')


def case_nobase(ck):
    """An add-on that needs the base: its install.dos with no UP-Term: assign stops with 20, names up-term.lha in ENV:UPTCHECK, and writes nothing."""
    ids = based_ids()
    prepare(None, ids)
    before = Before()
    run('Assign UP-Term:')
    for pkg in ids:
        run('Delete >NIL: ENV:UPTCHECK QUIET')
        rc, out = run_long('runinstall-' + pkg)
        ck(rc == 20, '%s: install.dos without the base returns 20' % pkg, 'rc %d: %s' % (rc, out))
        ck('up-term.lha' in run('Type ENV:UPTCHECK')[1], '%s: ENV:UPTCHECK names up-term.lha' % pkg, run('Type ENV:UPTCHECK')[1])
        addon_absent(ck, pkg, 'after the refused Install')
    ck(run('Type S:User-Startup')[1] == before.startup, 'nobase: S:User-Startup as it was, byte for byte')
    ck(run('List >NIL: UP-Term:pkg')[0] != 0, 'nobase: no UP-Term:pkg')
    run('Delete >NIL: ENV:UPTCHECK QUIET')


def case_net(ck):
    """The network tools (Install SSH BEBBOGET, K6): the kit's BebboSSH version is the one in UP-Term:bin and
    answers, ssh and scp are vsh functions of it, a closed port gives its connect error, and (sshd on the Mac
    only, never started here) the key exchange with the Mac's sshd reaches the host key check with the Mac's
    own ed25519 key. Uninstall takes the programs and the library it put there away again."""
    import socket, subprocess
    prepare(None)
    before = Before()
    bv = sorted((KIT / 'Files/net').glob('bebbossh-*'))[0].name.split('-', 1)[1]
    gv = sorted((KIT / 'Files/net').glob('bebboget-*'))[0].name.split('-', 1)[1]
    (VTC / 'runinstallnet').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE SSH BEBBOGET\n")
    base_install(ck, 'runinstallnet')
    for prog in ('bebbossh', 'bebboscp', 'bebbosshd', 'bebbosshkeygen'):
        rc, out = run('Search UP-Term:bin/%s "VER: %s"' % (prog, bv))
        ck(rc == 0, 'net: UP-Term:bin/%s is BebboSSH %s ($VER)' % (prog, bv), out)
    rc, out = run('Search UP-Term:bin/bebboget "VER: amigaget %s"' % gv)
    ck(rc == 0, 'net: UP-Term:bin/bebboget is %s ($VER)' % gv, out)
    ck(run('List >NIL: LIBS:libcryptossh.library')[0] == 0, 'net: LIBS:libcryptossh.library is there')
    rc, out = run('UP-Term:bin/bebbossh')
    ck('$VER: %s' % bv in out and 'USAGE: bebbossh' in out, 'net: bebbossh prints $VER %s and its usage' % bv, out[:200])
    rc, out = run('C:vsh -c "scp"')
    ck('$VER: %s' % bv in out and 'USAGE: amigascp' in out, 'net: scp (vsh function) runs bebboscp %s' % bv, out[:200])
    rc, out = run('C:vsh -c "ssh nobody@127.0.0.1:2"')
    ck(rc == 14 and "can't connect to given host" in out, 'net: ssh to a closed port: error 14, cannot connect', out)
    if socket.socket().connect_ex(('127.0.0.1', 22)) == 0:
        scan = subprocess.run('ssh-keyscan -t ed25519 127.0.0.1 2>/dev/null | ssh-keygen -lf -', shell=True,
                              capture_output=True, text=True).stdout.split()
        fp = scan[1].split(':', 1)[1] if len(scan) > 1 else '?'
        rc, out = run('C:vsh -c "ssh nobody@127.0.0.1 </dev/null"', 40)
        ck(rc == 21 and ('ssh-ed25519 key %s' % fp) in out,
           "net: ssh to the Mac's sshd: key exchange done, the host key shown is the Mac's (%s)" % fp, out)
    else:
        print('install_rig net: no sshd on 127.0.0.1:22 here; the Mac connection is not checked')
    base_uninstall(ck, before)
    ck(run('List >NIL: LIBS:libcryptossh.library')[0] != 0, 'net: after Uninstall LIBS:libcryptossh.library is gone')
    ck(run('List >NIL: UP-Term:bin/bebbossh')[0] != 0, 'net: after Uninstall bebbossh is gone')


def case_net_upgrade(ck):
    """Install SSH over an earlier install (K6 fix): LIBS:libcryptossh.library is a 1.45 one.
    A) with our marker (ENVARC:up-term/cryptossh): replaced by the kit's 1.46, marker kept, Uninstall removes it.
    B) without the marker (the user's own BebboSSH): kept as it is, Install prints the note and the replace command, Uninstall
       leaves it. The old library is the 1.45 one of the 020 build from the git history of dist/net/bebbossh-1.45."""
    import subprocess
    old = subprocess.run(['git', '-C', str(ROOT), 'show', 'f0727e4^:dist/net/bebbossh-1.45/libcryptossh.library020'],
                         capture_output=True).stdout
    ck(len(old) > 10000 and b'libcryptossh 1.45 ' in old, 'net-upgrade: the 1.45 library is at hand (git history)', len(old))
    prepare(None)
    (VTC / 'old145.library').write_bytes(old)
    (VTC / 'runinstallnet').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE SSH\n")
    before = Before()
    def seed(marker):
        run('Avail FLUSH >NIL:')
        rc, out = run('Copy VTC:old145.library LIBS:libcryptossh.library CLONE')
        ck(rc == 0, 'net-upgrade: seeded LIBS:libcryptossh.library with 1.45', out)
        if marker:
            run('MakeDir >NIL: ENVARC:up-term')
            run('Echo >ENVARC:up-term/cryptossh "installed by UP-Term"')
    def is_ver(v):
        return run('Search >NIL: LIBS:libcryptossh.library "libcryptossh %s "' % v)[0] == 0
    try:
        seed(True)
        ck(is_ver('1.45'), 'net-upgrade A: the seeded library is 1.45')
        rc, out = run_long('runinstallnet')
        ck(rc == 0, 'net-upgrade A: Install SSH runs', out)
        ck(is_ver('1.46') and not is_ver('1.45'), 'net-upgrade A: marked 1.45 library replaced by 1.46 ($VER libcryptossh 1.46)')
        ck('is your own' not in out, 'net-upgrade A: no note about a user library', out)
        ck(run('List >NIL: ENVARC:up-term/cryptossh')[0] == 0, 'net-upgrade A: the marker is still there')
        base_uninstall(ck, before)
        ck(run('List >NIL: LIBS:libcryptossh.library')[0] != 0, 'net-upgrade A: Uninstall removed our library')
        prepare(None)
        (VTC / 'old145.library').write_bytes(old)
        (VTC / 'runinstallnet').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE SSH\n")
        seed(False)
        rc, out = run_long('runinstallnet')
        ck(rc == 0, 'net-upgrade B: Install SSH runs', out)
        ck(is_ver('1.45') and not is_ver('1.46'), "net-upgrade B: the user's own 1.45 library is kept")
        ck('is your own and older' in out and 'Copy <kit>/Files/net/bebbossh-1.46.1/libcryptossh.library0' in out,
           'net-upgrade B: the note and the replace command are printed', out[-600:])
        ck(run('List >NIL: ENVARC:up-term/cryptossh')[0] != 0, 'net-upgrade B: no marker written')
        base_uninstall(ck, before)
        ck(is_ver('1.45'), "net-upgrade B: Uninstall leaves the user's library")
        # a current library of the user's own (the kit's 020 one, no marker): kept, no note
        run('Copy VTC:distkit/Files/net/bebbossh-1.46.1/libcryptossh.library020 LIBS:libcryptossh.library CLONE')
        prepare(None)
        (VTC / 'runinstallnet').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE SSH\n")
        rc, out = run_long('runinstallnet')
        ck(is_ver('1.46') and 'is your own' not in out, "net-upgrade C: the user's own 1.46 library: kept, no note", out[-300:])
        run_long('rununinstall')
    finally:
        run('Delete >NIL: LIBS:libcryptossh.library ENVARC:up-term/cryptossh QUIET')


def case_stale(ck):
    """An Install over an older Install refreshes what the kit ships: the key file (ENVARC: and ENV:, the old
    different one saved as keymap.bak, a line says so) and each font a kit marked ours (ENVARC:up-term/font-*),
    while a font without the marker (the user's own) is kept."""
    prepare(None)
    before = Before()
    base_install(ck)
    shipped = run('Type ENVARC:up-term/keymap')[1]
    ck(len(shipped) > 100, 'stale: the first Install wrote ENVARC:up-term/keymap', shipped[:80])
    old_font_marked, old_font_user = 'OLD MARKED FONT BYTES', 'USER OWN FONT BYTES'
    (VTC / 'stale_keymap').write_text('; an older kit keymap\nsuper+q = quit\n')
    (VTC / 'stale_font_marked').write_text(old_font_marked)
    (VTC / 'stale_font_user').write_text(old_font_user)
    run('Copy VTC:stale_keymap ENVARC:up-term/keymap')
    run('Copy VTC:stale_keymap ENV:up-term/keymap')
    run('Delete >NIL: ENVARC:up-term/keymap.bak ENVARC:up-term/keymap.bak2 QUIET')
    (VTC / 'stale_marker').write_text('installed by UP-Term\n')
    run('Copy VTC:stale_marker ENVARC:up-term/font-IBM-11')
    run('Copy VTC:stale_font_marked FONTS:IBM/11')              # marker present: ours from an earlier kit
    run('Copy VTC:stale_font_user FONTS:IBM/16')               # no marker: the user's own
    run('Delete >NIL: ENVARC:up-term/font-IBM-16 QUIET')
    ck(run('Type ENVARC:up-term/keymap')[1].startswith('; an older kit keymap') and run('Type ENV:up-term/keymap')[1].startswith('; an older'),
       'stale: set-up: the older keymap is planted in ENVARC: and ENV:')
    ck(run('Type FONTS:IBM/11')[1].strip() == old_font_marked and run('Type FONTS:IBM/16')[1].strip() == old_font_user
       and run('List >NIL: ENVARC:up-term/font-IBM-11')[0] == 0 and run('List >NIL: ENVARC:up-term/font-IBM-16')[0] != 0,
       'stale: set-up: IBM/11 old and marked, IBM/16 old and unmarked')
    rc, out = run_long('runinstall')
    ck(rc == 0, 'stale: the Install over the older one runs', out)
    ck('keymap.bak' in out, 'stale: the Install says where the old keymap is saved', out[-300:])
    ck(run('Type ENVARC:up-term/keymap')[1] == shipped, 'stale: ENVARC:up-term/keymap is the shipped one again')
    ck(run('Type ENV:up-term/keymap')[1] == shipped, 'stale: ENV:up-term/keymap is the shipped one again')
    ck(run('Type ENVARC:up-term/keymap.bak')[1].startswith('; an older kit keymap'), 'stale: the old keymap is saved as keymap.bak')
    ck(run('List >NIL: ENVARC:up-term/keymap.new')[0] != 0, 'stale: no keymap.new is written')
    ck(run('Type FONTS:IBM/11')[1].strip() != old_font_marked, 'stale: the marked old font FONTS:IBM/11 is replaced')
    ck(run('Type FONTS:IBM/16')[1].strip() == old_font_user, 'stale: the unmarked FONTS:IBM/16 is kept')
    rc, out = run_long('runinstall')
    ck('keymap.bak' not in out, 'stale: a third Install over the same keymap saves nothing', out[-300:])
    base_uninstall(ck, before)
    ck(run('List >NIL: FONTS:IBM/16')[0] == 0, 'stale: Uninstall leaves the user\'s own FONTS:IBM/16')
    run('Delete >NIL: FONTS:IBM/16 ENVARC:up-term/keymap.bak QUIET')


def case_shellalias(ck):
    """Install's upkg Alias in S:Shell-Startup: one marked block after the base Install, a new AmigaDOS Shell
    (Shell FROM, which runs S:Shell-Startup) resolves upkg, a second Install leaves one block, Uninstall takes
    the block out and the rest of the file stays."""
    prepare(None)
    before = Before()
    shell_before = run('Type S:Shell-Startup')[1]
    def blocks():
        rc, out = run('Search S:Shell-Startup ";BEGIN UP-Term upkg"')
        return out.count(';BEGIN UP-Term upkg')
    base_install(ck)
    ck(blocks() == 1, 'shellalias: S:Shell-Startup holds one ";BEGIN UP-Term upkg" block', run('Type S:Shell-Startup')[1])
    ck('Alias upkg UP-Term:bin/upkg []' in run('Type S:Shell-Startup')[1], 'shellalias: the block holds the upkg Alias')
    # a NewShell process (FROM replaces its own S:Shell-Startup run, so the script executes the file, as a Shell does)
    (VTC / 'chkalias').write_text("Execute S:Shell-Startup\nAlias >VTC:aliaslist\nupkg --help >NIL:\nEcho >VTC:aliasrc \"rc=$RC\"\nEndCLI\n")
    run('Delete >NIL: VTC:aliasrc VTC:aliaslist QUIET')
    run('NewShell "CON:0/12/400/100/aliaschk/CLOSE" FROM VTC:chkalias', 90)
    out = ''
    for _ in range(20):
        time.sleep(2)
        rc, out = run('Type VTC:aliasrc')
        if rc == 0:
            break
    ck('rc=0' in out, 'shellalias: a new Shell resolves upkg and upkg --help gives rc 0', out + run('Type VTC:aliaslist')[1][-300:])
    base_install(ck)
    ck(blocks() == 1, 'shellalias: after a second Install still one block', run('Type S:Shell-Startup')[1])
    base_uninstall(ck, before)
    ck(blocks() == 0, 'shellalias: Uninstall removed the block', run('Type S:Shell-Startup')[1])
    after = run('Type S:Shell-Startup')[1]
    ck(after.strip() == shell_before.strip(),
       'shellalias: the rest of S:Shell-Startup is as it was', 'now %r before %r' % (after[-120:], shell_before[-120:]))


def case_wasabi(ck):
    """The kit's one daemon option (Install WASABI, WM6): wasabid 0.11b2 lands in C: with our marker, a key
    (ENVARC:wasabi.key), a marked block in S:User-Startup, and is started; it answers a ping from the Mac's
    client (restarted on a port of its own, KIT_WASABI_PORT: the rig's wasabid, which run() talks through,
    holds WASABI_PORT and the Mac's 1234 is rig 1's; this case is for rig 3).
    Earlier kits' daemons go: our amiagent (UP-Term:bin/amiagent + marker) is deleted, our wasabid 0.2 is
    replaced. Uninstall takes the daemon, its key and the block away again."""
    import subprocess
    old = subprocess.run(['git', '-C', str(ROOT), 'show', '063bed5:dist/wasabi/wasabid'], capture_output=True).stdout
    ck(len(old) > 10000 and b'wasabid 0.2' in old, 'wasabi: the 0.2 wasabid of the old kit is at hand (git history)', len(old))
    prepare(None)
    (VTC / 'old-wasabid').write_bytes(old)
    rigkey = run('Type ENV:wasabi.key')[1].strip()         # the rig's own (rig.py wasabi on); wasabikey keeps a key that is set
    run('Delete >NIL: ENV:wasabi.key ENVARC:wasabi.key QUIET')
    before = Before()
    (VTC / 'runinstallbase').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE\n")
    (VTC / 'runinstallwasabi').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE WASABI\n")
    base_install(ck, 'runinstallbase')
    ck(run('List >NIL: C:wasabid')[0] != 0, 'wasabi: not ticked, no wasabid')
    # earlier kits left these: our amiagent copy and our wasabid 0.2 with its markers
    ck(run('Copy VTC:old-wasabid UP-Term:bin/amiagent CLONE')[0] == 0, 'wasabi: planted an old amiagent file')
    run('Echo >ENVARC:up-term/amiagent "installed by UP-Term"')
    ck(run('Copy VTC:old-wasabid C:wasabid CLONE')[0] == 0, 'wasabi: planted the old wasabid 0.2')
    run('Echo >ENVARC:up-term/wasabid "installed by UP-Term"')
    rc, out = run_long('runinstallbase')
    ck(rc == 0, 'wasabi: Install without WASABI runs', out)
    ck(run('List >NIL: UP-Term:bin/amiagent')[0] != 0 and run('List >NIL: ENVARC:up-term/amiagent')[0] != 0,
       'wasabi: Install removed our old amiagent and its marker')
    ck(run('List >NIL: C:wasabid')[0] != 0 and 'older wasabid' in out,
       'wasabi: unticked, our wasabid 0.2 is removed, with a note', out[-300:])
    ck(run('Copy VTC:old-wasabid C:wasabid CLONE')[0] == 0, 'wasabi: planted the old wasabid 0.2 again')
    run('Echo >ENVARC:up-term/wasabid "installed by UP-Term"')
    rc, out = run_long('runinstallwasabi')
    ck(rc == 0, 'wasabi: Install WASABI runs', out)
    ck(run('Search >NIL: C:wasabid "wasabid 0.11b2"')[0] == 0 and run('Search >NIL: C:wasabid "wasabid 0.2"')[0] != 0,
       'wasabi: C:wasabid is 0.11b2 (the 0.2 replaced)')
    ck(run('List >NIL: ENVARC:up-term/wasabid')[0] == 0, 'wasabi: our marker is there')
    rc, key = run('Type ENVARC:wasabi.key')
    key = key.strip()
    ck(rc == 0 and len(key) > 8, 'wasabi: a key was made (ENVARC:wasabi.key)', '%d %r' % (rc, key))
    rc2, key2 = run('Type ENV:wasabi.key')
    ck(rc2 == 0 and key2.strip() == key, 'wasabi: ENV:wasabi.key (what wasabid reads) is the same', '%d %r' % (rc2, key2))
    ck(key != rigkey, 'wasabi: the kit made a key of its own')
    ck(';BEGIN UP-Term wasabi' in run('Type S:User-Startup')[1], 'wasabi: the S:User-Startup block is there')
    ck(run('Status >NIL: COM C:wasabid')[0] == 0, 'wasabi: Install started wasabid')
    # restart on a port of its own: WASABI_PORT is held by the rig's wasabid (a second bind there fails and the
    # ping reaches the rig's daemon, which answers 'bad key'), the Mac's end of 1234 is rig 1's
    run('Status COM C:wasabid >T:wpid')
    rc, pid = run('Type T:wpid')
    pid = pid.split()[0] if pid.split() else ''
    ck(pid.isdigit(), 'wasabi: found the running wasabid', pid)
    run('Break %s C' % pid)
    time.sleep(3)
    run('Run >NIL: <NIL: C:wasabid %d name rig%d' % (KIT_WASABI_PORT, paths.RIG_N))
    time.sleep(3)
    client = ROOT / 'build/wasabi/wasabi'
    r = subprocess.run([sys.executable, '-I', str(client), '--host', '127.0.0.1', '--port', str(KIT_WASABI_PORT),
                        '--key', key, '--plain', 'ping'], capture_output=True, text=True, timeout=60)
    ck(r.returncode == 0 and 'wasabid 0.11b2' in (r.stdout + r.stderr), 'wasabi: the Mac client pings the kit-installed wasabid',
       (r.stdout + r.stderr)[:300])
    run('Status COM C:wasabid >T:wpid')
    pid = (run('Type T:wpid')[1].split() or [''])[0]
    run('Break %s C' % pid)
    time.sleep(3)
    base_uninstall(ck, before)
    ck(run('List >NIL: C:wasabid')[0] != 0, 'wasabi: after Uninstall C:wasabid is gone')
    ck(run('List >NIL: ENVARC:wasabi.key')[0] != 0, 'wasabi: after Uninstall the key is gone')
    ck(run('List >NIL: ENVARC:up-term/wasabid')[0] != 0, 'wasabi: after Uninstall the marker is gone')
    if rigkey:
        run('SetEnv wasabi.key %s' % rigkey)


def case_wasabi_owned(ck):
    """UNINSTALL-WASABI-BLOCK: a wasabid the owner put in C: before Install (no marker of ours), with no line in
    S:User-Startup. Install WASABI writes its block (it found no C:wasabid line); Uninstall takes the block out
    but keeps C:wasabid and leaves a plain `Run >NIL: C:wasabid` line in its place (the owner, 2026-10-25), so
    the owner's daemon still starts at boot."""
    import subprocess
    old = subprocess.run(['git', '-C', str(ROOT), 'show', '063bed5:dist/wasabi/wasabid'], capture_output=True).stdout
    ck(len(old) > 10000, 'wasabi_owned: a wasabid binary is at hand (git history)', len(old))
    prepare(None)
    (VTC / 'old-wasabid').write_bytes(old)
    before = Before()
    (VTC / 'runinstallwasabi').write_text("Execute VTC:distkit/Files/install.dos VTC:distkit/Files NOCONSOLE NODEVICE WASABI\n")
    ck(run('Copy VTC:old-wasabid C:wasabid CLONE')[0] == 0, "wasabi_owned: planted the owner's C:wasabid, no marker")
    ck('C:wasabid' not in before.startup, 'wasabi_owned: S:User-Startup has no wasabid line before Install')
    try:
        rc, out = run_long('runinstallwasabi')
        ck(rc == 0, 'wasabi_owned: Install WASABI runs', out)
        ck(run('List >NIL: ENVARC:up-term/wasabid')[0] != 0, 'wasabi_owned: no marker (the wasabid was there before)')
        ck(';BEGIN UP-Term wasabi' in run('Type S:User-Startup')[1], 'wasabi_owned: Install wrote its block')
        run('Status COM C:wasabid >T:wpid')
        pid = (run('Type T:wpid')[1].split() or [''])[0]
        if pid.isdigit():
            run('Break %s C' % pid)
            time.sleep(3)
        base_uninstall(ck, before, expect_user_startup_clean=False)
        after = run('Type S:User-Startup')[1]
        ck(run('List >NIL: C:wasabid')[0] == 0, "wasabi_owned: after Uninstall the owner's C:wasabid stays")
        ck(';BEGIN UP-Term wasabi' not in after, 'wasabi_owned: after Uninstall the block is gone', after[-200:])
        ck(after == before.startup + 'Run >NIL: C:wasabid\n',
           'wasabi_owned: after Uninstall S:User-Startup is as it was plus the plain wasabid line',
           'now %r before %r' % (after[-120:], before.startup[-120:]))
    finally:
        run('Delete >NIL: C:wasabid QUIET')
        (VTC / 'startup-before').write_text(before.startup)
        run('Copy VTC:startup-before S:User-Startup CLONE QUIET')


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
         'base-dest': (['base'], [], _global_pass(main, 'VTC:KitTest/UP-Term')),
         'base-move': (['base'], [], _global_pass(move)),
         'base-libs': (['base'], [], _libs_case),
         'base-var': (['base'], [], _global_pass(var_check))}
    for pkg in addon_ids():
        t[pkg] = (['base', pkg], [], (lambda ck, pkg=pkg: case_addon(ck, pkg)))
    for pkg in standalone_ids():
        t[pkg + '-bare'] = ([pkg], [], (lambda ck, pkg=pkg: case_bare(ck, pkg)))
    t['python-ixemul'] = (['python'], [], case_python_ixemul)
    t['all'] = (package_ids(), [], case_all)
    t['legacy'] = (['base', 'nvim'], [LEGACY_KIT / 'Files/MANIFEST', LEGACY_KIT / 'Files/install.dos'], case_legacy)
    t['nobase'] = (based_ids(), [], case_nobase)
    t['net'] = (['base'], [], case_net)
    t['net-upgrade'] = (['base'], [], case_net_upgrade)
    t['wasabi'] = (['base'], [], case_wasabi)
    t['wasabi-owned'] = (['base'], [], case_wasabi_owned)
    t['shellalias'] = (['base'], [], case_shellalias)
    t['stale'] = (['base'], [], case_stale)
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
        owners = OwnersAddons()
        try:
            if todo:
                try:
                    owners.remove()
                except SystemExit as e:
                    print('[ABORT] %s (no verdict written)' % e)
                    return 2
            for c, why in todo:
                print('install_rig: case %s (%s)' % (c, why))
                try:
                    ok, detail = run_case(c, table[c][2])
                except SystemExit as e:   # the agent or the rig is gone: not a verdict on the kit
                    print('[ABORT] %s: %s (no verdict written)' % (c, e))
                    return 2
                store.record(c, ok, detail, case_fingerprint(table[c]))
        finally:
            try:
                owners.restore()
            except BaseException as e:
                print('[WARN] the owner\'s add-ons are not put back: %s (%s lists them)' % (e, PREINSTALLED))
            show_close()
    bad = store.failed([c for c, _ in todo])
    print('install_rig: %d of %d cases failed: %s' % (len(bad), len(todo), ' '.join(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(cli(sys.argv[1:]))
