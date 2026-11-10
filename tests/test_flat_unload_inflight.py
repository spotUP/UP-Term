"""Unload waits for the callers inside a patched entry (FLAT-UNLOAD-INFLIGHT).

On unload amarchy-flat restored the vector and then waited a fixed Delay for callers already inside the patched entry. A
caller blocked longer returned through the entry's pop and rts after the segment was freed: a jump into freed memory. So: the
entry code is a template copied into a never-freed AllocMem block that counts callers in and out, and unload polls that counter
to 0 after the restore. A second window: a task that has fetched the entry address but has not yet counted in is invisible to
the wait; so every gate counts in FIRST, then tests a closed flag, and a closed gate passes the call on to the original
function (nothing of ours runs). Unload order, at all four sites (Text, RectFill, sysiclass dispatcher, dopusiclass
dispatcher; plus the frameiclass dispatcher that hook_remove unloads with the sysiclass one): close, restore, wait.
FLAT_ROOT points the test at another tree (the fail-before run against commit 288c14f8).
"""
import os, pathlib, re, unittest

ROOT = pathlib.Path(os.environ.get("FLAT_ROOT") or pathlib.Path(__file__).resolve().parent.parent)
SRC = ROOT / "amarchy" / "flat.c"


def code():
    return re.sub(r"/\*.*?\*/", "", SRC.read_text(errors="replace"), flags=re.S)


def asm(name):
    return [l.split(";")[0].strip() for l in (ROOT / "amarchy" / name).read_text().split("\n")]


def func(src, name):
    m = re.search(r"^static [^\n;]*?\b%s\([^\n;]*\)\n\{\n(.*?)^\}" % name, src, re.S | re.M)
    assert m, name
    return m.group(1)


GATES = (("flat_rf.s", "rf"), ("flat_tx.s", "tx"), ("flat_dg.s", "dg"))
# unload function -> [(gate field, swap field)] in unload order
SITES = (("tx_remove", [("tgate", "tsw")]), ("rf_remove", [("rgate", "rsw")]),
         ("hook_remove", [("fgate", "fsw"), ("sgate", "sw")]), ("dopus_unpatch", [("d5gate", "d5sw")]))


class FlatUnloadInflight(unittest.TestCase):
    def test_no_fixed_delay_after_the_restore(self):
        for fn in ("tx_remove", "rf_remove", "hook_remove"):
            body = func(code(), fn)
            self.assertNotRegex(body, r"Delay\(\s*\d{1,2}\s*\)", "%s waits a fixed Delay(n) for in-flight callers" % fn)
        # the D4 library entries (dsw) are gated too: dopus_unpatch waits on the counts, never on a fixed time
        self.assertNotRegex(func(code(), "dopus_unpatch"), r"Delay\(", "dopus_unpatch waits a fixed Delay for in-flight callers")

    def test_no_fixed_delay_in_any_unload_path(self):
        src = code()
        for fn in ("tx_remove", "rf_remove", "hook_remove", "dopus_unpatch"):
            self.assertNotRegex(func_any(src, fn), r"Delay\(", fn)
        self.assertEqual(re.findall(r"\bDelay\(\s*(\w+)\s*\)", func(src, "gate_wait")), ["1"], "the only Delay on an unload path is the poll inside gate_wait")

    def test_every_patched_dopus_entry_reaches_our_code_only_through_a_gate(self):
        src = code()
        scan = func_any(src, "dopus_scan")
        # no swap/body install may be handed one of our handlers directly: the operand is always a gate block's code
        for call in re.findall(r"flat_(?:swap|body)_install\(([^;]*?)\)\s*(?:!=|==)", scan, re.S):
            if "dsw" in call:
                self.assertIn("->code", call, "ungated install: " + " ".join(call.split()))
        self.assertEqual(len(re.findall(r"flat_(?:swap|body)_install\(&r->dsw\[el\]", scan)), 2)
        self.assertEqual(len(re.findall(r"gate_open\(g,", scan)), 2, "both install paths open their gate after the swap")
        self.assertIn("gate_make(", func_any(src, "dgate_get"))
        # unload: each element is closed before its restore, reopened when refused, waited on after the Forbid
        un = func_any(src, "dopus_unpatch")
        c = un.find("gate_close(r->dgate[el])")
        rm = un.find("flat_body_remove(&r->dsw[el]")
        rm2 = un.find("flat_swap_remove(&r->dsw[el]")
        ro = un.find("gate_reopen(r->dgate[el])")
        pm = un.find("Permit()")
        w = un.find("gate_wait(r->dgate[el])")
        cl = un.find("CloseLibrary(r->dlib)")
        self.assertTrue(0 <= c < rm < rm2 < ro < pm < w < cl, (c, rm, rm2, ro, pm, w, cl))
        # the stack-convention body handlers absorb the gate's return address
        self.assertRegex(src, r"d4_fieldbody\(void \*caller_return, ")
        self.assertRegex(src, r"d4_bevelbody\(void \*caller_return, ")
        self.assertNotRegex(src, r"FreeMem\(\s*r->dgate")

    def test_every_gate_counts_in_before_the_closed_test_and_the_pass_through_counts_out(self):
        for name, tag in GATES:
            lines = asm(name)
            def first(pat, start=0):
                for i in range(start, len(lines)):
                    if re.match(pat, lines[i]):
                        return i
                self.fail("%s: no line matching %s" % (name, pat))
            add = first(r"addq\.l\s+#1,\(\$11111111\)\.l$")
            tst = first(r"tst\.l\s+\(\$22222222\)\.l$")
            bne = first(r"bne\.s\s+_flat_%s_tpl_pass$" % tag)
            self.assertEqual(add + 1, tst, "%s: the closed test is the instruction right after the count-in" % name)
            self.assertEqual(tst + 1, bne, name)
            run_sub = first(r"subq\.l\s+#1,\(\$11111111\)\.l$", bne)
            rts = first(r"rts$", run_sub)
            jsr = first(r"jsr\b", bne)
            self.assertLess(jsr, run_sub, "%s: count out after the handler returns" % name)
            self.assertEqual(rts, run_sub + 1, name)
            pas = first(r"_flat_%s_tpl_pass:$" % tag, rts)
            psub = first(r"subq\.l\s+#1,\(\$11111111\)\.l$", pas)
            pmov = first(r"move\.l\s+\(\$33333333\)\.l,-\(a7\)$", pas)
            prts = first(r"rts$", pas)
            self.assertTrue(pas < psub < pmov < prts, "%s: pass-through counts out, then runs the original, no register used" % name)
            body = [l for l in lines[pas:prts + 1] if l]
            self.assertFalse([l for l in body if re.search(r"\b[ad][0-6]\b|a7", l) and "-(a7)" not in l], "%s: pass-through touches a register" % name)
            self.assertEqual(len([l for l in lines if l.startswith("jsr")]), 1, name)

    def test_dispatcher_gate_is_one_plain_jsr_with_no_register_or_argument_traffic(self):
        lines = [l for l in asm("flat_dg.s") if l]
        run = lines[lines.index("bne.s\t_flat_dg_tpl_pass") + 1:lines.index("_flat_dg_tpl_pass:")]
        self.assertEqual(run, ["jsr\t($44444444).l", "subq.l\t#1,($11111111).l", "rts"])

    def test_stack_dispatchers_take_a_leading_dummy_parameter(self):
        src = code()
        self.assertRegex(src, r"ULONG c2_disp_stk\(void \*caller_return, struct Hook \*h, Object \*obj, Msg msg\)")
        self.assertRegex(src, r"ULONG c8_disp_stk\(void \*caller_return, struct Hook \*h, Object \*obj, Msg msg\)")
        self.assertNotRegex(func_any(src, "c2_disp_stk"), r"caller_return[^)]")

    def test_all_four_unloads_close_restore_then_wait(self):
        src = code()
        self.assertRegex(func(src, "gate_wait"), r"while\s*\(.*count\s*>\s*0\)\s*\n?\s*Delay\(1\)")
        self.assertNotRegex(func(src, "gate_wait"), r"break|return")
        self.assertRegex(func(src, "gate_close"), r"closed\s*=\s*1")
        for fn, pairs in SITES:
            body = func_any(src, fn)
            pos = 0
            for gate, sw in pairs:
                c = body.find("gate_close(r->%s)" % gate, pos)
                r_ = body.find("flat_swap_remove(&r->%s" % sw, pos)
                w = body.find("gate_wait(r->%s)" % gate, pos)
                self.assertTrue(0 <= c < r_ < w, "%s: %s must be closed, then restored, then waited on (%d %d %d)" % (fn, gate, c, r_, w))
                pos = w
        self.assertNotRegex(src, r"FreeMem\(\s*r->\w*gate", "the gate blocks are never freed once entered")
        self.assertNotRegex(src, r"FreeVec\(\s*r->\w*gate")

    def test_gates_are_installed_from_the_block_with_the_original_set_after_the_swap(self):
        src = code()
        for fn, gate in (("tx_install", "tgate"), ("rf_install", "rgate"), ("hook_install", "sgate")):
            body = func_any(src, fn)
            self.assertIn("(uintptr_t)r->%s->code" % gate, body, fn)
            self.assertIn("gate_open(r->%s" % gate, body, fn)
        self.assertIn("(uintptr_t)r->fgate->code", func_any(src, "hook_install"))
        self.assertIn("(uintptr_t)r->d5gate->code", src)
        self.assertIn("gate_open(r->d5gate", src)
        self.assertIn("CacheClearU()", func(src, "gate_make"))
        self.assertIn("MEMF_PUBLIC", func(src, "gate_make"))


def func_any(src, name):
    m = re.search(r"^(?:static )?[^\n;]*?\b%s\([^\n;]*\)\n\{\n(.*?)^\}" % name, src, re.S | re.M)
    assert m, name
    return m.group(1)


if __name__ == "__main__":
    unittest.main()
