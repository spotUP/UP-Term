"""keymap_rig presses the font keys by the key that prints '+' or '-' on the rig's keymap.

The handler names minus and plus by the key that prints them on the active keymap (config/keymap.c
keymap_set_layout, resolved through MapANSI when the keymap loads). A rig whose ENV:Sys/input.prefs
names the Swedish keymap has '+' on raw 0x0B and '-' on raw 0x3A; the rows used to press the USA
codes 0x0B and 0x0C, so font_smaller ran Bigger (font-size 9 -> 11) and font_bigger hit nothing
(rig 5, 2026-10-25). The rows now carry the key name and look the code up by the rig's keymap.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import keymap_rig as m


def input_prefs(keymap):
    """An input.prefs body: a PRHD chunk, then the INPT chunk whose struct InputPrefs (OS 3.1 prefs/input.h)
    starts with ip_Keymap[16], the keymap name."""
    name = keymap.encode("latin-1").ljust(16, b"\0")
    return b"FORM\0\0\0\x40PREF" + b"PRHD\0\0\0\x06" + b"\0" * 6 + b"INPT" + b"\0\0\0\x24" + name + b"\0" * 20


class ActiveKeymap(unittest.TestCase):
    def keymap(self, data):
        with mock.patch.object(m.ami, "read_file", return_value=data):
            return m.active_keymap()

    def test_the_input_prefs_name_the_keymap(self):
        self.assertEqual(self.keymap(input_prefs("s")), "s")
        self.assertEqual(self.keymap(input_prefs("usa")), "usa")

    def test_no_prefs_or_no_name_is_usa(self):
        self.assertEqual(self.keymap(b""), "usa")
        self.assertEqual(self.keymap(None), "usa")
        self.assertEqual(self.keymap(input_prefs("")), "usa")


class SymbolRaw(unittest.TestCase):
    def raw(self, keymap, name):
        with mock.patch.object(m.ami, "read_file", return_value=input_prefs(keymap)):
            return m.symbol_raw(name)

    def test_swedish_plus_is_0x0b_minus_0x3a(self):
        self.assertEqual(self.raw("s", "plus"), (0x0B, "s"))
        self.assertEqual(self.raw("s", "minus"), (0x3A, "s"))

    def test_usa_minus_is_0x0b_plus_0x0c(self):
        self.assertEqual(self.raw("usa", "minus"), (0x0B, "usa"))
        self.assertEqual(self.raw("usa", "plus"), (0x0C, "usa"))

    def test_a_keymap_without_a_table_gives_none(self):
        self.assertEqual(self.raw("d", "plus"), (None, "d"))


class FontRows(unittest.TestCase):
    def row(self, name):
        return next(r for r in m.ROWS if r[0] == name)

    def test_the_font_rows_name_their_keys(self):
        # a fixed raw code here is the rig 5 failure: the code must follow the keymap at run time
        self.assertEqual(self.row("font_smaller")[1], "minus")
        self.assertEqual(self.row("font_bigger")[1], "plus")

    def run_row(self, name, sizes):
        keys = []
        states = [{"km-hits": n, "font-size": s} for n, s in enumerate(sizes)]
        with mock.patch.object(m.ami, "read_file", return_value=input_prefs("s")), \
             mock.patch.object(m.ami, "key", side_effect=lambda k, q=0: keys.append((k, q))), \
             mock.patch.object(m, "counters", side_effect=states), \
             mock.patch.object(m.time, "sleep"):
            ok, detail = m.run_row(self.row(name))
        return ok, detail, keys

    def test_font_bigger_presses_the_swedish_plus_key(self):
        ok, detail, keys = self.run_row("font_bigger", [8, 9])
        self.assertTrue(ok, detail)
        self.assertEqual(keys, [(0x0B, m.LAMIGA)])
        self.assertIn("keymap s raw 0x0B font-size 8 -> 9", detail)

    def test_font_smaller_steps_up_first_then_presses_the_swedish_minus_key(self):
        ok, detail, keys = self.run_row("font_smaller", [9, 8])
        self.assertTrue(ok, detail)
        self.assertEqual(keys, [(0x0B, m.LAMIGA), (0x3A, m.LAMIGA)])
        self.assertIn("keymap s raw 0x3A font-size 9 -> 8", detail)

    def test_the_wrong_direction_fails_the_row(self):
        ok, detail, _ = self.run_row("font_smaller", [9, 11])
        self.assertFalse(ok, detail)


if __name__ == "__main__":
    unittest.main()
