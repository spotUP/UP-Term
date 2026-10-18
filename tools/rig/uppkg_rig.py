#!/usr/bin/env python3
"""uppkg_rig.py -- dist/uppkg on the rig: list, install, update and remove from a vsh script against a release
this Mac serves over plain http (uppkg plan P4.2; the host version of the same decisions is
tests/test_dist_uppkg.py).

The release is a temp drawer of symlinks to build/*.lha and a copy of build/uppkg-index.txt (`make dist`),
changed per case. FS-UAE's bsdsocket emulation connects from the Mac itself, so 127.0.0.1 on the Amiga is this
Mac (wget_rig.py): the rig gets `SetEnv UPPKGURL http://127.0.0.1:<port>`, which dist/uppkg reads.
Rig up with the base installed (UP-Term:bin/uppkg, curl with its https part, Installer on the rig).

  list        uppkg list shows every archive of the index; UP-Term-nvim is installed '-'
  install     uppkg install nvim: the nvim Installer's pages answered with every box ticked (installer_rig.drive),
              UP-Term:pkg/UP-Term-nvim/VERSIONS names the index version, nvim --version runs   (reachability case)
  update      the record's version lowered by hand, uppkg update reinstalls to the index version, a second
              update finds nothing to do. (A bumped index cannot be installed: the archive's Install writes the
              archive's own version, and uppkg then reports "not installed".)
  remove      uppkg remove UP-Term-nvim: the record and the nvim files are gone
  needsbase   the index says nvim needs UP-Term 99.0: refused, no Installer window opened
  badsum      the index holds a wrong sha256 for Python3: refused, nothing under ENVARC:uppkg/Python3

Resumable (RULES section 6a): the verdict of each case goes to build/<rig>/uppkg/<case>.json with a
fingerprint of dist/uppkg, the index and this script.
  uppkg_rig.py                  every case not yet passed with this kit
  uppkg_rig.py --only nvim ...  named cases (--only install; repeat it), even when they passed
  uppkg_rig.py --failed         only the cases that failed last time
  uppkg_rig.py --force          every case, from zero
  uppkg_rig.py --list           the recorded verdicts
One script per rig: a second one on the same rig stops with the first one's pid (paths.rig_lock)."""
import http.server, os, pathlib, re, shutil, sys, tempfile, threading, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import verdicts
import install_rig as ir
import installer_rig as irg

ROOT = ir.ROOT
VTC = ir.VTC
BUILD = ROOT / 'build'
OUT = paths.RIG / 'uppkg'
NVIM = 'UP-Term:pkg/UP-Term-nvim'
CASES = ['list', 'install', 'update', 'remove', 'needsbase', 'badsum']


# --- pure parts (tests/test_rig_fixtures.py) ---------------------------------------------------------------

FIELDS = ('id', 'archive', 'version', 'needs', 'record', 'size', 'sha256')


def parse_index(text):
    """[{field: value}] of the package lines of an index (the '#' header is skipped)."""
    return [dict(zip(FIELDS, l.split())) for l in text.splitlines() if l.strip() and not l.startswith('#')]


def edit_index(text, pkg, **fields):
    """The index text with the named fields of package `pkg` (its id or archive name) replaced."""
    if not set(fields) <= set(FIELDS):
        raise ValueError('index fields: ' + ' '.join(FIELDS))
    out, hit = [], False
    for l in text.splitlines():
        f = l.split()
        if f and not l.startswith('#') and pkg in f[:2]:
            row = dict(zip(FIELDS, f))
            row.update(fields)
            l, hit = ' '.join(row[k] for k in FIELDS), True
        out.append(l)
    if not hit:
        raise KeyError('no package %s in the index' % pkg)
    return '\n'.join(out) + '\n'


def lower_record(text, version):
    """A VERSIONS record whose first line names `version` (line: <archive> <version> ...)."""
    lines = text.splitlines()
    f = lines[0].split()
    f[1] = version
    return '\n'.join([' '.join(f)] + lines[1:]) + '\n'


class Release:
    """A directory http.server serves: symlinks to build/*.lha and an index, replaced per case."""

    def __init__(self):
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix='uppkgrig'))
        owner = self

        class Quiet(http.server.SimpleHTTPRequestHandler):
            def __init__(self, *a, **k):
                super().__init__(*a, directory=str(owner.dir), **k)

            def log_message(self, *a):
                pass

        self.srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Quiet)
        self.url = 'http://127.0.0.1:%d' % self.srv.server_address[1]
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def set(self, index_text):
        for f in BUILD.glob('*.lha'):
            link = self.dir / f.name
            if not link.exists():
                link.symlink_to(f)
        (self.dir / 'uppkg-index.txt').write_text(index_text)

    def close(self):
        self.srv.shutdown()
        shutil.rmtree(self.dir, ignore_errors=True)


# --- the rig side ------------------------------------------------------------------------------------------

def index_text():
    return (BUILD / 'uppkg-index.txt').read_text()


def pkg_row(pkg):
    return next(r for r in parse_index(index_text()) if pkg in (r['id'], r['archive']))


def uppkg(name, args, timeout=600):
    """`uppkg <args>` from a vsh script, to completion: (rc, output). Not for a command that opens an Installer."""
    (VTC / (name + '.sh')).write_text('UP-Term:bin/uppkg %s 2>&1\necho "[exit $?]"\n' % args)
    rc, out = ir.run_slow(name, 'VTC:vsh VTC:%s.sh' % name, timeout)
    m = re.search(r'\[exit (\d+)\]', out)
    return (int(m.group(1)) if m else 1000 + rc), out


def uppkg_gui(ck, name, args, what):
    """`uppkg <args>` where the package's Installer opens: started detached, its pages answered with every box
    ticked (installer_rig.drive; nested pages, drawer and room pages included), then waited out. Returns
    (rc, output). The Installer is named as uppkg finds it (installer_rig.find_installer)."""
    inst, base = irg.read_installer()
    for f in (name + '.out',):
        (VTC / f).unlink(missing_ok=True)
    (VTC / (name + '.sh')).write_text('UP-Term:bin/uppkg %s\necho "[exit $?]"\n' % args)
    (VTC / (name + '.run')).write_text('VTC:vsh VTC:%s.sh >VTC:%s.out\n' % (name, name))
    ir.run('Run >NIL: Execute VTC:%s.run' % name, 30)
    t0 = time.time()
    while time.time() - t0 < 900 and not irg.installer_running(inst) and not done(name):
        time.sleep(3)   # the download (curl) comes first
    finished, log = (True, ['no Installer came up']) if done(name) else irg.drive(name, inst, None, base, tick_all=True)
    (OUT / (name + '.log.txt')).write_text('\n'.join(log) + '\n')
    ck(finished, '%s: the Installer ran to its end' % what, log[-1] if log else '')
    if not finished and irg.installer_running(inst):
        ir.run('Break >NIL: `Status COM "%s"` C' % inst)
    while not done(name) and time.time() - t0 < 1800:
        time.sleep(3)
    out = read_out(name)
    m = re.search(r'\[exit (\d+)\]', out)
    return (int(m.group(1)) if m else 1000), out


def read_out(name):
    f = VTC / (name + '.out')
    return f.read_text(errors='replace') if f.exists() else ''


def done(name):
    return '[exit ' in read_out(name)


def installed_version(record=NVIM):
    rc, out = ir.run('Type %s/VERSIONS' % record)
    f = out.split()
    return f[1] if rc == 0 and len(f) > 1 else None


def nvim_files_there():
    return ir.run('List >NIL: UP-Term:nvim/bin/nvim')[0] == 0


def remove_nvim():
    """The cleanup between cases: uppkg remove, the way a user does, nothing judged."""
    if installed_version():
        uppkg('uppkgrm', 'remove UP-Term-nvim', 900)


def ensure_nvim(ck):
    """nvim installed through uppkg (a case that starts from an installed package)."""
    if installed_version() == pkg_row('nvim')['version']:
        return True
    rc, out = uppkg_gui(ck, 'uppkginst', 'install nvim', 'nvim (setup)')
    return ck(rc == 0 and installed_version() == pkg_row('nvim')['version'], 'setup: uppkg install nvim', out[-300:])


def case_list(ck, rel):
    remove_nvim()
    rel.set(index_text())
    rc, out = uppkg('uppkglist', 'list')
    ck(rc == 0, 'list: exit 0', out)
    for r in parse_index(index_text()):
        ck(re.search(r'^%s\s+\S+\s+%s\s+%s$' % (re.escape(r['archive']), re.escape(r['version']), r['size']), out, re.M),
           'list: %s %s %s bytes' % (r['archive'], r['version'], r['size']), out)
    ck(re.search(r'^UP-Term-nvim\s+-\s', out, re.M), 'list: UP-Term-nvim is installed "-"', out)


def case_install(ck, rel):
    remove_nvim()
    rel.set(index_text())
    want = pkg_row('nvim')['version']
    rc, out = uppkg_gui(ck, 'uppkginst', 'install nvim', 'install nvim')
    ck(rc == 0 and ('uppkg: UP-Term-nvim %s installed' % want) in out, 'install: uppkg says installed %s' % want, out[-300:])
    ck(installed_version() == want, 'install: %s/VERSIONS names the index version %s' % (NVIM, want), installed_version())
    rc, out = ir.run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
    ck(rc == 0 and 'NVIM v0.12' in out, 'install: nvim runs from UP-Term:nvim', out[-300:])
    ck(ir.run('List >NIL: T:uppkg')[0] != 0, 'install: T:uppkg left behind')


def case_update(ck, rel):
    rel.set(index_text())
    if not ensure_nvim(ck):
        return
    want = pkg_row('nvim')['version']
    old = '0.0.1'
    f = VTC / 'uppkg-versions'
    f.write_text(lower_record(ami.read_file(NVIM + '/VERSIONS').decode('latin-1'), old))
    ir.run('Copy VTC:uppkg-versions %s/VERSIONS' % NVIM)
    ck(installed_version() == old, 'update: the record lowered to %s' % old, installed_version())
    rc, out = uppkg_gui(ck, 'uppkgupd', 'update', 'update')
    ck(rc == 0 and 'UP-Term-nvim' in out, 'update: uppkg update reinstalls UP-Term-nvim', out[-300:])
    ck(installed_version() == want, 'update: the record is the index version %s again' % want, installed_version())
    rc, out = uppkg('uppkgupd2', 'update')
    ck(rc == 0 and 'up to date' in out, 'update: a second update finds nothing to do', out)


def case_remove(ck, rel):
    rel.set(index_text())
    if not ensure_nvim(ck):
        return
    rc, out = uppkg('uppkgrm', 'remove UP-Term-nvim', 900)
    ck(rc == 0 and 'UP-Term-nvim removed' in out, 'remove: uppkg says removed', out[-300:])
    ck(ir.run('List >NIL: %s' % NVIM)[0] != 0, 'remove: %s is gone' % NVIM)
    ck(not nvim_files_there(), 'remove: UP-Term:nvim/bin/nvim is gone')


def case_needsbase(ck, rel):
    remove_nvim()
    rel.set(edit_index(index_text(), 'nvim', needs='99.0'))
    base = {t for t, _ in irg.windows(ami.req(0x0D).decode('latin-1'))}
    rc, out = uppkg('uppkgneeds', 'install nvim')
    ck(rc != 0 and 'needs UP-Term 99.0' in out, 'needsbase: refused with "needs UP-Term 99.0"', out)
    now = {t for t, _ in irg.windows(ami.req(0x0D).decode('latin-1'))}
    ck(now <= base, 'needsbase: no Installer window opened', sorted(now - base))
    ck(installed_version() is None, 'needsbase: nothing installed', installed_version())


def case_badsum(ck, rel):
    if ir.run('List >NIL: ENVARC:uppkg/Python3')[0] == 0:
        ck(False, 'badsum: precondition, Python3 is installed (ENVARC:uppkg/Python3): remove it first')
        return
    row = pkg_row('python')
    rel.set(edit_index(index_text(), 'python', sha256=('0' if row['sha256'][0] != '0' else '1') + row['sha256'][1:]))
    rc, out = uppkg('uppkgsum', 'install python', 900)
    ck(rc != 0 and 'sha256' in out, 'badsum: refused with "sha256"', out[-300:])
    ck(ir.run('List >NIL: ENVARC:uppkg/Python3')[0] != 0, 'badsum: no ENVARC:uppkg/Python3')


TABLE = {'list': case_list, 'install': case_install, 'update': case_update, 'remove': case_remove,
         'needsbase': case_needsbase, 'badsum': case_badsum}


def fingerprint(case=None):
    return verdicts.fingerprint(ROOT / 'dist/uppkg', BUILD / 'uppkg-index.txt', pathlib.Path(__file__))


def parse_args(argv):
    only, failed, force, lst = [], False, False, False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--only', '--case'):
            i += 1
            if i >= len(argv):
                raise SystemExit('%s needs a case: %s' % (a, ' '.join(CASES)))
            only.append(argv[i])
        elif a == '--failed':
            failed = True
        elif a == '--force':
            force = True
        elif a == '--list':
            lst = True
        else:
            raise SystemExit('uppkg_rig: unknown argument %s (see the top of the file)' % a)
        i += 1
    return only, failed, force, lst


def run_case(name, rel):
    ck = verdicts.Checks()
    fx = ir.Fixtures()
    try:
        fx.take()
        ir.run('SetEnv UPPKGURL %s' % rel.url)
        TABLE[name](ck, rel)
    finally:
        try:
            remove_nvim()   # a case that installed nvim leaves the rig without it
        finally:
            ir.run('Delete >NIL: ENV:UPPKGURL')
            fx.restore()
    print('uppkg_rig %s: passed %d of %d' % (name, ck.passed, ck.total))
    return not ck.failed, '; '.join(ck.failed) or 'all %d checks' % ck.total


def cli(argv):
    only, failed, force, lst = parse_args(argv)
    OUT.mkdir(parents=True, exist_ok=True)
    store = verdicts.Store(OUT)
    todo, skip = verdicts.select(store, CASES, fingerprint, only, failed, force)
    if lst:
        for c in CASES:
            v = store.verdict(c)
            print('%-10s %s' % (c, v['verdict'] if v else 'not run'))
        return 0
    for c, why in skip:
        print('[SKIP] %s: %s' % (c, why))
    if todo:
        for need in [BUILD / 'uppkg-index.txt', BUILD / 'UP-Term-nvim.lha', BUILD / 'Python3.lha']:
            if not need.exists():
                raise SystemExit('uppkg_rig: no %s: make dist first' % need)
    with paths.rig_lock():
        rel = Release()
        try:
            for c, why in todo:
                print('uppkg_rig: case %s (%s)' % (c, why))
                try:
                    ok, detail = run_case(c, rel)
                except SystemExit as e:   # the agent or the rig is gone: not a verdict on uppkg
                    print('[ABORT] %s: %s (no verdict written)' % (c, e))
                    return 2
                store.record(c, ok, detail, fingerprint())
        finally:
            rel.close()
            ir.show_close()
    bad = store.failed([c for c, _ in todo])
    print('uppkg_rig: %d of %d cases failed: %s' % (len(bad), len(todo), ' '.join(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    if '--help' in sys.argv or '-h' in sys.argv:
        print(__doc__)
        sys.exit(0)
    sys.exit(cli(sys.argv[1:]))
