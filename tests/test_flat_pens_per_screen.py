"""TE-10.6b wiring guard: amarchy/flat.c keeps pen numbers per screen. The draw hooks take them from the slot of w->WScreen
(st_scr / st_rp -> flat_state_for), never from one screen-global pen set; the icon-label cache is a per-screen icl_pentab; no
bare LockPubScreen call. The overlay logic itself is tested in tests/test_flat_core.c and tests/test_iclabel.c."""
import pathlib, re, unittest

SRC = (pathlib.Path(__file__).resolve().parent.parent / "amarchy/flat.c").read_text()


class FlatPensPerScreen(unittest.TestCase):
    def test_tables_not_single_sets(self):
        self.assertIn("static flat_pens_tab g_pens;", SRC)
        self.assertIn("static icl_pentab icl_pc;", SRC)
        self.assertIsNone(re.search(r"\bflat_pens\s+g_pens\b", SRC))
        self.assertIsNone(re.search(r"\bicl_pencache\s+icl_pc\b", SRC))

    def test_hooks_overlay_slot_of_their_screen(self):
        self.assertIn("flat_state_for(&r->st, &g_pens, scr, scr_epoch(scr)", SRC)
        self.assertGreaterEqual(SRC.count("st_rp(") + SRC.count("st_scr("), 6)

    def test_no_bare_lock_pub_screen(self):
        self.assertIsNone(re.search(r"(?<![A-Za-z_])LockPubScreen\s*\(", SRC))


if __name__ == "__main__":
    unittest.main()
