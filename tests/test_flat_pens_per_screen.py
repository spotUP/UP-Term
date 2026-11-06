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

    def test_draw_sync_reads_roles_outside_list_lock(self):
        # G6 (TE-10.6c brief 4b): ipd_screen_roles takes the pen record semaphore, which the record sync takes BEFORE the public
        # screen list lock; so draw_sync must never call it while it holds that lock.
        start = SRC.index("static void draw_sync(flatshared *r)")
        body = SRC[start:SRC.index("\n}\n", start)]
        self.assertIn("ipd_screen_roles(", body)
        self.assertIn("flat_role_resolve(", body)
        self.assertRegex(SRC, r"static\s+ipd_roles\s+g_roles_in\s*\[")
        self.assertRegex(SRC, r"static\s+flat_role_pens\s+g_roles_out\s*\[")
        locked = False
        for m in re.finditer(r"LockPubScreenList\s*\(|UnlockPubScreenList\s*\(|ipd_screen_roles\s*\(", body):
            t = m.group(0)
            if t.startswith("Unlock"):
                locked = False
            elif t.startswith("Lock"):
                locked = True
            else:
                self.assertFalse(locked, "ipd_screen_roles called under the public screen list lock")

    # ---- TE-10.6c brief 4c-1: hook gates read the per-screen state pointer ----
    GATED = ("c2_ring_refill", "c2_draw", "c2_get_pens", "c8_skip", "flat_tx_c", "flat_rf_c")
    HOOKS = GATED + ("ghost_menu", "d4_flat")

    @staticmethod
    def _body(name):
        m = re.search(r"^(?:static )?[A-Za-z_][\w \*]*?\b" + name + r"\([^;{]*\)\s*\n\{", SRC, re.M)
        assert m, name
        return SRC[m.start():SRC.index("\n}\n", m.start())]

    def test_g1_gates_read_per_screen_state(self):
        for n in self.GATED:
            b = self._body(n)
            self.assertNotIn("r->st.enabled", b, n)
            self.assertNotRegex(b, r"r->st\.pen_", n)

    def test_g2_no_flat_state_local(self):
        for n in self.HOOKS:
            b = self._body(n)
            self.assertIsNone(re.search(r"\bflat_state\s+\w+\s*(=|;)", b), n)

    def test_g3_no_iconpen_or_intuition_reads_in_hooks(self):
        for n in self.HOOKS:
            b = self._body(n)
            for bad in ("ipd_", "GetScreenDrawInfo", "GetRGB32"):
                self.assertNotIn(bad, b, n)
        calls = [m.start() for m in re.finditer(r"ipd_screen_roles\(", SRC)]
        d0 = SRC.index("static void draw_sync(flatshared *r)")
        d1 = SRC.index("\n}\n", d0)
        for c in calls:
            self.assertTrue(d0 <= c < d1, "ipd_screen_roles( outside draw_sync")

    # G4 (no ObtainPen on a non-Workbench ColorMap) cannot be a source check: icl_pen_fn takes the ColorMap as a parameter.

    def test_g5_no_role_table_names_in_flat_objects(self):
        base = pathlib.Path(__file__).resolve().parent.parent / "amarchy"
        for f in ("flat.c", "flat_core.c", "flat_glyph.c"):
            self.assertNotIn("GTE_", (base / f).read_text(), f)

    def test_g7_colour_count_clamped_to_colormap(self):
        self.assertIn("cm->Count", self._body("draw_screen_colours"))


if __name__ == "__main__":
    unittest.main()
