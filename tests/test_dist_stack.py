#!/usr/bin/env python3
"""Every Amiga program UP-Term builds into the kit sets its own stack, and every
launcher that starts a loaded program gives it the stack its $STACK: cookie asks
for (RULES.md rule 6).

upgetty crashed on the owner's A1200 (#80000006): it started vsh, whose cookie
asks for 262144, from its seglist with a fixed NP_StackSize of 16000, and it set
no stack of its own. The program list comes from the Makefile's kit recipe (each
$(BUILD)/amiga/<name> copied into $(KIT)/Files), the source from that target's
rule, so a new program cannot slip into the kit without a stack."""
import os, re, shutil, sys, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Mounted by the system (mountlist / DosNode stack), never started as a command.
MOUNTED = {'vtcon-handler', 'pty-handler', 'ixpipe-handler', 'up-console.device'}

# Kit programs not yet audited for their stack (no __stack, no cookie). The
# audit sizes each from a measured run (wasabi ps headroom) and removes it here;
# the set may only shrink.
AUDIT_PENDING = set()

# DIST-STACK-ALL: every file in a bin drawer (Files/<drawer>/bin) of the staged dist
# (KIT=build/dist/<archive>) must carry a $STACK cookie. Only AmigaOS executables (hunk header
# 000003F3) count: scripts, icons and the git-* exec stubs have no stack to size. dist-check
# runs this for every archive: the base kit must ship both DIST_BINS drawers, an add-on is
# judged by the bin drawers it has (up-nvim, up-python) and may have none (up-claude).
# DIST_EXEMPT is file -> reason for the base kit programs that cannot carry a cookie.
DIST_BASE = 'up-term'
DIST_BINS = ('Files/coreutils/bin', 'Files/userland/bin')
DIST_EXEMPT = {
    'hostname': 'GNU 5.2.1 (dist/gg): coreutils 9.12 does not build it; no cookie, small program, no big buffers',
    'pinky': 'GNU 5.2.1 (dist/gg): coreutils 9.12 does not build it',
    'who': 'GNU 5.2.1 (dist/gg): coreutils 9.12 does not build it',
}
DIST_COOKIE = re.compile(rb'\$STACK: *[1-9]')

# "$STACK: 262144", or "$STACK: " STR(MENU_STACK) (amarchy/menu.c, amarchy/gui.c).
COOKIE = re.compile(r'"\$STACK: *([1-9][0-9]*"|"\s*[A-Za-z_]+\()')


# Synthetic kit files for the cases below: an AmigaOS executable with no cookie, and a base
# kit that is fine (one program with a cookie, the exempt ones without, both drawers).
HUNK = b'\x00\x00\x03\xf3' + b'\x00' * 32
BASE_OK = {'Files/coreutils/bin/ls': HUNK + b'$STACK: 65536\x00',
           'Files/userland/bin/sh.info': b'icon'}
BASE_OK.update(('Files/coreutils/bin/' + f, HUNK) for f in DIST_EXEMPT)


def dist_bin_drawers(kit):
    """The bin drawers of a staged kit, kit-relative: every Files/<drawer>/bin."""
    files = os.path.join(kit, 'Files')
    if not os.path.isdir(files):
        return []
    return sorted('Files/%s/bin' % d for d in os.listdir(files)
                  if os.path.isdir(os.path.join(files, d, 'bin')))


def dist_stack_problems(kit, is_base):
    """What is wrong with the stack cookies of a staged kit, one line each; [] when fine."""
    drawers = dist_bin_drawers(kit)
    problems = ['%s: drawer missing from the base kit' % d
                for d in (DIST_BINS if is_base else ()) if d not in drawers]
    missing = set()
    for d in drawers:
        for f in os.listdir(os.path.join(kit, d)):
            path = os.path.join(kit, d, f)
            if not os.path.isfile(path):
                continue
            with open(path, 'rb') as fh:
                data = fh.read()
            if data[:4] == b'\x00\x00\x03\xf3' and not DIST_COOKIE.search(data):
                missing.add(f)
    exempt = set(DIST_EXEMPT) if is_base else set()
    problems += ['%s: no "$STACK: n" cookie (PIPE-FREEZE: vsh gives 16000 bytes)' % f
                 for f in sorted(missing - exempt)]
    problems += ['%s: exempt, but it now has a cookie or is gone: remove from DIST_EXEMPT' % f
                 for f in sorted(exempt - missing)]
    return problems


def makefile():
    """The Makefile with its backslash continuations joined."""
    with open(os.path.join(ROOT, 'Makefile')) as f:
        return re.sub(r'\\\n', ' ', f.read())


def expand(mk, words, depth=0):
    """Words with each $(VAR) replaced by its := / = definition."""
    out = []
    for w in words:
        m = re.fullmatch(r'\$\(([A-Z0-9_]+)\)', w)
        if m and depth < 5:
            d = re.search(r'^%s\s*:?=([^\n]*)' % m.group(1), mk, re.M)
            out += expand(mk, d.group(1).split(), depth + 1) if d else []
        else:
            out.append(w)
    return out


def kit_programs(mk):
    """{kit name: build target name} for each $(BUILD)/amiga/<name> the kit recipe copies."""
    out = {}
    for line in mk.splitlines():
        if not line.startswith('\tcp ') or '$(KIT)/Files' not in line:
            continue
        srcs = re.findall(r'\$\(BUILD\)/amiga/([A-Za-z0-9_.-]+)', line)
        dest = re.search(r'"?\$\(KIT\)/Files/([^"]*)"?\s*$', line).group(1)
        if len(srcs) == 1 and dest and not dest.endswith('/'):
            out[os.path.basename(dest)] = srcs[0]
        else:
            for s in srcs:
                out[s] = s
    return out


def target_sources(mk, target):
    """The .c files the target's rule depends on, its variables expanded."""
    m = re.search(r'^\$\(BUILD\)/amiga/%s:([^\n]*)' % re.escape(target), mk, re.M)
    if not m:
        return []
    return [w for w in expand(mk, m.group(1).split()) if w.endswith('.c')]


def sources():
    for d, subs, files in os.walk(ROOT):
        rel = os.path.relpath(d, ROOT)
        if rel.split(os.sep)[0] in ('build', 'vendor', 'tests', '.git', 'thoughts', '.claude'):
            subs[:] = []
            continue
        for f in files:
            if f.endswith('.c'):
                yield os.path.join(d, f)


def launches(text):
    """Each CreateNewProcTags(...) call's argument text."""
    for m in re.finditer(r'CreateNewProcTags\s*\(', text):
        i, depth = m.end(), 1
        while depth and i < len(text):
            depth += {'(': 1, ')': -1}.get(text[i], 0)
            i += 1
        yield text[m.start():i]


class Stack(unittest.TestCase):
    def test_the_kit_list_is_read_from_the_makefile(self):
        progs = kit_programs(makefile())
        for name in ('upgetty', 'vsh', 'amarchy-menu', 'upprefs'):
            self.assertIn(name, progs)

    def test_every_kit_program_sets_its_own_stack(self):
        mk = makefile()
        missing = set()
        for name, target in kit_programs(mk).items():
            if name in MOUNTED:
                continue
            srcs = target_sources(mk, target)
            self.assertTrue(srcs, 'no .c in the rule for %s' % target)
            text = ''
            for src in srcs:
                with open(os.path.join(ROOT, src), encoding='latin-1') as f:
                    text += f.read()
            if not COOKIE.search(text):
                missing.add(name)
        self.assertEqual(missing - AUDIT_PENDING, set(),
                         'kit programs with no "$STACK: n" cookie (RULES.md rule 6)')
        self.assertEqual(AUDIT_PENDING - missing, set(),
                         'audited since: remove from AUDIT_PENDING')

    def test_every_program_in_the_dist_bin_drawers_has_a_stack_cookie(self):
        kit = os.environ.get('KIT')
        if not kit:
            self.skipTest('KIT=<staged dist/<archive>> not set (make dist-check sets it)')
        is_base = os.path.basename(os.path.normpath(kit)) == DIST_BASE
        self.assertEqual(dist_stack_problems(kit, is_base), [])

    def _kit(self, files):
        """A staged kit in a temporary drawer: {kit-relative path: bytes}."""
        kit = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, kit)
        for rel, data in files.items():
            path = os.path.join(kit, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as fh:
                fh.write(data)
        return kit

    def test_an_addon_kit_with_no_bin_drawer_passes(self):
        # dist-check stopped at up-claude: the base kit's drawers were demanded of every archive
        kit = self._kit({'Files/Claude': HUNK, 'Files/install.dos': b'x'})
        self.assertEqual(dist_stack_problems(kit, False), [])

    def test_an_addon_kit_program_with_no_cookie_is_reported(self):
        kit = self._kit({'Files/nvim/bin/nvim': HUNK,
                         'Files/nvim/bin/readme': b'text',
                         'Files/python3/bin/python3': HUNK + b'$STACK: 1048576\x00'})
        self.assertEqual(dist_stack_problems(kit, False),
                         ['nvim: no "$STACK: n" cookie (PIPE-FREEZE: vsh gives 16000 bytes)'])

    def test_the_base_kit_must_ship_both_bin_drawers(self):
        self.assertEqual(dist_stack_problems(self._kit(BASE_OK), True), [])
        lacks = {k: v for k, v in BASE_OK.items() if not k.startswith('Files/userland/')}
        self.assertEqual(dist_stack_problems(self._kit(lacks), True),
                         ['Files/userland/bin: drawer missing from the base kit'])

    def test_the_base_name_is_the_package_table_base(self):
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import pkgs
        self.assertEqual(pkgs.get('base', 'archive'), DIST_BASE)

    def test_a_launcher_never_gives_a_loaded_program_a_fixed_stack(self):
        bad = []
        for p in sources():
            with open(p, encoding='latin-1') as f:
                text = f.read()
            for call in launches(text):
                if 'NP_Seglist' in call and re.search(r'NP_StackSize\s*,\s*[0-9]', call):
                    bad.append(os.path.relpath(p, ROOT))
        self.assertEqual(bad, [], 'NP_Seglist launch with a literal NP_StackSize: use seg_stack (config/segstack.h)')

    def test_upgetty_gives_its_shell_the_cookie_stack(self):
        with open(os.path.join(ROOT, 'device', 'upgetty.c'), encoding='latin-1') as f:
            text = f.read()
        self.assertTrue('seg_stack(seg)' in text, 'start_shell must size the shell from its cookie')
        self.assertTrue(re.search(r'long __stack = [1-9][0-9]*;', text), 'upgetty must set its own stack')


if __name__ == '__main__':
    unittest.main()
