"""The slim base (P9): git, perl and vim are upkg packages, not part of UP-Term.lha.

dist/slim.txt names the paths each package owns (the one list: tools/slim.py). Checked here:
the base kit and UP-Term.lha hold none of them (the fat base v1.0 did: 37 MB of 70), each
package's staged tree and .ipk hold exactly its paths and its SOURCES.txt row, and the rows of
the base's SOURCES.txt name no moved port. The kit of the real build is read (make dist dist-ipk);
skipped when it was not built, required with SLIM_REQUIRE=1 (make dist-check).
"""
import io, os, pathlib, subprocess, sys, tarfile, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pkgs, slim

from test_dist_ipk import split_ar

BUILD = ROOT / 'build'
PORTS = pathlib.Path(os.environ.get('PORTS_KIT', ROOT.parent / 'upterm-ports/build/kit/userland'))
REQUIRE = os.environ.get('SLIM_REQUIRE') == '1'
ARCHIVE = {r['id']: r['archive'] for r in pkgs.rows()}
BASE = BUILD / 'dist/UP-Term/Files/userland'
MOVED = ('bin/git', 'bin/perl', 'bin/vim', 'share/vim', 'share/git-core', 'share/man/man1/vim.1',
         'licenses/git', 'licenses/perl', 'licenses/vim')


def tree(root):
    """every file, symlink and drawer under root, relative, sorted"""
    root = pathlib.Path(root)
    return sorted(str(p.relative_to(root)) for p in root.rglob('*'))


def need(case, path):
    if not path.exists():
        if REQUIRE:
            case.fail('%s missing: make dist dist-ipk' % path)
        case.skipTest('%s not built' % path)


class Table(unittest.TestCase):
    def test_the_packages_are_git_perl_vim_and_are_rows_of_packages_txt(self):
        self.assertEqual(slim.ids(), ['git', 'perl', 'vim'])
        for i in slim.ids():
            self.assertIn(i, ARCHIVE)
            self.assertTrue(ARCHIVE[i].startswith('UP-Term-'), i)      # they need the base: UP-Term:bin

    def test_git_is_selected_by_pattern_so_the_dashed_binaries_may_change(self):
        self.assertIn('bin/git-*', slim.patterns('git'))
        self.assertFalse([p for p in slim.patterns('git') if p.startswith('bin/git-') and '*' not in p])

    def test_uninstall_lines_come_from_the_same_list(self):
        self.assertIn('Delete >NIL: "UP-Term:bin/git-#?" ALL QUIET', slim.uninstall('git'))
        self.assertIn('Delete >NIL: "UP-Term:share/vim" ALL QUIET', slim.uninstall('vim'))


class SlimBase(unittest.TestCase):
    def test_the_base_kit_has_none_of_the_moved_paths(self):
        need(self, BASE)
        have = [m for m in MOVED if (BASE / m).exists() or (BASE / m).is_symlink()]
        self.assertEqual(have, [])
        self.assertEqual([p for p in tree(BASE) if p.startswith('bin/git') or p.startswith('bin/perl') or p.startswith('bin/vim')], [])

    def test_the_base_sources_name_no_moved_port(self):
        need(self, BASE)
        ports = {l.split('\t', 1)[0] for l in (BASE / 'SOURCES.txt').read_text().splitlines()}
        self.assertEqual(ports & set(slim.ids()), set())

    def test_the_base_archive_lists_none_of_them(self):
        need(self, BUILD / 'UP-Term.lha')
        out = subprocess.run(['lha', 'l', str(BUILD / 'UP-Term.lha')], capture_output=True, text=True, check=True).stdout
        for m in ('Files/userland/bin/git', 'Files/userland/bin/perl', 'Files/userland/bin/vim',
                  'Files/userland/share/vim/', 'Files/userland/share/git-core/'):
            self.assertNotIn(' UP-Term/' + m, out, m)


class Packages(unittest.TestCase):
    def expected(self, i):
        """what the package holds: each owned path with its parents and whatever is under it, and the SOURCES.txt row"""
        need(self, PORTS / 'SOURCES.txt')
        out = {'licenses/%s' % i, 'licenses/%s/SOURCES.txt' % i}
        for rel in slim.paths(i, PORTS):
            out |= {str(q) for q in pathlib.Path(rel).parents if str(q) != '.'} | {rel}
            out |= {str(q.relative_to(PORTS)) for q in (PORTS / rel).rglob('*')}
        return sorted(out)

    def test_each_stage_holds_exactly_its_paths_and_its_sources_row(self):
        for i in slim.ids():
            stage = BUILD / 'dist' / ARCHIVE[i] / 'Files/userland'
            need(self, stage)
            self.assertEqual(tree(stage), self.expected(i), i)
            row = (stage / 'licenses' / i / 'SOURCES.txt').read_text().splitlines()
            self.assertEqual([l.split('\t')[0] for l in row], [i])

    def test_each_ipk_holds_the_stage_and_nothing_of_userland_beyond_it(self):
        for i in slim.ids():
            ipk = BUILD / 'ipk' / pkgs.ipk_name({r['id']: r for r in pkgs.rows()}[i])
            need(self, ipk)
            members = dict(split_ar(ipk.read_bytes()))
            names = tarfile.open(fileobj=io.BytesIO(members['data.tar.gz'])).getnames()
            top = './T/upkg-stage/%s/Files/userland/' % ARCHIVE[i]
            got = sorted(n[len(top):] for n in names if n.startswith(top))
            self.assertEqual(got, self.expected(i), i)


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
