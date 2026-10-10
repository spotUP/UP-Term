#!/usr/bin/env python3
"""The kit's file modes (tools/kitmodes.py), on the host: lha carries them to
the Amiga as protection bits, and a file without w and d there cannot be
replaced by the next Install or an Update (less's 0444 LICENSE and COPYING,
2026-10-09)."""
import os, pathlib, shutil, stat, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import kitmodes as km

HUNK = b'\x00\x00\x03\xf3' + b'\0' * 28


def mode(p):
    return stat.S_IMODE(os.lstat(p).st_mode)


class KitModes(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        files = {"Files/userland/licenses/less/LICENSE": (b"less licence\n", 0o444),
                 "Files/coreutils/COPYING": (b"GNU GPL\n", 0o755),
                 "Files/coreutils/bin/ls": (HUNK, 0o755),
                 "Files/vsh": (HUNK, 0o644),
                 "Files/userland/bin/zcat": (b"#!/UP-Term/bin/sh\ngzip -dc\n", 0o755),
                 "Files/README.txt": (b"text\n", 0o600)}
        for name, (data, m) in files.items():
            p = self.d / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
            os.chmod(p, m)

    def tearDown(self):
        for dirpath, _, fs in os.walk(self.d):
            for f in fs:
                os.chmod(os.path.join(dirpath, f), 0o644)
        shutil.rmtree(self.d)

    def test_a_read_only_licence_becomes_writable_so_an_update_can_replace_it(self):
        km.normalise(self.d)
        self.assertEqual(mode(self.d / "Files/userland/licenses/less/LICENSE"), 0o644)
        self.assertEqual(km.unwritable(self.d), [])

    def test_a_text_file_loses_x_a_program_keeps_or_gets_it(self):
        km.normalise(self.d)
        self.assertEqual(mode(self.d / "Files/coreutils/COPYING"), 0o644)
        self.assertEqual(mode(self.d / "Files/coreutils/bin/ls"), 0o755)
        self.assertEqual(mode(self.d / "Files/vsh"), 0o755)
        self.assertEqual(mode(self.d / "Files/userland/bin/zcat"), 0o755)
        self.assertEqual(mode(self.d / "Files/README.txt"), 0o644)

    def test_the_tool_exits_0_and_names_what_it_changed(self):
        r = subprocess.run([sys.executable, str(ROOT / "tools/kitmodes.py"), str(self.d)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("licenses/less/LICENSE 444 -> 644", r.stdout)

    def test_a_staged_file_without_u_w_is_found(self):
        os.chmod(self.d / "Files/README.txt", 0o444)
        self.assertEqual(sorted(km.unwritable(self.d)), sorted([str(self.d / "Files/README.txt"),
                         str(self.d / "Files/userland/licenses/less/LICENSE")]))

    def test_the_built_kit_has_no_file_without_u_w(self):
        kit = ROOT / "build/dist/UP-Term"
        if not kit.exists():
            self.skipTest("no build/dist/UP-Term (make dist)")
        self.assertEqual(km.unwritable(kit), [])


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
