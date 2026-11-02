#!/usr/bin/env python3
"""test_rig_bootlog_truncate.py -- BOOTX:boot.log is truncated once per boot, in the rig STARTUP before go runs, and
every other writer appends. go runs beside the Startup-Sequence, so a truncating go wiped the early shadows lines
that S:User-Startup wrote first. Host-only: python3 tests/test_rig_bootlog_truncate.py"""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/rig'))
import rig  # noqa: E402

TRUNC = re.compile(r'Echo >BOOTX:boot\.log')


class BootLogTruncate(unittest.TestCase):
    def test_startup_truncates_before_go(self):
        lines = rig.STARTUP.splitlines()
        trunc = [i for i, l in enumerate(lines) if TRUNC.search(l)]
        go = [i for i, l in enumerate(lines) if 'BOOTX:go' in l]
        self.assertEqual(len(trunc), 1)
        self.assertLess(trunc[0], go[0])

    def test_truncate_uses_the_shell_builtin_echo(self):
        """Echo is a Shell built-in on OS 3.x: no C:Echo file exists, so DH0:C/Echo failed, boot.log was never
        truncated and "boot started" never appeared (rig 58/59, 2026-11-02)."""
        line = next(l for l in rig.STARTUP.splitlines() if TRUNC.search(l))
        self.assertTrue(line.startswith('Echo '), line)

    def test_no_other_writer_truncates(self):
        for name in ('GO', 'SHADOWS_UP_SCRIPT'):
            self.assertIsNone(TRUNC.search(getattr(rig, name)), name)


if __name__ == '__main__':
    unittest.main()
