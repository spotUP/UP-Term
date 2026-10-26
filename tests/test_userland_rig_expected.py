"""userland_rig stops a package with no expected outputs before it uses the rig.

On 2026-10-26 the port sweep reported bzip2 (0 of 6) and file (0 of 34) as
FAIL after full rig runs: every case said "no expected output (make host-<p>
in the ports repo)". The ports were fine; `make host-<p>` had not run. The
harness now names the missing host run and returns before staging or running
any case, so a harness gap cannot pass for a port failure.
"""
import contextlib, io, pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import userland_rig


class MissingExpected(unittest.TestCase):
    def check(self, with_expected):
        m = userland_rig
        used = []
        saved = (m.PORTS, m.VERDICTS, m.binaries, m.stage, m.tty_stage, m.run_case, m.run, m.fingerprint)
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            (root / "pkgs/p/check").mkdir(parents=True)
            (root / "pkgs/p/check/cases").write_text("one\techo one\ntwo\techo two\n")
            (root / "pkgs/p/check/tty").write_text("keys\tp -i\n")
            (root / "bin").mkdir()
            (root / "bin/p").write_bytes(b"\0")
            if with_expected:
                (root / "build/expected/p").mkdir(parents=True)
                for f in ("one.txt", "two.txt", "keys.stream"):
                    (root / "build/expected/p" / f).write_text("x")
            try:
                m.PORTS, m.VERDICTS = root, root / "verdicts"
                m.binaries = lambda pkg: [root / "bin/p"]
                m.fingerprint = lambda pkg, bins: "f"
                m.stage = lambda *a: used.append("stage")
                m.tty_stage = lambda *a: used.append("tty_stage")
                m.run_case = lambda *a: used.append("run_case") or (0, "")
                m.run = lambda *a, **k: used.append("run") or (0, "")
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    try:
                        ok = m.check_package("p", None)
                    except Exception:   # past the guard, the fakes run out; only the call record matters
                        ok = None
            finally:
                m.PORTS, m.VERDICTS, m.binaries, m.stage, m.tty_stage, m.run_case, m.run, m.fingerprint = saved
        return ok, used, out.getvalue()

    def test_no_expected_outputs_stop_before_the_rig(self):
        ok, used, out = self.check(False)
        self.assertIs(ok, False)
        self.assertEqual(used, [], "the rig was used for a package with no expected outputs")
        self.assertIn("[ERROR] p: no expected output for 3 of 3 cases (one, two, tty:keys): make host-p", out)

    def test_present_expected_outputs_reach_the_rig(self):
        ok, used, out = self.check(True)
        self.assertIn("stage", used)
        self.assertNotIn("no expected output", out)


if __name__ == "__main__":
    unittest.main()
