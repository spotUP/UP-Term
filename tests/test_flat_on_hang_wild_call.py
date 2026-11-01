"""s28 flat-ON hang: Amarchy stopped answering right after flat-system-look was switched on. amarchy-flat's Text patch
(flat_tx_c) called the stock graphics.library Text through its saved pointer with a6 = SysBase (left by FindTask), the
ROM Text read its base from a6 and jumped through a garbage base into DOS device data, where it spun forever.

Every call of a saved LIBRARY routine (an LVO patched through flat_swap_install on <base> + LVO: Text, RectFill, the
dopus5 DrawBox/FieldBox) must pass that library's base in a6. The class-hook calls (sw, fsw: h_SubEntry) are hooks,
not LVOs, and take no base."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = (ROOT / "amarchy/flat.c").read_text()

# saved-swap field -> the library base the stock routine expects in a6
LVO_SWAPS = {"tsw": "GfxBase", "rsw": "GfxBase", "dsw": "dlib"}


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

    def test_saved_lvo_typedefs_take_a6(self):
        td = typedefs()
        for name, swap, _ in saved_calls():
            if swap in LVO_SWAPS:
                self.assertIn(name, td, name)
                self.assertIn('__reg("a6")', td[name], f"{name} (called through {swap}.saved) does not load a6")

    def test_saved_lvo_calls_pass_the_library_base(self):
        for name, swap, args in saved_calls():
            if swap in LVO_SWAPS:
                last = args.rsplit(",", 1)[-1]
                self.assertIn(LVO_SWAPS[swap], last, f"{swap}.saved({args}) does not pass {LVO_SWAPS[swap]} last (a6)")


if __name__ == "__main__":
    unittest.main()
