"""Unload waits for the callers inside a patched entry (FLAT-UNLOAD-INFLIGHT).

On unload amarchy-flat restores the vector and then waited a fixed Delay(5) for callers already inside the Text / RectFill
entry. A caller blocked longer (a draw waiting on a layer lock) returned through the entry stub's pop and rts after the
segment was freed: a jump into freed memory. So: the entry code is a template copied into a never-freed AllocMem block that
counts callers in and out (single addq/subq on a counter in the block), and unload polls that counter to 0 after the restore.
"""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "amarchy" / "flat.c"


def code():
    return re.sub(r"/\*.*?\*/", "", SRC.read_text(errors="replace"), flags=re.S)


def asm(name):
    return [l.split(";")[0].strip() for l in (ROOT / "amarchy" / name).read_text().split("\n")]


def func(src, name):
    m = re.search(r"^static [^\n;]*?\b%s\([^\n;]*\)\n\{\n(.*?)^\}" % name, src, re.S | re.M)
    assert m, name
    return m.group(1)


class FlatUnloadInflight(unittest.TestCase):
    def test_no_fixed_delay_after_the_restore(self):
        for fn in ("tx_remove", "rf_remove"):
            body = func(code(), fn)
            self.assertNotRegex(body, r"Delay\(\s*\d{1,2}\s*\)", "%s waits a fixed Delay(n) for in-flight callers" % fn)

    def test_stubs_count_in_and_out(self):
        for name, tag in (("flat_rf.s", "rf"), ("flat_tx.s", "tx")):
            lines = asm(name)
            idx = {i: l for i, l in enumerate(lines)}
            add = [i for i, l in idx.items() if re.match(r"addq\.l\s+#1,\(.*\)\.l$", l)]
            sub = [i for i, l in idx.items() if re.match(r"subq\.l\s+#1,\(.*\)\.l$", l)]
            jsr = [i for i, l in idx.items() if l.startswith("jsr")]
            rts = [i for i, l in idx.items() if l == "rts"]
            self.assertEqual((len(add), len(sub), len(jsr), len(rts)), (1, 1, 1, 1), name)
            self.assertLess(add[0], jsr[0], "%s: count in before the handler runs" % name)
            self.assertLess(jsr[0], sub[0], "%s: count out after the handler returns" % name)
            self.assertLess(sub[0], rts[0], name)
            self.assertEqual([i for i, l in idx.items() if l.startswith("_flat_%s_tpl_c1:" % tag)] != [], True, name)

    def test_unload_waits_on_the_count_and_never_frees_the_block(self):
        src = code()
        self.assertRegex(func(src, "gate_wait"), r"while\s*\(.*count\s*>\s*0\)\s*\n?\s*Delay\(1\)")
        self.assertNotRegex(func(src, "gate_wait"), r"break|return")
        for fn, gate in (("tx_remove", "tgate"), ("rf_remove", "rgate")):
            body = func(src, fn)
            self.assertIn("gate_wait(r->%s)" % gate, body, fn)
            self.assertLess(body.index("flat_swap_remove"), body.index("gate_wait"), "%s: restore first, then wait" % fn)
        self.assertNotRegex(src, r"FreeMem\([^)]*gate", "the entry block is never freed")
        self.assertNotRegex(src, r"FreeVec\(\s*r->[tr]gate")

    def test_entries_are_installed_from_the_block(self):
        src = code()
        self.assertIn("(uintptr_t)r->tgate->code", func(src, "tx_install"))
        self.assertIn("(uintptr_t)r->rgate->code", func(src, "rf_install"))
        self.assertIn("CacheClearU()", func(src, "gate_make"))
        self.assertIn("MEMF_PUBLIC", func(src, "gate_make"))


if __name__ == "__main__":
    unittest.main()
