#!/usr/bin/env python3
"""upkg_rig.py -- the real `upkg` (the opkg 0.9.0 binary, UP-Term:bin/upkg) on the rig, against the .ipk feed
build/ipk/ (`make dist-ipk`) that this Mac serves over plain http (upkg plan P10.5-5; the host decisions are
tests/test_dist_ipk.py).

FS-UAE's bsdsocket emulation connects from the Mac itself, so 127.0.0.1 on the Amiga is this Mac (wget_rig.py).
The config is UP-Term:etc/upkg.conf, asserted byte for byte equal to dist/upkg.conf, then copied to
T:upkg-rig.conf with only its `src/gz` line changed to the Mac's http feed (https is P10.7); every command runs
`upkg -f T:upkg-rig.conf ...`. Rig up (`rig.py start --max`); the base of the kit of `make dist` is installed first when the rig has none (or an older one)
and uninstalled again at the end when this run installed it.
The package is named as the feed names it (up-nvim, up-claude), the way `upkg list` prints it.

  version     `upkg --version` prints "based on opkg 0.9.0": the opkg binary ran, not the vsh script   (sentinel)
  updatelist  `upkg update`, then `upkg list` names the 4 add-ons
  install     `upkg install up-nvim`: files land (Files/install.dos ran under ixemul /bin/sh), the status
              record is in UP-Term:pkg/upkg/status, T:upkg-stage holds nothing for nvim (`dest root /` reached T:)
  upgrade     a feed with nvim 1.1 (ipk rebuilt in a temp drawer; the real version is untouched): `upkg upgrade`
  migrate     the OLD name (UP-Term-nvim 1.0, no Replaces, its record in UP-Term:pkg/UP-Term-nvim) is installed from a temp
              feed; the real feed then names up-nvim with Replaces/Conflicts/Provides: UP-Term-nvim. `upkg upgrade`
              alone is run and recorded; `upkg install up-nvim` must remove UP-Term-nvim (its prerm runs the old
              Uninstall, no requester) and leave only up-nvim, files and the up-nvim record drawer   (sentinel: the
              opkg line that removes/replaces UP-Term-nvim)
  remove      `upkg remove up-nvim`: files, status record and record drawer gone, no T:upkg-stage left
  needsbase   UP-Term:VERSIONS faked older than nvim's needs-base: install refused, nothing written (restored)
  badsum      Packages holds a wrong SHA256sum for up-claude: install refused, nothing written
  appaddon    an app add-on of tools/mkappaddons.py (up-nano): `upkg install up-nano` lands UP-Term:bin/nano and
              its record, nano runs (sentinel: "GNU nano" from `nano --version`); `upkg remove up-nano` takes the
              binary, UP-Term:share/nano, the record drawer and the status record away again
  zipaddon    the same for up-zip (zip left the base for an add-on): UP-Term:bin/zip lands, `zip -h` prints
              "Zip 3.0" (sentinel), remove takes it away again

Resumable (RULES section 6a): the verdict of each case goes to build/<rig>/upkg/<case>.json with a
fingerprint of dist/upkg.conf, build/ipk/Packages and this script.
  upkg_rig.py                  every case not yet passed with this kit
  upkg_rig.py --only install   named cases (repeat it), even when they passed
  upkg_rig.py --failed         only the cases that failed last time
  upkg_rig.py --force          every case, from zero
  upkg_rig.py --list           the recorded verdicts
One script per rig: a second one on the same rig stops with the first one's pid (paths.rig_lock)."""
import gzip, http.server, os, pathlib, re, shutil, subprocess, sys, tempfile, threading, time
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
CASES = ['version', 'updatelist', 'install', 'upgrade', 'migrate', 'remove', 'needsbase', 'badsum', 'listnew', 'appaddon', 'zipaddon']
OLD_NVIM = 'UP-Term-nvim'


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


def old_name_table(text, pkg, old):
    """A packages table text holding only row `pkg`, its archive and record drawer spelled `old`."""
    keep = [l for l in text.splitlines() if re.match(r'^%s\s' % re.escape(pkg), l)]
    if len(keep) != 1:
        raise KeyError('no row %s in the table' % pkg)
    new = re.split(r'\s{2,}', keep[0].strip())[1]
    return keep[0].replace('pkg/' + new, 'pkg/' + old).replace('  ' + new + ' ', '  ' + old + ' ', 1) + '\n'


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


def staged_dist(row_id=None):
    """build/dist of this checkout, else of the main checkout a worktree belongs to (staging needs the ports tree):
    the first holding every package's install.dos, or the one of row `row_id`."""
    rows = [r for r in pkgs.addons() if row_id in (None, r['id'])]
    for base in (BUILD, ROOT.parent.parent.parent / 'build'):
        if all((base / 'dist' / r['archive'] / 'Files/install.dos').is_file() for r in rows):
            return base / 'dist'
    return BUILD / 'dist'


class Release:
    """A directory http.server serves: symlinks to the .ipk files of a feed drawer and its Packages(.gz)."""

    def __init__(self):
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix='upkgrig'))
        self.bumped = None
        self.old = None
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

    def bumped_feed(self, pkg, version, only=False):
        """A temp feed drawer whose package `pkg` is `version`: its ipk rebuilt from the staged tree
        (tools/pkgs.py with a copy of the table; the real table and build/ipk are not touched)."""
        if self.bumped is None:
            tmp = pathlib.Path(tempfile.mkdtemp(prefix='upkgbump'))
            (tmp / 'dist').symlink_to(staged_dist(pkg if only else None))
            table = tmp / 'packages.txt'
            text = bump_table(pkgs.TABLE.read_text(), pkg, version)
            if only:   # a checkout whose build/dist stages only some packages: the feed holds just `pkg`
                text = '\n'.join(l for l in text.splitlines() if not l.strip() or l.lstrip().startswith('#') or re.match(r'^%s\s' % re.escape(pkg), l)) + '\n'
            table.write_text(text)
            pkgs.ipk(tmp, table)
            pkgs.write_feed(tmp, table)
            self.bumped = tmp
        return self.bumped / 'ipk'

    def old_name_feed(self, pkg, old):
        """A temp feed drawer holding the package of row `pkg` as it was before its rename: named `old`, the
        record drawer UP-Term:pkg/<old> (the staged tree copied with its new name spelled as the old one in
        install.dos and Uninstall), no Replaces fields (old_names(old) is empty)."""
        if self.old is None:
            new = pkgs.get(pkg, 'archive')
            tmp = pathlib.Path(tempfile.mkdtemp(prefix='upkgold'))
            (tmp / 'dist').mkdir()
            subprocess.run(['cp', '-Rc', str(BUILD / 'dist' / new), str(tmp / 'dist' / old)], check=True)
            for rel in ('Files/install.dos', 'Uninstall'):
                f = tmp / 'dist' / old / rel
                f.write_bytes(f.read_bytes().replace(new.encode(), old.encode()))
            table = tmp / 'packages.txt'
            table.write_text(old_name_table(pkgs.TABLE.read_text(), pkg, old))
            pkgs.ipk(tmp, table)
            pkgs.write_feed(tmp, table)
            self.old = tmp
        return self.old / 'ipk'

    def close(self):
        self.srv.shutdown()
        if self.old:
            shutil.rmtree(self.old, ignore_errors=True)
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
    if installed_version(OLD_NVIM):
        upkg('upkgrmold', 'remove %s' % OLD_NVIM)
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


def case_migrate(ck, rel):
    cleanup()
    ir.run_slow('olddrawerrm', 'Delete >NIL: UP-Term:pkg/%s ALL QUIET' % OLD_NVIM)
    rel.set(rel.old_name_feed('nvim', OLD_NVIM))
    upkg('upkgupd', 'update')
    rc, out = upkg('upkginstold', 'install %s' % OLD_NVIM)
    ck(rc == 0 and installed_version(OLD_NVIM) == '1.0', 'migrate: setup: upkg install %s' % OLD_NVIM, out[-400:])
    ck(exists('UP-Term:pkg/%s' % OLD_NVIM), 'migrate: setup: the old record drawer UP-Term:pkg/%s is there' % OLD_NVIM)
    rel.set()
    rc, out = upkg('upkgupd', 'update')
    ck(rc == 0, 'migrate: update against the real feed', out[-300:])
    rc, out = upkg('upkgupg', 'upgrade')                       # recorded, not judged: what upgrade alone does
    print('migrate: `upkg upgrade` alone: exit %d, status %s=%s %s=%s\n%s' % (rc, OLD_NVIM, installed_version(OLD_NVIM), NVIM, installed_version(), out[-600:]))
    rc, out = upkg('upkginst', 'install %s' % NVIM)
    print('migrate: `upkg install %s` output:\n%s' % (NVIM, out[-1500:]))
    ck(rc == 0, 'migrate: upkg install %s exit 0' % NVIM, out[-600:])
    ck(re.search(r'(Removing|Replacing|Replaces|removing).*%s' % OLD_NVIM, out), 'migrate: opkg removes/replaces %s (sentinel)' % OLD_NVIM, out[-800:])
    ck(installed_version() == '1.0', 'migrate: status holds %s' % NVIM, installed_version())
    ck(installed_version(OLD_NVIM) is None, 'migrate: status does not hold %s' % OLD_NVIM, installed_version(OLD_NVIM))
    ck(nvim_files_there(), 'migrate: nvim files are there')
    ck(not exists('UP-Term:pkg/%s' % OLD_NVIM), 'migrate: old record drawer UP-Term:pkg/%s is gone' % OLD_NVIM)
    ck(exists('UP-Term:pkg/%s' % NVIM), 'migrate: record drawer UP-Term:pkg/%s is there' % NVIM)


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


def without_package(text, pkg):
    """The Packages text without the stanza of `pkg`."""
    return '\n\n'.join(b for b in text.split('\n\n') if b.strip() and not b.startswith('Package: %s\n' % pkg)) + '\n\n'


def case_listnew(ck, rel):
    """patch 0023/0022 on the rig: update keeps <feed>.prev, list-new prints what the last update added,
    (host: pkgs/opkg/check/list_new.sh)."""
    pkg = 'up-pi'
    full = (BUILD / 'ipk/Packages').read_text()
    lists = 'UP-Term:pkg/upkg/lists'
    ir.run('Delete >NIL: %s/#? QUIET' % lists)
    rel.set(packages=without_package(full, pkg))
    rc, out = upkg('lnupd1', 'update')
    ck(rc == 0, 'listnew: first update exit 0', out[-300:])
    ck(not exists('%s/upterm.prev' % lists), 'listnew: no .prev after the first update')
    rc, out = upkg('lnnew1', 'list-new')
    ck(rc == 0 and not out.replace('[exit 0]', '').strip(), 'listnew: list-new prints nothing after the first update', out[-300:])
    rel.set()
    rc, out = upkg('lnupd2', 'update')
    ck(rc == 0, 'listnew: second update exit 0', out[-300:])
    ck(exists('%s/upterm.prev' % lists), 'listnew: upterm.prev kept by the second update')
    rc, want = upkg('lnlist', 'list %s' % pkg)
    rc2, got = upkg('lnnew2', 'list-new')
    strip = lambda t: re.sub(r'\[exit \d+\]', '', t).strip()
    ck(rc == 0 and rc2 == 0 and strip(want).startswith(pkg) and strip(got) == strip(want),
       'listnew: list-new prints exactly the row of %s' % pkg, 'want %r got %r' % (want[-200:], got[-200:]))
    rel.set()
    upkg('lnupd3', 'update')


def app_addon(ck, rel, case, pkg, files, sentinel, want):
    """An app add-on of tools/mkappaddons.py: install lands `files` and the record, `sentinel` prints `want`,
    remove takes the files, the record drawer and the status record away again."""
    tag = case[:4]
    rel.set()
    upkg(tag + 'upd', 'update')
    if installed_version(pkg):
        upkg(tag + 'rm0', 'remove %s' % pkg)
    rc, out = upkg(tag + 'inst', 'install %s' % pkg)
    ck(rc == 0, '%s: install exit 0' % case, out[-400:])
    ck(exists(files[0]), '%s: %s landed (Files/install.dos ran)' % (case, files[0]))
    ck(exists('UP-Term:pkg/%s/VERSIONS' % pkg), '%s: the record UP-Term:pkg/%s/VERSIONS is there' % (case, pkg))
    ck(installed_version(pkg) == '1.0', '%s: %s names %s 1.0' % (case, STATUS, pkg), installed_version(pkg))
    ck(not exists('%s/%s' % (STAGE, pkg)), '%s: %s/%s left behind' % (case, STAGE, pkg))
    rc, out = ir.run_slow(tag + 'run', sentinel, 200)
    ck(rc == 0 and want in out, '%s: %s runs from UP-Term:bin (sentinel)' % (case, files[0]), out[-300:])
    rc, out = upkg(tag + 'rm', 'remove %s' % pkg)
    ck(rc == 0, '%s: remove exit 0' % case, out[-400:])
    for f in files:
        ck(not exists(f), '%s: remove took %s away' % (case, f))
    ck(not exists('UP-Term:pkg/%s' % pkg), '%s: remove took the record drawer away' % case)
    ck(installed_version(pkg) is None, '%s: no status record for %s after remove' % (case, pkg), installed_version(pkg))


def case_appaddon(ck, rel):
    app_addon(ck, rel, 'appaddon', 'up-nano', ['UP-Term:bin/nano', 'UP-Term:share/nano'],
              'UP-Term:bin/nano --version', 'GNU nano')


def case_zipaddon(ck, rel):
    app_addon(ck, rel, 'zipaddon', 'up-zip', ['UP-Term:bin/zip'], 'UP-Term:bin/zip -h', 'Zip 3.0')


TABLE = {'version': case_version, 'updatelist': case_updatelist, 'install': case_install, 'upgrade': case_upgrade, 'migrate': case_migrate,
         'remove': case_remove, 'needsbase': case_needsbase, 'badsum': case_badsum, 'listnew': case_listnew,
         'appaddon': case_appaddon, 'zipaddon': case_zipaddon}


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


def base_stale(installed, kit):
    """True when the rig's base is not the kit's: no UP-Term:VERSIONS (None or empty) or another first line.
    The first line names the release, the vtcon commit and the build date, so any rebuilt kit differs."""
    first = lambda t: (t or '').strip().splitlines()[:1]
    return not first(installed) or first(installed) != first(kit)


BLOCK = 512
# System: on the rig's image is a 106M partition with about 63 MB free (measured 2026-10-22 on a fresh
# copy, rig 4: DH0 217222 blocks, 130146 free); the kit alone is 61 MB. Rushhours: has 1 GB: the base
# goes there, as the image's own old install (a 1.x base in this drawer) already does.
DEST = 'Rushhours:Tools/UP-Term'
INFO_ROW = re.compile(r'^(\S+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)%\s+\d+\s+(\S+)\s+(.+?)\s*$')


def info_free_blocks(out, name):
    """Free 512-byte blocks of the unit (DH1:) or volume (Rushhours:) `name` in AmigaDOS `Info` output, or None.
    A row reads: Unit Size Used Free Full Errs Status Name ("DH0:  106M  87076  130146  40%  0  Read/Write  System")."""
    want = name.rstrip(':').lower()
    for line in out.replace('\r', '').split('\n'):
        m = INFO_ROW.match(line.strip())
        if m and want in (m.group(1).rstrip(':').lower(), m.group(7).lower()):
            return int(m.group(4))
    return None


def tree_bytes(path):
    """Bytes of the files under `path` (a file: its size)."""
    path = pathlib.Path(path)
    if path.is_file():
        return path.stat().st_size
    return sum(f.stat().st_size for f in path.rglob('*') if f.is_file() and not f.is_symlink())


def free_problem(free_blocks, need_bytes, vol):
    """None when the volume has `need_bytes` free, else the message the run stops with."""
    if free_blocks is None:
        return 'no row for %s in the Info output' % vol
    have = free_blocks * BLOCK
    if have < need_bytes:
        return ('%s has %.1f MB free, the run needs %.1f MB: a full volume opens a DOS requester that eats the '
                'keys; free space or move DEST' % (vol, have / 1048576.0, need_bytes / 1048576.0))
    return None


def free_check(vol, need_bytes, what, note=print):
    """Stop the run, with a clear message, when `vol` cannot hold `need_bytes` (Info on the rig)."""
    rc, out = ir.run('Info')
    prob = free_problem(info_free_blocks(out, vol), need_bytes, vol)
    note('free space %s before %s: %s blocks free, need %.1f MB' % (vol, what, info_free_blocks(out, vol), need_bytes / 1048576.0))
    if prob:
        raise SystemExit('[ERROR] free-space precheck before %s: %s' % (what, prob))


def base_processes(status):
    """Process numbers in `Status` output whose program is loaded from a UP-Term drawer's bin (the image's User-Startup runs
    UP-Term:bin/Amarchy, which starts amarchy-gui and amarchy-menu). A running program holds a lock on the bin drawer:
    "bin Not Deleted: object is in use" on the base's Delete (2026-10-29)."""
    found = []
    for l in status.splitlines():
        m = re.match(r'\s*Process\s+(\d+):\s+Loaded as command:\s+(\S+)', l)
        if m and re.search(r'(UP-Term:|/UP-Term/)bin/', m.group(2), re.I):
            found.append(int(m.group(1)))
    return found


def stop_base_programs(rounds=10):
    """Break the programs running from the old base's bin, re-listing Status each round (Amarchy restarts its children),
    until none is left; the Delete of the drawer needs no lock on bin."""
    for _ in range(rounds):
        procs = base_processes(ir.run('Status')[1])
        if not procs:
            return
        for n in procs:
            ir.run('Break %d ALL' % n)
        time.sleep(2)
    raise SystemExit('[ERROR] programs from the old base bin still run after %d rounds: %s' % (rounds, base_processes(ir.run('Status')[1])))


def must(ok, what, seen=''):
    """install_rig's check() shape for a step the run cannot go on without."""
    if not ok:
        raise SystemExit('[ERROR] %s failed:\n%s' % (what, seen[-500:]))


def ensure_base(must, note=print, ipk=BUILD / 'ipk'):
    """The kit's UP-Term base on the rig (installed when absent or stale), upkg's own binary not included. True when this call
    installed a base on a rig that had none: uninstall_base() then takes it away again. `ipk` is the feed whose size the
    free-space check adds twice."""
    ir.put_vsh()
    had_base = ir.run('Assign >NIL: UP-Term: EXISTS')[0] == 0
    kit_versions = ir.KIT / 'Files/VERSIONS'
    if not kit_versions.exists():
        raise SystemExit('upkg_browse_rig: there is no kit at %s (make dist first)' % ir.KIT)
    installed = None
    if had_base:
        rc, installed = ir.run('Type UP-Term:VERSIONS')
        installed = installed if rc == 0 else None
    # an older base on the rig would test the old handler and vtcon, not the kit's
    if base_stale(installed, kit_versions.read_text()):
        # a CLEAN install: opkg does not downgrade, so the image's older base must not be under it, and the
        # delete frees the space; the base goes to DEST (System: is too small for the 61 MB kit)
        ir.prepare(None)
        # the image's assigns lock the old drawers ("Python3 Not Deleted: object is in use"); the install makes new ones
        for a in ('Python3:', 'UP-Term:'):
            ir.run('Assign %s REMOVE' % a)
        stop_base_programs()
        for old in (DEST, 'System:UP-Term'):
            # a drawer this size takes over 60 s on the 68020: past the agent's limit, so run_slow
            rc, o = ir.run_slow('browse-del', 'Delete %s ALL QUIET FORCE' % old, 900)
            if rc >= 20:
                raise SystemExit('[ERROR] the old base %s could not be deleted (rc %d): %s' % (old, rc, o.strip()[-300:]))
        free_check(DEST.split(':')[0], tree_bytes(ir.KIT) + 2 * tree_bytes(ipk), 'the base install', note)
            # upkg's download backend runs curl by name: the kit's userland puts upterm-ports' curl in UP-Term:bin (the CURL option went in 970905e0)
        (ir.VTC / 'runinstallbrowse').write_text('Execute VTC:distkit/Files/install.dos VTC:distkit/Files DEST=' + DEST + ' '
                                                 'NOCONSOLE NODEVICE SSH BEBBOGET WASABI CPU060 UNIFONT EMOJI\n')
        ir.base_install(must, 'runinstallbrowse')
        # prepare() assigned LIBS: as Install sees it, without the rig's AmiSSL drawer: curl needs
        # amisslmaster.library even for the http feed
        for line in ir.rig_libs():
            ir.run(line)
        installed_now = not had_base
        note('the base installed from %s (%s)' % (ir.KIT, 'over an older base' if had_base else 'no base before'))
    return installed_now


def uninstall_base():
    ir.run_long('rununinstall')


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
    with paths.rig_lock():
        rel = Release()
        based = False
        try:
            if todo:
                # the cases run `upkg` from UP-Term:bin and the maintainer scripts under GG:bin/sh: a base of this kit
                # (and the vsh of this tree, put by ensure_base) must be on the rig; install_rig's base case uninstalls its own
                based = ensure_base(must)
            for c, why in todo:
                print('upkg_rig: case %s (%s)' % (c, why))
                try:
                    ok, detail = run_case(c, rel)
                except SystemExit as e:   # the agent or the rig is gone: not a verdict on upkg
                    print('[ABORT] %s: %s (no verdict written)' % (c, e))
                    return 2
                store.record(c, ok, detail, fingerprint())
        finally:
            if based:
                uninstall_base()
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
