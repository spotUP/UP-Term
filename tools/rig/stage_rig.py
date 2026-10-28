"""stage_rig.py -- put the freshly built Amiga programs on the rig and prove they are the ones there.

A rig script that starts VTC:Apps/bin/<name> must run the build it was just given, not a binary copied
by hand earlier (a --fresh run once used a stale Amarchy). stage_build() builds the make targets, then
for each program reads the rig's copy back through wasabi, compares its SHA-256 with the host build,
puts the host file when they differ and reads it back again; a copy that still differs raises
StageError (the script stops, it does not run a stale binary).

Imported by amarchy_rig.py; amarchy_menu_rig.py and others may use it the same way.
"""
import hashlib, os, pathlib, subprocess, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths

BUILD = paths.VTCON / 'build/amiga'
# program -> make target that builds it (amarchy-menu needs upterm-ports built, see the Makefile)
TARGETS = {'Amarchy': 'build/amiga/Amarchy', 'amarchy-menu': 'build/amiga/amarchy-menu',
           'amarchy-gui': 'build/amiga/amarchy-gui', 'amarchy-index': 'build/amiga/amarchy-index',
           'wbfake': 'build/amiga/wbfake', 'scrprobe': 'build/amiga/scrprobe',
           'lcmdprobe': 'build/amiga/lcmdprobe', 'AmTile': 'build/amiga/AmTile'}

# staged when a rig script names nothing: amarchy-gui is the menu Amarchy draws, amarchy-menu its text fallback
DEFAULT = ('Amarchy', 'amarchy-menu', 'amarchy-gui', 'amarchy-index')


class StageError(SystemExit):
    pass


def sha(b):
    return hashlib.sha256(b).hexdigest() if b is not None else None


def make(names, run=subprocess.run):
    r = run(['make'] + [TARGETS[n] for n in names], cwd=str(paths.VTCON), capture_output=True, text=True,
            env={**os.environ, 'UPTERM_ROOT': str(paths.UPTERM_ROOT)})
    if r.returncode:
        raise StageError('stage_rig: make failed:\n' + (r.stdout + r.stderr)[-1500:])


def stage_build(names=DEFAULT, build=True, dest=None, read=None, put=None, run=subprocess.run):
    """Returns {name: 'restaged' | 'current'}. read/put default to ami.read_file / ami.put (injectable for tests)."""
    if read is None or put is None:
        import ami
        read, put = read or ami.read_file, put or ami.put
    dest = dest or paths.UPTERM_BIN
    if build:
        make(names, run)
    out = {}
    for n in names:
        local = BUILD / n
        if not local.is_file():
            raise StageError('stage_rig: %s was not built' % local)
        want = local.read_bytes()
        remote = '%s/%s' % (dest, n)
        if sha(read(remote)) == sha(want):
            out[n] = 'current'
            continue
        put(str(local), remote)
        got = read(remote)
        if sha(got) != sha(want):
            raise StageError('stage_rig: %s on the rig is %s bytes sha %s after the put, host build is %d bytes sha %s'
                             % (remote, None if got is None else len(got), (sha(got) or 'missing')[:12], len(want), sha(want)[:12]))
        out[n] = 'restaged'
    return out


def report(res):
    for n, how in sorted(res.items()):
        print('stage_rig: %s %s' % (n, how), flush=True)
