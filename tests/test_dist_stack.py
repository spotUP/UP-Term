#!/usr/bin/env python3
"""Every Amiga program UP-Term builds into the kit sets its own stack, and every
launcher that starts a loaded program gives it the stack its $STACK: cookie asks
for (RULES.md rule 6).

upgetty crashed on the owner's A1200 (#80000006): it started vsh, whose cookie
asks for 262144, from its seglist with a fixed NP_StackSize of 16000, and it set
no stack of its own. The program list comes from the Makefile's kit recipe (each
$(BUILD)/amiga/<name> copied into $(KIT)/Files), the source from that target's
rule, so a new program cannot slip into the kit without a stack."""
import os, re, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Mounted by the system (mountlist / DosNode stack), never started as a command.
MOUNTED = {'vtcon-handler', 'pty-handler', 'ixpipe-handler', 'up-console.device'}

# Kit programs not yet audited for their stack (no __stack, no cookie). The
# audit sizes each from a measured run (wasabi ps headroom) and removes it here;
# the set may only shrink.
AUDIT_PENDING = {
    'Amarchy', 'amarchy-index', 'hl', 'mdv', 'ps', 'top', 'Reboot', 'rz', 'sz',
    'UP-Term Prefs', 'UPConsole', 'UPDemo', 'upicon', 'upsame', 'uptelnet',
    'UPTerm', 'upupdate', 'wasabikey',
}

# "$STACK: 262144", or "$STACK: " STR(MENU_STACK) (amarchy/menu.c, amarchy/gui.c).
COOKIE = re.compile(r'"\$STACK: *([1-9][0-9]*"|"\s*[A-Za-z_]+\()')


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
        for name in ('upgetty', 'vsh', 'amarchy-menu', 'UP-Term Prefs'):
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
