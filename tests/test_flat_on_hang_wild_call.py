"""s28 flat-ON hang: Patina stopped answering right after flat-system-look was switched on. patina-flat's Text patch
(flat_tx_c) called the stock graphics.library Text through its saved pointer with a6 = SysBase (left by FindTask), the
ROM Text read its base from a6 and jumped through a garbage base into DOS device data, where it spun forever.

Every call of a saved LIBRARY routine (an LVO patched through flat_swap_install on <base> + LVO: Text, RectFill, the
dopus5 DrawBox/FieldBox) must pass that library's base in a6. The class-hook calls (sw, fsw: h_SubEntry) are hooks,
not LVOs, and take no base."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = (ROOT / "patina/flat.c").read_text()

# saved-swap field -> the library base the stock routine expects in a6
LVO_SWAPS = {"tsw": "GfxBase", "rsw": "GfxBase", "dsw": "dlib"}

# Calls through dsw[el].saved that are NOT LVO calls. When flat_body_install succeeds, dsw[el].saved is the trampoline into
# a dopus5 internal BODY routine with stack convention (flat.c comment above the fieldbodyfn typedef; install in
# flat_body_install). The bodies save a6 and read every argument, GfxBase included, from the stack frame; their entry bytes
# are checked before install (field 48 e7 20 32 41 ef 00 14, bevel 48 e7 38 32 ..., patina/flat_core.c). So a6 is not an
# input and no base belongs in it; the bevel has no LVO at all. Exempt exactly these two typedefs; every other dsw call
# (d4_drawbox, d4_fieldbox) is register convention and keeps the a6 rule.
STACK_BODY_TYPEDEFS = {"fieldbodyfn", "bevelbodyfn"}


def typedefs():
    return {m.group(1): m.group(2) for m in re.finditer(r"typedef\s+\w+\s*\(\*\s*(\w+)\s*\)\s*(\([^;]*?\));", SRC)}


def saved_calls():
    """(typedef name, swap field, argument text) for every ((type)X->swap[...].saved)(args) call."""
    pat = r"\(\((\w+)\)\s*[\w>-]+?->(\w+)(?:\[[^\]]*\])?\.saved\)\(([^;]*)\);"
    return [(m.group(1), m.group(2), m.group(3)) for m in re.finditer(pat, SRC)]


class FlatOnHangWildCall(unittest.TestCase):
    def test_every_saved_lvo_call_exists(self):
        seen = {swap for _, swap, _ in saved_calls()}
        for swap in LVO_SWAPS:
            self.assertIn(swap, seen, f"no call through {swap}.saved found: the parser no longer matches flat.c")

    def test_stack_body_exemptions_still_match(self):
        names = {name for name, swap, _ in saved_calls() if swap == "dsw"}
        for name in STACK_BODY_TYPEDEFS:
            self.assertIn(name, names, f"exempt {name} is no longer called through dsw.saved: drop it from the exemption")

    def test_saved_lvo_typedefs_take_a6(self):
        td = typedefs()
        bad = []
        for name, swap, _ in saved_calls():
            if swap in LVO_SWAPS and name not in STACK_BODY_TYPEDEFS:
                if name not in td:
                    bad.append(f"{name} (called through {swap}.saved): typedef not found")
                elif '__reg("a6")' not in td[name]:
                    bad.append(f"{name} (called through {swap}.saved) does not load a6")
        self.assertEqual(bad, [], "\n".join(bad))

    def test_saved_lvo_calls_pass_the_library_base(self):
        bad = []
        for name, swap, args in saved_calls():
            if swap in LVO_SWAPS and name not in STACK_BODY_TYPEDEFS:
                last = args.rsplit(",", 1)[-1]
                if LVO_SWAPS[swap] not in last:
                    bad.append(f"{swap}.saved({args}) does not pass {LVO_SWAPS[swap]} last (a6)")
        self.assertEqual(bad, [], "\n".join(bad))

if __name__ == "__main__":
    unittest.main()
