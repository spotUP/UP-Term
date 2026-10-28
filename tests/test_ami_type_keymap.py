#!/usr/bin/env python3
"""ami typing under the rig's keymap. wasabid's `key type` maps characters with the US default, so ';' came out as
raw key 0x29, which the rig's Swedish console reads as 'o with diaeresis' (upkg-ask rig pass C, 2026-10-26). ami._type
now sends raw keys from the console's own table. tests/data/rkc-table-swedish.txt is rkcprobe TABLE as captured on rig 8
(input.prefs keymap "s"). The one wiring check drives ami.req (the entry every rig script types through) and asserts
the raw events that reach the KEY frame."""
import pathlib, struct, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import ami

TABLE_TEXT = (ROOT / "tests/data/rkc-table-swedish.txt").read_text()
SHIFT = 0x0001


class TypeKeymap(unittest.TestCase):
    def setUp(self):
        self.table = ami.parse_keymap_table(TABLE_TEXT)

    def test_semicolon_is_shift_comma_not_the_us_key(self):
        self.assertEqual(self.table[ord(';')], (0x38, SHIFT))   # 0x29 is the Swedish o-umlaut key
        self.assertEqual(self.table[0xf6], (0x29, 0))

    def test_layout_dependent_characters_all_have_a_key(self):
        for ch in '\'":/\\[]=-;,.() \r\t$!&|<>_#@{}':
            self.assertIn(ord(ch), self.table, repr(ch))

    def test_every_event_is_down_then_up_of_the_same_key(self):
        ev = ami.keys_for_text(b'a;', self.table)
        self.assertEqual(len(ev), 4)
        self.assertEqual(ev[1], (ev[0][0] | 0x80, ev[0][1]))
        self.assertEqual(ev[3], (ev[2][0] | 0x80, ev[2][1]))

    def test_missing_character_is_an_error(self):
        with self.assertRaises(SystemExit):
            ami.keys_for_text(bytes([0x80]), self.table)

    def test_req_type_reaches_the_key_frame_as_raw_events(self):
        sent, saved = [], (ami._keymap_table, ami._keys, ami._conn)

        class Conn:
            def __enter__(s): return object()
            def __exit__(s, *a): return False
        ami._keymap_table = lambda c: self.table
        ami._keys = lambda c, events: sent.append(list(events))
        ami._conn = lambda timeout=None, touch=True: Conn()
        try:
            ami.req(0x08, bytes([4]) + b'x;y')
        finally:
            ami._keymap_table, ami._keys, ami._conn = saved
        flat = [e for chunk in sent for e in chunk]
        self.assertEqual(flat[2], (0x38, SHIFT))
        self.assertNotIn(0x29, [e[0] & 0x7f for e in flat])


if __name__ == '__main__':
    unittest.main()
