"""installer_rig: the base Uninstall leaves DOpus's own ;BEGIN DOpus5 block in S:User-Startup, and nothing else."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

BEFORE = "Echo hi\n;BEGIN GG\nAssign GG: DH0:GG\n;END GG\n"
BLOCK = ';BEGIN DOpus5\nAssign DOpus5: "DH0:Opus5"\nPath DOpus5:C ADD\n;END DOpus5\n'


class Checks:
    def __init__(self):
        self.results = []

    def __call__(self, ok, what, detail=""):
        self.results.append((bool(ok), what))


class StartupBlock(unittest.TestCase):
    def run_check(self, before, now, leaves=("DOpus5",)):
        import install_rig as ir
        ck = Checks()
        ir.check_startup_after_uninstall(ck, before, now, leaves)
        return ck.results

    def test_block_left_and_rest_as_before_passes(self):
        res = self.run_check(BEFORE, BEFORE + BLOCK)
        self.assertEqual(len(res), 2)
        self.assertTrue(all(ok for ok, _ in res))

    def test_block_taken_out_fails_the_presence_check(self):
        res = self.run_check(BEFORE, BEFORE)
        self.assertEqual([ok for ok, _ in res], [True, False])

    def test_other_difference_fails_the_byte_check(self):
        res = self.run_check(BEFORE, BEFORE + "Echo leftover\n" + BLOCK)
        self.assertFalse(res[0][0])

    def test_no_leaves_is_byte_for_byte(self):
        self.assertEqual([ok for ok, _ in self.run_check(BEFORE, BEFORE + BLOCK, ())], [False])
        self.assertEqual([ok for ok, _ in self.run_check(BEFORE, BEFORE, ())], [True])

    def test_block_already_there_before_is_not_demanded(self):
        res = self.run_check(BEFORE + BLOCK, BEFORE + BLOCK)
        self.assertEqual(len(res), 1)
        self.assertTrue(res[0][0])


if __name__ == "__main__":
    unittest.main()
