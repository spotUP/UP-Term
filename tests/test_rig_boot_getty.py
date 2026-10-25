"""The master disk's S:User-Startup starts `C:upgetty LOOP` at every boot; the wire rigs must
end it (getty_rig.end_boot_getty) before their first check, or check 1 never sees the rig's
own prompt (zmodem_rig 2 of 5, 2026-10-25)."""
import ast, pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import getty_rig

STATUS = """Process  2: Loaded as command: C:upgetty
Process  3: Loaded as command: upgetty LOOP
Process  4: Loaded as command: vsh
Process  5: Loaded as command: Status
Process  6: Loaded as command: VTC:upgetty
Process  7: Loaded as command: C:upgettyx
"""


class Parse(unittest.TestCase):
    def test_upgetty_cli_numbers(self):
        self.assertEqual(getty_rig.process_clis(STATUS), [2, 3, 6])

    def test_other_names(self):
        self.assertEqual(getty_rig.process_clis(STATUS, ("vsh",)), [4])
        self.assertEqual(getty_rig.process_clis(""), [])


class Order(unittest.TestCase):
    def test_fixture_precedes_first_check(self):
        for name in ("getty_rig.py", "zmodem_rig.py"):
            main = next(n for n in ast.parse((ROOT / "tools/rig" / name).read_text()).body
                        if isinstance(n, ast.FunctionDef) and n.name == "main")
            calls = [(c.lineno, c.func) for c in ast.walk(main) if isinstance(c, ast.Call)]
            fix = [l for l, f in calls if getattr(f, "attr", getattr(f, "id", "")) == "end_boot_getty"]
            chk = [l for l, f in calls if getattr(f, "id", "") == "check"]
            self.assertTrue(fix, "%s never calls end_boot_getty" % name)
            self.assertLess(min(fix), min(chk), "%s checks before ending the boot upgetty" % name)


if __name__ == "__main__":
    unittest.main()
