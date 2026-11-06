"""TE-10.6b wiring guard: amarchy/flat.c keeps pen numbers per screen. The draw hooks take them from the slot of w->WScreen
(st_scr / st_rp -> flat_draw_state, a table the main task fills with flat_state_for in draw_sync), never from one screen-global pen set; the icon-label cache is a per-screen icl_pentab; no
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
        self.assertIn("static flat_draw_tab g_draw;", SRC)
        self.assertIn("return flat_draw_state(&g_draw, &r->st, scr);", SRC)
        self.assertGreaterEqual(SRC.count("st_rp(") + SRC.count("st_scr("), 6)
        self.assertIn("flat_draw_sync(&g_draw, &r->st, &g_pens,", SRC)

    def test_hooks_copy_no_state_and_call_no_iconpen(self):
        # FLAT-C3: no flat_state stack buffer handed to st_scr / st_rp, no st_pens, no ipd_ call from a hook helper
        self.assertIsNone(re.search(r"st_(scr|rp)\(r,\s*&", SRC))
        self.assertNotIn("st_pens", SRC)
        body = SRC[SRC.index("static const flat_state *st_scr"):SRC.index("static const flat_state *st_rp")]
        self.assertNotIn("ipd_", body)
        self.assertNotIn("scr_epoch", body)
        self.assertIn("flat_draw_epoch(&g_draw, w->WScreen)", SRC)
        self.assertNotIn("icl_tab_for(&icl_pc, w->WScreen, scr_epoch", SRC)

    def test_workbench_pens_written_to_live_state(self):
        # the Workbench hooks read &r->st: pens_run stores its slot's four pens there
        self.assertRegex(SRC, r"flat_pens_refresh_on\(sl, &o, &r->st, &r->st,")
        self.assertRegex(SRC, r"flat_pens_follow_on\(sl, &o, &r->st, &r->st,")

    def test_no_bare_lock_pub_screen(self):
        self.assertIsNone(re.search(r"(?<![A-Za-z_])LockPubScreen\s*\(", SRC))


if __name__ == "__main__":
    unittest.main()
