"""`--only a,b` in the amarchy rig scripts selects those rows (it ran 0 rows, "passed 0 of 0", before), and an unknown
row name exits non-zero naming it. One shared helper: tools/rig/verdicts.py parse_only. The test drives each script's
main() with stage_rig stubbed and the rows replaced by recorders, so no emulator is needed."""
import importlib, pathlib, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

SCRIPTS = ["amarchy_menu_rig", "amarchy_bar_rig", "amarchy_clash_rig", "amarchy_rig", "amarchy_window_rig",
           "amarchy_prefs_rig", "amarchy_p2_rig", "amarchy_demo_rig"]


def run_main(mod, only):
    ran = []
    rows = [(n, (lambda n=n: (ran.append(n), (True, "x"))[1])) for n in ("r_a", "r_b", "r_c")]
    attr = "STEPS" if hasattr(mod, "STEPS") else "ROWS"
    with tempfile.TemporaryDirectory() as d, \
            mock.patch.object(mod, attr, rows), mock.patch.object(mod, "OUT", pathlib.Path(d)), \
            mock.patch.object(mod.stage_rig, "report"), mock.patch.object(mod.stage_rig, "stage_build"), \
            mock.patch.object(sys, "argv", ["x", "--only", only, "--no-build"]):
        if hasattr(mod, "tabstate"):
            mock.patch.object(mod.tabstate, "open_host", lambda *a, **k: None).start()
        if hasattr(mod, "wb_front"):
            mock.patch.object(mod, "wb_front", lambda: None).start()
        if hasattr(mod, "ICON_ROWS"):
            mock.patch.object(mod, "ICON_ROWS", set()).start()
        if hasattr(mod, "stale_guard"):
            mock.patch.object(mod, "stale_guard", lambda f: None).start()
        if hasattr(mod, "g"):
            mock.patch.object(mod.g, "process_running", lambda n: True).start()
        try:
            mod.main()
        finally:
            mock.patch.stopall()
    return ran


class OnlyList(unittest.TestCase):
    def test_comma_list_selects_rows_and_unknown_exits(self):
        for name in SCRIPTS:
            with self.subTest(script=name):
                mod = importlib.import_module(name)
                self.assertEqual(run_main(mod, "r_a,r_c"), ["r_a", "r_c"])
                self.assertEqual(run_main(mod, "r_b"), ["r_b"])
                with self.assertRaises(SystemExit) as e:
                    run_main(mod, "r_a,nosuch")
                self.assertIn("nosuch", str(e.exception))


if __name__ == "__main__":
    unittest.main()
