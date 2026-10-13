#!/usr/bin/env python3
"""USER DATA (kit-split D5), one rule set for every package, on the assembled scripts:

- Install and Update never write inside UP-Term:var, never write a file in ENVARC:amiga-pi, and write in
  ENVARC:Claude only `remote` (when an address is given);
- Uninstall never touches ENVARC:Claude or ENVARC:amiga-pi, and deletes UP-Term:var (or var/<program>)
  only where the user said so (inside the upkeepvar / upvardelete answer).
It replaces test_dist_installer's AmigaPiInstall pair: the rule covers every install.dos and Uninstall."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mkinstaller as mi
import pkgs

IDS = [r['id'] for r in pkgs.rows()]


def install_dos(pid):
    path = ROOT / ('dist/install.dos' if pid == 'base' else 'dist/addons/%s/install.dos' % pid)
    return mi.build(path, pid)


def uninstall(pid):
    path = ROOT / ('dist/Uninstall' if pid == 'base' else 'dist/common/uninstall-addon.dos')
    return mi.build(path, pid)


def code(text):
    return [l.strip() for l in text.splitlines() if l.strip() and not l.strip().startswith(';')]


WRITE = re.compile(r'(?i)^(copy|makedir|delete|protect|rename)\b|^echo\b.*>|^\$\w+\b|^type\b.*>')


class InstallNeverWritesUserData(unittest.TestCase):
    def test_nothing_is_written_into_var_or_the_users_amiga_pi_files(self):
        for pid in IDS:
            for l in code(install_dos(pid)):
                self.assertNotRegex(l, r'(?i)(:|/)var(/|"|\s|$)', '%s: %s' % (pid, l))
                self.assertFalse(re.search(r'ENVARC:amiga-pi/', l), '%s: %s' % (pid, l))

    def test_claudes_drawer_gets_only_the_remote_file(self):
        for pid in IDS:
            for l in code(install_dos(pid)):
                for m in re.finditer(r'ENVARC:Claude/(\S+)', l):
                    self.assertEqual(m.group(1).strip('"'), 'remote', '%s: %s' % (pid, l))
        # the drawers themselves are made, never filled
        self.assertIn('MakeDir ENVARC:Claude', install_dos('claude'))
        self.assertIn('MakeDir ENVARC:amiga-pi', install_dos('pi'))


class UninstallKeepsUserData(unittest.TestCase):
    def test_the_settings_drawers_are_never_touched(self):
        for pid in IDS:
            for l in code(uninstall(pid)):
                self.assertNotRegex(l, r'ENVARC:(Claude|amiga-pi)', '%s: %s' % (pid, l))

    def test_var_is_deleted_only_on_the_users_yes(self):
        for pid in IDS:
            stack = []
            for l in code(uninstall(pid)):
                if re.match(r'(?i)if\b', l):
                    stack.append(l)
                elif re.match(r'(?i)else\b', l) and stack:
                    stack[-1] = 'ELSE ' + stack[-1]
                elif re.match(r'(?i)endif\b', l) and stack:
                    stack.pop()
                elif re.match(r'(?i)delete\b', l) and re.search(r'(var\b|:var|/var)', l) and '$updest/~(var|var.info)' not in l:
                    cond = ' '.join(stack)
                    self.assertRegex(cond, r'upkeepvar', '%s: %s' % (pid, l))
                    if 'ELSE' in stack[-1] or '"$upkeepvar" EQ "YES"' not in stack[-1]:
                        self.assertTrue(stack[-1].startswith('ELSE ') or 'NOT "$upkeepvar" EQ "YES"' in stack[-1], '%s: %s' % (pid, l))

    def test_the_base_keeps_var_when_the_user_said_no(self):
        un = uninstall('base')
        self.assertIn('Delete >NIL: "$updest/~(var|var.info)" ALL QUIET', un)
        self.assertLess(un.index('If "$upkeepvar" EQ "YES"'), un.index('Delete >NIL: "$updest/~(var|var.info)"'))

    def test_an_add_on_that_has_a_program_drawer_asks_about_its_own(self):
        for pid, drawer in (('claude', 'UP-Term:var/Claude'), ('pi', 'UP-Term:var/amiga-pi')):
            self.assertIn('Delete >NIL: "%s" ALL QUIET' % drawer, uninstall(pid))
        for pid in ('nvim', 'python'):
            self.assertNotIn(':var', uninstall(pid))


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
