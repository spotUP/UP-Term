#!/usr/bin/env python3
"""upkg_rig.py -- the real `upkg` (the opkg 0.9.0 binary, UP-Term:bin/upkg) on the rig, against the .ipk feed
build/ipk/ (`make dist-ipk`) that this Mac serves over plain http (upkg plan P10.5-5; the host decisions are
tests/test_dist_ipk.py).

FS-UAE's bsdsocket emulation connects from the Mac itself, so 127.0.0.1 on the Amiga is this Mac (wget_rig.py).
The config is UP-Term:etc/upkg.conf, asserted byte for byte equal to dist/upkg.conf, then copied to
T:upkg-rig.conf with only its `src/gz` line changed to the Mac's http feed (https is P10.7); every command runs
`upkg -f T:upkg-rig.conf ...`. Rig up (`rig.py start --max`) with the base installed from the kit of `make dist`.
The package is named as the feed names it (up-nvim, up-claude), the way `upkg list` prints it.

  version     `upkg --version` prints "based on opkg 0.9.0": the opkg binary ran, not the vsh script   (sentinel)
  updatelist  `upkg update`, then `upkg list` names the 4 add-ons
  install     `upkg install up-nvim`: files land (Files/install.dos ran under ixemul /bin/sh), the status
              record is in UP-Term:pkg/upkg/status, T:upkg-stage holds nothing for nvim (`dest root /` reached T:)
  upgrade     a feed with nvim 1.1 (ipk rebuilt in a temp drawer; the real version is untouched): `upkg upgrade`
  remove      `upkg remove up-nvim`: files, status record and record drawer gone, no T:upkg-stage left
  needsbase   UP-Term:VERSIONS faked older than nvim's needs-base: install refused, nothing written (restored)
  badsum      Packages holds a wrong SHA256sum for up-claude: install refused, nothing written

Resumable (RULES section 6a): the verdict of each case goes to build/<rig>/upkg/<case>.json with a
fingerprint of dist/upkg.conf, build/ipk/Packages and this script.
  upkg_rig.py                  every case not yet passed with this kit
  upkg_rig.py --only install   named cases (repeat it), even when they passed
  upkg_rig.py --failed         only the cases that failed last time
  upkg_rig.py --force          every case, from zero
  upkg_rig.py --list           the recorded verdicts
One script per rig: a second one on the same rig stops with the first one's pid (paths.rig_lock)."""
import gzip, http.server, os, pathlib, re, shutil, sys, tempfile, threading
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import paths  # before ami: it sets the agent port of UPTERM_RIG
import ami
import verdicts
import install_rig as ir
import pkgs

ROOT = ir.ROOT
VTC = ir.VTC
BUILD = ROOT / 'build'
OUT = paths.RIG / 'upkg'
NVIM = 'up-nvim'
CONF = 'T:upkg-rig.conf'
STATUS = 'UP-Term:pkg/upkg/status'
STAGE = 'T:upkg-stage'
ADDONS = ['up-claude', 'up-pi', 'up-nvim', 'up-python']
CASES = ['version', 'updatelist', 'install', 'upgrade', 'remove', 'needsbase', 'badsum']


# --- pure parts (tests/test_rig_fixtures.py) ---------------------------------------------------------------

def parse_packages(text):
    """[{field: value}] of the stanzas of an opkg Packages text."""
    return [dict(l.split(': ', 1) for l in b.splitlines() if ': ' in l) for b in text.split('\n\n') if b.strip()]


def edit_packages(text, pkg, **fields):
    """The Packages text with the named fields of package `pkg` replaced (Version=, SHA256sum=, ...)."""
    out, hit = [], False
    for b in text.split('\n\n'):
        ls = b.splitlines()
        if ls and ls[0] == 'Package: ' + pkg:
            hit = True
            ls = [('%s: %s' % (l.split(': ', 1)[0], fields[l.split(': ', 1)[0]])) if ': ' in l and l.split(': ', 1)[0] in fields else l
                  for l in ls]
        out.append('\n'.join(ls))
    if not hit:
        raise KeyError('no package %s in Packages' % pkg)
    return '\n\n'.join(out)


def conf_for(conf, url):
    """dist/upkg.conf with its one `src/gz` line pointing at `url`; nothing else changes."""
    lines = conf.split('\n')
    hit = [i for i, l in enumerate(lines) if l.startswith('src/gz ')]
    if len(hit) != 1:
        raise ValueError('upkg.conf needs exactly one src/gz line, has %d' % len(hit))
    lines[hit[0]] = 'src/gz upterm ' + url
    return '\n'.join(lines)


def bump_table(text, pkg, version):
    """dist/packages.txt text with the version of row `pkg` (its id) set to `version`."""
    out, n = re.subn(r'^(%s\s+\S+\s+)\S+' % re.escape(pkg), r'\g<1>' + version, text, flags=re.M)
    if n != 1:
        raise KeyError('no row %s in the table' % pkg)
    return out


def lower_record(text, version):
    """A VERSIONS record whose first line names `version` (line: <archive> <version> ...)."""
    lines = text.splitlines()
    f = lines[0].split()
    f[1] = version
    return '\n'.join([' '.join(f)] + lines[1:]) + '\n'


def status_version(status, pkg):
    """The Version of `pkg` in an opkg status file, None when it has no stanza."""
    for st in parse_packages(status):
        if st.get('Package') == pkg:
            return st.get('Version')
    return None


class Release:
    """A directory http.server serves: symlinks to the .ipk files of a feed drawer and its Packages(.gz)."""

    def __init__(self):
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix='upkgrig'))
        self.bumped = None
        owner = self

        class Quiet(http.server.SimpleHTTPRequestHandler):
            def __init__(self, *a, **k):
                super().__init__(*a, directory=str(owner.dir), **k)

            def log_message(self, *a):
                pass

        self.srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Quiet)
        self.url = 'http://127.0.0.1:%d' % self.srv.server_address[1]
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def set(self, feed_dir=None, packages=None):
        """Serve the .ipk files of `feed_dir` (default build/ipk) and `packages` (default that drawer's Packages)."""
        feed_dir = pathlib.Path(feed_dir or BUILD / 'ipk')
        for old in self.dir.iterdir():
            old.unlink()
        for f in feed_dir.glob('*.ipk'):
            (self.dir / f.name).symlink_to(f.resolve())
        text = packages if packages is not None else (feed_dir / 'Packages').read_text()
        (self.dir / 'Packages').write_text(text)
        (self.dir / 'Packages.gz').write_bytes(gzip.compress(text.encode(), mtime=0))

    def bumped_feed(self, pkg, version):
        """A temp feed drawer whose package `pkg` is `version`: its ipk rebuilt from the staged tree
        (tools/pkgs.py with a copy of the table; the real table and build/ipk are not touched)."""
        if self.bumped is None:
            tmp = pathlib.Path(tempfile.mkdtemp(prefix='upkgbump'))
            (tmp / 'dist').symlink_to(BUILD / 'dist')
            table = tmp / 'packages.txt'
            table.write_text(bump_table(pkgs.TABLE.read_text(), pkg, version))
            pkgs.ipk(tmp, table)
            pkgs.write_feed(tmp, table)
            self.bumped = tmp
        return self.bumped / 'ipk'

    def close(self):
        self.srv.shutdown()
        shutil.rmtree(self.dir, ignore_errors=True)
        if self.bumped:
            shutil.rmtree(self.bumped, ignore_errors=True)


# --- the rig side ------------------------------------------------------------------------------------------

def upkg(name, args, timeout=900, conf=True):
    """`upkg [-f CONF] <args>` from a vsh script, to completion: (rc, output)."""
    (VTC / (name + '.sh')).write_text('UP-Term:bin/upkg %s%s 2>&1\necho "[exit $?]"\n' % ('-f %s ' % CONF if conf else '', args))
    rc, out = ir.run_slow(name, 'VTC:vsh VTC:%s.sh' % name, timeout)
    m = re.search(r'\[exit (\d+)\]', out)
    return (int(m.group(1)) if m else 1000 + rc), out


def exists(path):
    return ir.run('List >NIL: %s' % path)[0] == 0


def status_text():
    return ami.read_file(STATUS).decode('latin-1') if exists(STATUS) else ''


def installed_version(pkg=NVIM):
    return status_version(status_text(), pkg)


def nvim_files_there():
    return exists('UP-Term:nvim/bin/nvim')


def nvim_pristine(ck, what):
    ck(not nvim_files_there(), '%s: UP-Term:nvim/bin/nvim is not there' % what)
    ck(not exists('UP-Term:pkg/%s' % NVIM), '%s: no record drawer UP-Term:pkg/%s' % (what, NVIM))
    ck(installed_version() is None, '%s: no status record for %s' % (what, NVIM), installed_version())
    ck(not exists('%s/%s' % (STAGE, NVIM)), '%s: nothing of nvim under %s' % (what, STAGE))


def cleanup():
    """Between cases: nvim removed through upkg the way a user does, nothing judged; the stage tree dropped."""
    if installed_version():
        upkg('upkgrm', 'remove %s' % NVIM)
    ir.run_slow('stagerm', 'Delete >NIL: %s ALL QUIET' % STAGE)


def ensure_nvim(ck, rel):
    rel.set()
    upkg('upkgupd', 'update')
    if installed_version() == '1.0':
        return True
    rc, out = upkg('upkginst', 'install %s' % NVIM)
    return ck(rc == 0 and installed_version() == '1.0', 'setup: upkg install %s' % NVIM, out[-300:])


def case_version(ck, rel):
    rc, out = upkg('upkgver', '--version', conf=False)
    ck('based on opkg 0.9.0' in out, 'version: "based on opkg 0.9.0" (the opkg binary ran, not the vsh script)', out)
    want = (ROOT / 'dist/upkg.conf').read_bytes()
    ck(ami.read_file('UP-Term:etc/upkg.conf') == want, 'version: UP-Term:etc/upkg.conf equals dist/upkg.conf byte for byte')


def case_updatelist(ck, rel):
    rel.set()
    rc, out = upkg('upkgupd', 'update')
    ck(rc == 0, 'update: exit 0', out[-300:])
    rc, out = upkg('upkglist', 'list')
    ck(rc == 0, 'list: exit 0', out[-300:])
    for a in ADDONS:
        ck(re.search(r'^%s - ' % re.escape(a), out, re.M), 'list: names %s' % a, out[-400:])


def case_install(ck, rel):
    cleanup()
    rel.set()
    upkg('upkgupd', 'update')
    rc, out = upkg('upkginst', 'install %s' % NVIM)
    ck(rc == 0, 'install: exit 0', out[-400:])
    ck(nvim_files_there(), 'install: UP-Term:nvim/bin/nvim landed (Files/install.dos ran)')
    ck(exists('UP-Term:pkg/%s/VERSIONS' % NVIM), 'install: the record UP-Term:pkg/%s/VERSIONS is there' % NVIM)
    ck(installed_version() == '1.0', 'install: %s names %s 1.0' % (STATUS, NVIM), installed_version())
    ck(not exists('%s/%s' % (STAGE, NVIM)), 'install: %s/%s left behind (dest root / reached T:, postinst dropped it)' % (STAGE, NVIM))
    rc, out = ir.run_slow('nvimverrun', 'UP-Term:nvim/bin/nvim --version', 400)
    ck(rc == 0 and 'NVIM v0.12' in out, 'install: nvim runs from UP-Term:nvim', out[-300:])


def case_upgrade(ck, rel):
    if not ensure_nvim(ck, rel):
        return
    rel.set(rel.bumped_feed('nvim', '1.1'))
    rc, out = upkg('upkgupd', 'update')
    ck(rc == 0, 'upgrade: update against the feed with nvim 1.1', out[-300:])
    rc, out = upkg('upkgupg', 'upgrade')
    ck(rc == 0, 'upgrade: upkg upgrade exit 0', out[-400:])
    ck(installed_version() == '1.1', 'upgrade: the status record moved to 1.1', installed_version())
    ck(nvim_files_there(), 'upgrade: nvim files are there')
    ck(not exists('%s/%s' % (STAGE, NVIM)), 'upgrade: nothing of nvim left under %s' % STAGE)


def case_remove(ck, rel):
    if not ensure_nvim(ck, rel):
        return
    ir.run_slow('stagerm', 'Delete >NIL: %s ALL QUIET' % STAGE)   # judge only what remove leaves
    rc, out = upkg('upkgrm', 'remove %s' % NVIM)
    ck(rc == 0, 'remove: exit 0', out[-400:])
    nvim_pristine(ck, 'remove')
    ck(not exists(STAGE), 'remove: %s is gone, empty dir too' % STAGE)


def case_needsbase(ck, rel):
    cleanup()
    rel.set()
    upkg('upkgupd', 'update')
    real = ami.read_file('UP-Term:VERSIONS').decode('latin-1')
    (VTC / 'upkg-versions').write_text(lower_record(real, '0.5'))
    try:
        ir.run('Copy VTC:upkg-versions UP-Term:VERSIONS')
        rc, out = upkg('upkgneeds', 'install %s' % NVIM)
        ck(rc != 0 and 'needs UP-Term 1.0' in out, 'needsbase: refused with "needs UP-Term 1.0"', out[-400:])
        nvim_pristine(ck, 'needsbase')
    finally:
        (VTC / 'upkg-versions').write_text(real)
        ir.run('Copy VTC:upkg-versions UP-Term:VERSIONS')
    ck(ami.read_file('UP-Term:VERSIONS').decode('latin-1') == real, 'needsbase: UP-Term:VERSIONS restored')


def case_badsum(ck, rel):
    if installed_version('up-claude') or exists('ENVARC:upkg/up-claude'):
        ck(False, 'badsum: precondition, up-claude is installed (ENVARC:upkg/up-claude): remove it first')
        return
    text = (BUILD / 'ipk/Packages').read_text()
    good = next(p for p in parse_packages(text) if p['Package'] == 'up-claude')['SHA256sum']
    rel.set(packages=edit_packages(text, 'up-claude', SHA256sum=('0' if good[0] != '0' else '1') + good[1:]))
    upkg('upkgupd', 'update')
    rc, out = upkg('upkgsum', 'install up-claude')
    ck(rc != 0 and re.search(r'sha256|checksum|corrupt package file', out, re.I), 'badsum: refused over the checksum', out[-400:])
    ck(installed_version('up-claude') is None, 'badsum: no status record for up-claude', installed_version('up-claude'))
    ck(not exists('ENVARC:upkg/up-claude'), 'badsum: no ENVARC:upkg/up-claude')
    ck(not exists('%s/up-claude' % STAGE), 'badsum: nothing of up-claude under %s' % STAGE)


TABLE = {'version': case_version, 'updatelist': case_updatelist, 'install': case_install, 'upgrade': case_upgrade,
         'remove': case_remove, 'needsbase': case_needsbase, 'badsum': case_badsum}


def fingerprint(case=None):
    return verdicts.fingerprint(ROOT / 'dist/upkg.conf', BUILD / 'ipk/Packages', pathlib.Path(__file__))


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
            raise SystemExit('upkg_rig: unknown argument %s (see the top of the file)' % a)
        i += 1
    return only, failed, force, lst


def run_case(name, rel):
    ck = verdicts.Checks()
    fx = ir.Fixtures()
    try:
        fx.take()
        (VTC / 'upkg-rig.conf').write_text(conf_for((ROOT / 'dist/upkg.conf').read_text(), rel.url))
        ir.run('Copy VTC:upkg-rig.conf %s' % CONF)
        TABLE[name](ck, rel)
    finally:
        try:
            cleanup()   # a case that installed nvim leaves the rig without it
        finally:
            ir.run('Delete >NIL: %s QUIET' % CONF)
            fx.restore()
    print('upkg_rig %s: passed %d of %d' % (name, ck.passed, ck.total))
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
        for need in [BUILD / 'ipk/Packages', BUILD / 'dist/up-nvim/Files/install.dos']:
            if not need.exists():
                raise SystemExit('upkg_rig: no %s: make dist dist-ipk first' % need)
        ir.put_vsh()   # the maintainer scripts run under GG:bin/sh: the vsh of this tree, not an older one
    with paths.rig_lock():
        rel = Release()
        try:
            for c, why in todo:
                print('upkg_rig: case %s (%s)' % (c, why))
                try:
                    ok, detail = run_case(c, rel)
                except SystemExit as e:   # the agent or the rig is gone: not a verdict on upkg
                    print('[ABORT] %s: %s (no verdict written)' % (c, e))
                    return 2
                store.record(c, ok, detail, fingerprint())
        finally:
            rel.close()
            ir.show_close()
    bad = store.failed([c for c, _ in todo])
    print('upkg_rig: %d of %d cases failed: %s' % (len(bad), len(todo), ' '.join(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    if '--help' in sys.argv or '-h' in sys.argv:
        print(__doc__)
        sys.exit(0)
    sys.exit(cli(sys.argv[1:]))
