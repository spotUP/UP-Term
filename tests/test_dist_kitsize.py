#!/usr/bin/env python3
"""K7: the Install scripts check the room on the destination disk against the SIZE OF THE KIT, not against numbers
typed in once. On a 106 MB System: partition an Install with every option died with "Error in writing - disk is
full": the base's check compared the free space with a guess of 17 MB for a kit of 26 MB.

- the scripts hold no size typed by hand (every KB comes from tools/kitsize.py, counted at `make dist` time);
- the base asks for another drawer when the disk of the first is too small (a loop around askdir), the add-ons,
  which install into a fixed place, abort with the figures;
- tools/kitsize.py's arithmetic on a made-up kit, and its option files are files install.dos copies;
- the kit `make dist` built (build/dist/<archive>/Install): the numbers in it are not under what the archive
  holds (the MANIFEST: size and part of every file) and not far over it. Skipped when no kit is staged, unless
  KITSIZE_REQUIRE=1 (make dist-check sets it)."""
import math, os, pathlib, re, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import kitsize, mkinstaller as mi, pkgs

ROWS = {r['id']: r for r in pkgs.rows()}
SCRIPTS = {i: ROOT / ('dist/Install.installer' if i == 'base' else 'dist/addons/%s/Install.installer' % i) for i in ROWS}


def raw(i):
    return SCRIPTS[i].read_text(encoding='latin-1')


class NoSizeTypedByHand(unittest.TestCase):
    def test_no_script_holds_a_size_typed_by_hand(self):
        """`(set #kb-system 2200)` and `(set #totalkb (+ 900 20))` aged until the check passed an install that did
        not fit; a size in a script must be an @KB...@ value."""
        for i in ROWS:
            s = raw(i)
            for m in re.finditer(r'\(set #(kb-[a-z]+|totalkb) \(?\+? ?[1-9]\d\d+', s):
                self.fail('%s: %s' % (i, m.group(0)))
            for m in re.finditer(r'\(set #step "[a-z-]+" #kb [1-9]\d\d+', s):
                if '"unix"' not in m.group(0) and '"record"' not in m.group(0):
                    self.fail('%s: %s' % (i, m.group(0)))

    def test_the_assembled_scripts_hold_the_kit_sizes(self):
        for i in ROWS:
            self.assertNotRegex(mi.build(SCRIPTS[i], i), r'@KB[A-Z]*@', i)


class ABaseTooSmallForTheChoicesAsksForAnotherDrawer(unittest.TestCase):
    def setUp(self):
        self.s = mi.build(SCRIPTS['base'], 'base')

    def test_the_drawer_is_asked_again_while_the_disk_is_too_small(self):
        s = self.s
        self.assertIn('(procedure P_ASKDEST', s)
        loop = s[s.index('(while (not #roomok)'):]
        loop = loop[:loop.index('; what goes on SYS:')]
        self.assertIn('(getdiskspace (pathonly #dest))', loop)
        self.assertIn('(P_ASKDEST)', loop)
        self.assertIn('(message', loop)
        self.assertNotIn('(abort', loop)        # nothing was copied: the user may choose, Abort Install is a button
        self.assertIn('" KB free;', loop)       # how much there is ...
        self.assertIn('#totalkb', loop)         # ... and how much is needed

    def test_the_check_comes_after_the_choices_and_before_any_copying(self):
        s = self.s
        self.assertLess(s.index('(set #wasabi ""'), s.index('(while (not #roomok)'))
        self.assertLess(s.index('(while (not #roomok)'), s.index('(P_OPTIONS)'))
        self.assertLess(s.index('(while (not #roomok)'), s.index('(P_STEP)'))

    def test_every_add_on_aborts_with_both_figures(self):
        for i in ROWS:
            if i == 'base':
                continue
            s = mi.build(SCRIPTS[i], i)
            self.assertRegex(s, r'\(set #totalkb \d+\)', i)
            m = re.search(r'\(abort "Nothing was installed\. " #dest " has " \(/ #free 1024\) " KB free;\\n"\s*'
                          r'"[^"]*" #totalkb " KB', s)
            self.assertTrue(m, i)


class KitsizeArithmetic(unittest.TestCase):
    def kit(self, files):
        d = pathlib.Path(tempfile.mkdtemp())
        for rel, size in files.items():
            p = d / 'Files' / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b'x' * size)
        return d

    def test_a_file_takes_its_blocks_a_header_and_extension_blocks(self):
        self.assertEqual(kitsize.file_bytes(0), 512)
        self.assertEqual(kitsize.file_bytes(488), 2 * 512)
        self.assertEqual(kitsize.file_bytes(489), 3 * 512)
        # 72 data blocks fit the header; the 73rd needs an extension block
        self.assertEqual(kitsize.file_bytes(72 * 488), (1 + 72) * 512)
        self.assertEqual(kitsize.file_bytes(72 * 488 + 1), (1 + 73 + 1) * 512)

    def test_a_part_is_the_sum_of_its_files(self):
        kit = self.kit({'unifont/00': 1000000, 'unifont/01': 500000, 'emoji/1F0': 100000, 'XCON': 300})
        v = kitsize.sizes(kit, 'base')
        want = lambda *sz: -(-(sum(kitsize.file_bytes(n) for n in sz) + 512) // 1024)      # + the drawer
        self.assertEqual(v['KBCOPYUNIFONT'], want(1000000, 500000))
        self.assertEqual(v['KBCOPYEMOJI'], want(100000))

    def test_a_replaced_library_counts_twice_for_its_kept_copy(self):
        kit = self.kit({'libs/ixemul.library': 200000, 'libs/ixnet.library': 20000})
        one = -(-(kitsize.file_bytes(200000) + kitsize.file_bytes(20000) + 512) // 1024)
        self.assertEqual(kitsize.sizes(kit, 'base')['KBLIBS'], 2 * one)

    def test_only_the_files_of_a_chosen_option_count_for_it(self):
        kit = self.kit({'net/curl-8.0/curl.020': 900000, 'net/curl-8.0/curl.040': 950000, 'net/curl-8.0/curl.060': 1000000,
                        'net/curl-8.0/COPYING': 35000, 'net/bebbossh-1.0/bebbossh': 40000,
                        'net/bebbossh-1.0/bebbossh-1.0-src.tar.bz2': 250000, 'UPTerm': 10})
        v = kitsize.sizes(kit, 'base')
        self.assertEqual(v['KBCURL'], -(-kitsize.file_bytes(1000000) // 1024))        # the largest build only
        self.assertEqual(v['KBSSH'], -(-kitsize.file_bytes(40000) // 1024))           # the program, not the source
        self.assertLess(v['KBTOOLS'], 100)                                            # nothing of net/ in the tools part

    def test_every_option_file_is_a_file_install_dos_copies(self):
        dos = (ROOT / 'dist/install.dos').read_text(encoding='latin-1')
        copies = ' '.join(l for l in dos.splitlines() if l.lstrip().startswith('Copy '))
        for name, groups in kitsize.OPTIONS.items():
            if name == 'KBWASABI':
                continue
            for _, pats in groups:
                for pat in pats:
                    rx = re.escape(pat.split('/')[-1]).replace(r'\?', '.')
                    self.assertRegex(copies, r'(?<![\w.])' + rx + r'(?![\w.])', '%s: %s' % (name, pat))


def manifest(kit):
    """{part: bytes on disk, lower bound} from the kit's MANIFEST (the size and part of every file)."""
    out = {}
    for line in (kit / 'Files/MANIFEST').read_text(encoding='latin-1').splitlines():
        f = line.split(None, 3)
        if len(f) == 4 and '#' not in f[3] and f[0] != '-':
            out[f[0]] = out.get(f[0], 0) + kitsize.file_bytes(int(f[1]))
    return out


class TheBuiltKitsNumbersMatchTheirArchives(unittest.TestCase):
    def kit(self, i):
        d = ROOT / 'build/dist' / ROWS[i]['archive']
        if not (d / 'Install').exists():
            if os.environ.get('KITSIZE_REQUIRE'):
                self.fail('%s: not built: make dist' % d)
            self.skipTest('%s is not built (make dist)' % d)
        return d

    def number(self, text, name):
        m = re.search(r'\(set #%s \(?(?:if #\w+ )?(\d+)' % re.escape(name), text)
        self.assertTrue(m, name)
        return int(m.group(1))

    def within(self, embedded, real_bytes, what):
        lower = real_bytes / 1024
        self.assertGreaterEqual(embedded, lower, '%s: %d KB in the script, the archive holds %d KB' % (what, embedded, lower))
        self.assertLessEqual(embedded, lower * 1.1 + 60, '%s: %d KB for %d KB' % (what, embedded, lower))

    def test_the_base_part_sizes_are_what_its_archive_holds(self):
        kit = self.kit('base')
        text, mf = (kit / 'Install').read_text(encoding='latin-1'), manifest(kit)
        for name, part in (('kb-system', 'system'), ('kb-coreutils', 'copy-coreutils'), ('kb-userland', 'copy-userland'),
                           ('kb-manpages', 'userland'), ('kb-unifont', 'copy-unifont'), ('kb-emoji', 'copy-emoji')):
            self.within(self.number(text, name), mf[part], name)

    def test_the_total_of_an_add_on_is_what_its_archive_installs(self):
        for i in ROWS:
            if i == 'base':
                continue
            kit = self.kit(i)
            text, mf = (kit / 'Install').read_text(encoding='latin-1'), manifest(kit)
            installed = sum(b for p, b in mf.items() if p != 'handed')
            self.within(self.number(text, 'totalkb'), installed, i)


if __name__ == '__main__':
    unittest.main()
