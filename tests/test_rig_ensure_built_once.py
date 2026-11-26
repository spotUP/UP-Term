#!/usr/bin/env python3
"""RIG-START-HOST-SLOW: one rig start called ensure_built 4 times, each a make -n parse (8-43 s). A name made current once
in the process is not checked again."""
import pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class Once(unittest.TestCase):
    def test_second_call_runs_no_make(self):
        t = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, t, ignore_errors=True)
        (t / "build/amiga").mkdir(parents=True)
        f = t / "build/amiga/foo"
        f.write_bytes(b"x")
        calls = []
        def run(cmd, **kw):
            calls.append(cmd)
            return subprocess.CompletedProcess(cmd, 0, "", "")
        rig._BUILT_CURRENT.clear()
        self.addCleanup(rig._BUILT_CURRENT.clear)
        with mock.patch.object(rig, "ROOT", t):
            rig.ensure_built([f], run)
            rig.ensure_built([f], run)
        self.assertEqual(len(calls), 1)


if __name__ == "__main__":
    unittest.main()
