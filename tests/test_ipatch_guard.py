"""AMARCHY-IPATCH-GATE source guard (evidence: thoughts/shared/s61-evidence/b2-ipatch-gate-harden.md).

The gate counts a caller in its own memory before it reads the hook word; the hook leaves with ipatch_leave on the gate it was
entered through; a gate is never freed; a remove waits for the counters for as long as it takes and only then frees g_pub.
"""
import pathlib, re, unittest

AMARCHY = pathlib.Path(__file__).resolve().parent.parent / "amarchy"
PATCHES = ["strut_patch.c", "wbreset_patch.c", "borderblank_patch.c"]
COMMENT = re.compile(r'/\*.*?\*/|//[^\n]*', re.S)


def clean(name):
    return COMMENT.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), (AMARCHY / name).read_text(errors="replace"))


def functions(text):
    """name -> body of each top-level function (the '{' alone on a line starts it)."""
    out = {}
    for m in re.finditer(r'^[^\s#}][^\n;{]*?\b(\w+)\s*\([^;{]*\)\s*\n\{', text, re.M):
        i = m.end() - 1
        depth = 0
        for j in range(i, len(text)):
            depth += (text[j] == '{') - (text[j] == '}')
            if depth == 0:
                out[m.group(1)] = text[i:j + 1]
                break
    return out


def return_before_free(body):
    tail = body[body.index("ipatch_busy("):]
    return re.search(r'\breturn\b', tail[:tail.index("FreeMem(g_pub")]) is not None


class IpatchGuard(unittest.TestCase):
    def test_gates_are_never_freed(self):
        self.assertNotIn("FreeMem", clean("ipatch.c"))
        self.assertNotIn("ipatch_free", "".join(clean(f) for f in PATCHES))

    def test_hooks_do_not_count_themselves_and_leave_through_the_gate(self):
        for f in PATCHES:
            text = clean(f)
            self.assertNotRegex(text, r'busy\s*(\+\+|--)', f)
            fns = functions(text)
            hooks = [n for n, b in fns.items() if "ipatch_saved(" in b]
            self.assertTrue(hooks, f)
            for n in hooks:
                self.assertIn("ipatch_leave(", fns[n], f"{f}:{n} must call ipatch_leave")

    def test_remove_waits_then_frees_with_no_return_between(self):
        for f in PATCHES:
            bodies = [b for n, b in functions(clean(f)).items() if n.endswith("_remove")]
            self.assertEqual(len(bodies), 1, f)
            self.assertIn("BUSY_WAIT", bodies[0], f)
            self.assertFalse(return_before_free(bodies[0]), f"{f}: return between the busy wait and FreeMem(g_pub)")

    def test_guard_sees_a_violation(self):
        bad = "void r(void)\n{\n  while (ipatch_busy(&a)) Delay(1);\n  if (x) return;\n  FreeMem(g_pub, 1);\n}\n"
        self.assertTrue(return_before_free(functions(bad)["r"]))


if __name__ == "__main__":
    unittest.main()
