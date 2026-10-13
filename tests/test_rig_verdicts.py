#!/usr/bin/env python3
"""tools/rig/verdicts.py and paths.rig_lock on the host (no emulator): the resumable verdicts of the rig
scripts (RULES section 6a: record each case, skip what is recorded, --only / --failed / --force) and the
per-rig lock (a second script on the same rig stops with the holder's pid)."""
import os, pathlib, subprocess, sys, tempfile, time, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import paths, verdicts


class Fingerprint(unittest.TestCase):
    def test_it_follows_the_bytes_and_a_files_content(self):
        d = pathlib.Path(tempfile.mkdtemp())
        f = d / "MANIFEST"
        f.write_bytes(b"one")
        a = verdicts.fingerprint(b"script", f)
        self.assertEqual(a, verdicts.fingerprint(b"script", f))
        f.write_bytes(b"two")
        self.assertNotEqual(a, verdicts.fingerprint(b"script", f), "a changed file must change it")
        self.assertNotEqual(verdicts.fingerprint(b"other", f), verdicts.fingerprint(b"script", f))

    def test_a_missing_file_is_not_the_same_as_an_empty_one(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "empty").write_bytes(b"")
        self.assertNotEqual(verdicts.fingerprint(d / "empty"), verdicts.fingerprint(d / "absent"))


class Selection(unittest.TestCase):
    CASES = ["base", "nvim", "all"]

    def setUp(self):
        self.store = verdicts.Store(pathlib.Path(tempfile.mkdtemp()) / "install")   # the drawer is made by the first record
        self.fp = "fp1"

    def pick(self, **kw):
        run, skip = verdicts.select(self.store, self.CASES, lambda c: self.fp, **kw)
        return [c for c, _ in run], [c for c, _ in skip]

    def test_nothing_recorded_runs_every_case(self):
        self.assertEqual(self.pick(), (self.CASES, []))

    def test_a_case_recorded_as_passed_with_the_same_kit_is_skipped(self):
        self.store.record("base", True, "all", "fp1")
        self.assertEqual(self.pick(), (["nvim", "all"], ["base"]))

    def test_a_pass_of_another_kit_runs_again(self):
        self.store.record("base", True, "all", "old")
        self.assertEqual(self.pick()[0], self.CASES)

    def test_a_failure_runs_again_without_flags(self):
        self.store.record("nvim", False, "x", "fp1")
        self.assertEqual(self.pick()[0], self.CASES)

    def test_failed_picks_only_the_failures(self):
        self.store.record("base", True, "ok", "fp1")
        self.store.record("nvim", False, "x", "fp1")
        self.assertEqual(self.pick(failed=True), (["nvim"], ["base", "all"]))
        self.assertEqual(self.store.failed(self.CASES), ["nvim"])

    def test_only_runs_the_named_case_even_when_it_passed(self):
        self.store.record("base", True, "ok", "fp1")
        self.assertEqual(self.pick(only=["base"]), (["base"], []))

    def test_only_with_failed_keeps_a_passed_case_out(self):
        self.store.record("base", True, "ok", "fp1")
        self.assertEqual(self.pick(only=["base"], failed=True)[0], [])

    def test_force_runs_everything_from_zero(self):
        for c in self.CASES:
            self.store.record(c, True, "ok", "fp1")
        self.assertEqual(self.pick(force=True)[0], self.CASES)

    def test_an_unknown_case_is_refused_with_the_list(self):
        with self.assertRaises(SystemExit) as e:
            self.pick(only=["nope"])
        self.assertIn("base nvim all", str(e.exception))

    def test_record_keeps_the_verdict_the_fingerprint_and_the_detail(self):
        self.store.record("all", False, "a" * 600, "fpX")
        v = self.store.verdict("all")
        self.assertEqual((v["verdict"], v["fp"], v["detail"]), ("fail", "fpX", "a" * 600))
        self.assertIsNone(self.store.verdict("base"))


class Counter(unittest.TestCase):
    def test_checks_count_and_keep_the_reasons(self):
        ck = verdicts.Checks()
        ck(True, "fine")
        ck(False, "broken", " seen ")
        self.assertEqual((ck.total, ck.passed, ck.failed), (2, 1, ["broken: seen"]))


class Lock(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp()) / "rig2"
        os.environ.pop("UPTERM_RIG_LOCK_HELD", None)

    def test_a_second_script_on_the_same_rig_stops_with_the_holders_pid(self):
        with paths.rig_lock(self.d):
            held = os.environ.pop("UPTERM_RIG_LOCK_HELD")   # as a separate process sees it
            try:
                with self.assertRaises(SystemExit) as e:
                    with paths.rig_lock(self.d):
                        self.fail("the second script got the rig")
            finally:
                os.environ["UPTERM_RIG_LOCK_HELD"] = held
            self.assertIn("pid %d" % os.getpid(), str(e.exception))

    def test_the_lock_of_another_process_is_seen(self):
        code = ("import sys,time; sys.path.insert(0,%r); import paths, pathlib\n"
                "with paths.rig_lock(pathlib.Path(%r)):\n print('held',flush=True); time.sleep(30)\n") % (str(ROOT / "tools/rig"), str(self.d))
        p = subprocess.Popen([sys.executable, "-I", "-c", code], stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(p.stdout.readline().strip(), "held")
            with self.assertRaises(SystemExit) as e:
                with paths.rig_lock(self.d):
                    self.fail("the second script got the rig")
            self.assertIn("pid %d" % p.pid, str(e.exception))
        finally:
            p.kill()
            p.wait()
        with paths.rig_lock(self.d):   # a dead holder frees it (the kernel drops the flock)
            pass

    def test_the_lock_is_free_again_after_the_block(self):
        with paths.rig_lock(self.d):
            pass
        with paths.rig_lock(self.d):
            pass
        self.assertNotIn("UPTERM_RIG_LOCK_HELD", os.environ)

    def test_a_script_started_by_the_holder_does_not_wait_for_it(self):
        # assign_rig runs install_rig as a child: the child inherits the environment and must pass
        with paths.rig_lock(self.d):
            r = subprocess.run([sys.executable, "-I", "-c",
                                "import sys,pathlib; sys.path.insert(0,%r); import paths\n"
                                "with paths.rig_lock(pathlib.Path(%r)): print('ran')" % (str(ROOT / "tools/rig"), str(self.d))],
                               capture_output=True, text=True, env=dict(os.environ))
            self.assertEqual(r.stdout.strip(), "ran", r.stderr)


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
