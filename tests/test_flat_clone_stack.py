"""The c2 clone path runs on a private stack, never on the drawing task's (S63-KS31-STACK, KS31-BOOT-CRASH).

c2_clone runs NewObjectA(sysiclass), AllocBitMap, the saved ROM dispatcher and a ReadPixel loop on whatever task draws the
gadget; IPrefs had 1052 bytes free at that point on KS 3.1 and the boot reset or hung about 1 in 3 times. So: c2_clone calls
flat_clone_draw_in only through flat_on_stack (the asm StackSwap helper), the stack is allocated in main before the hooks install
and freed after they are removed (under cl_ss), and it is at least CLONE_STACK_MIN bytes.
"""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "amarchy" / "flat.c"
CLONE_STACK_MIN = 16384


def code():
    return re.sub(r"/\*.*?\*/", "", SRC.read_text(errors="replace"), flags=re.S)


def func(src, name):
    m = re.search(r"^[a-z][^\n]*\b" + name + r"\([^\n]*\)\n\{\n(.*?)^\}", src, re.S | re.M)
    assert m, f"flat.c has no {name}()"
    return m.group(1)


class FlatCloneStack(unittest.TestCase):
    def test_clone_draw_runs_only_through_flat_on_stack(self):
        src = code()
        uses = [m.start() for m in re.finditer(r"flat_clone_draw_in\(", src)]
        self.assertEqual(len(uses), 1, "flat_clone_draw_in must have exactly one call site")
        self.assertIn("flat_clone_draw_in(", func(src, "clone_on_private"), "the call must live in the function run on the private stack")
        self.assertRegex(func(src, "c2_clone"), r"flat_on_stack\(\s*clone_on_private\s*,", "c2_clone does not call flat_on_stack(clone_on_private, ...)")
        self.assertNotIn("flat_clone_draw_in(", func(src, "c2_clone"), "c2_clone calls the clone on the caller's stack")
        self.assertRegex(func(src, "c2_clone"), r"!cl_stk", "c2_clone has no fallback for a missing stack")

    def test_the_stack_is_big_enough(self):
        m = re.search(r"#define\s+CLONE_STACK_BYTES\s+(\d+)", code())
        self.assertTrue(m, "no CLONE_STACK_BYTES")
        self.assertGreaterEqual(int(m.group(1)), CLONE_STACK_MIN)

    def test_allocated_before_hook_install_and_freed_after_removal(self):
        body = func(code(), "main")
        self.assertIn("clone_stack_alloc(", body)
        self.assertLess(body.index("clone_stack_alloc("), body.index("install_all("), "the stack is allocated after the hooks can install")
        free = body.index("clone_stack_free(")
        for gone in ("hook_remove(r)", "rf_remove(r)", "tx_remove(r)"):
            self.assertLess(body.index(gone), free, f"the stack is freed before {gone}")
        self.assertLess(body.rindex("ObtainSemaphore(&cl_ss)", 0, free), free, "the stack is freed without holding cl_ss")
        self.assertGreater(body.index("ReleaseSemaphore(&cl_ss)", free), free)

    def test_the_asm_helper_swaps_twice_and_is_linked(self):
        asm = (ROOT / "amarchy" / "flat_stk.s").read_text()
        self.assertEqual(asm.count("-732(a6)"), 2, "StackSwap must be called once in, once out")
        self.assertIn("_flat_on_stack", asm)
        self.assertIn("flat_stk.o", (ROOT / "Makefile").read_text())


if __name__ == "__main__":
    unittest.main()
