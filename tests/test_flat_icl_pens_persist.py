"""S57-ICL-PEN-RECOLOUR wiring: amarchy-flat's icon-label shade pens stay obtained across Text calls (red pixels in the grey
antialias came from the per-call ReleasePen), bound to the screen identity (icl_cache_bind), and are given back only at unload on
the live Workbench map. The behaviour itself is tested in tests/test_iclabel.c; this guards that flat.c uses it."""
import pathlib, re, unittest

SRC = (pathlib.Path(__file__).resolve().parent.parent / "amarchy/flat.c").read_text()


def body(name):
    i = SRC.index("\n" + name)
    return SRC[i:SRC.index("\n}\n", i)]


class FlatIclPensPersist(unittest.TestCase):
    def test_label_call_does_not_drain(self):
        self.assertNotIn("icl_cache_drain", SRC)

    def test_label_call_binds_identity(self):
        self.assertIn("icl_tab_for(&icl_pc, w->WScreen, flat_draw_epoch(&g_draw, w->WScreen))", body("static int icl_label("))

    def test_no_static_colormap_pointer(self):
        self.assertIsNone(re.search(r"^static\s+struct\s+ColorMap\s*\*", SRC, re.M), "a ColorMap pointer outlives a call")
        self.assertNotIn("ReleasePen(icl_", SRC)

    def test_full_cache_never_obtains(self):
        b = body("static int icl_pen_for(")
        self.assertLess(b.index("icl_cache_has_room("), b.index("ObtainBestPen("))

    def test_label_call_verifies_held_pens(self):
        b = body("static int icl_label(")
        # S62-FLAT-RF-TX-BUDGET: the ColorMap travels in icx.cm; the verify still runs before the label is drawn (icl_run)
        self.assertLess(b.index("icl_tab_for("), b.index("icl_cache_verify(icx.pc, cm_pen_is_free, icx.cm)"))
        self.assertLess(b.index("icl_cache_verify(icx.pc, cm_pen_is_free, icx.cm)"), b.index("icl_run("))

    def test_watch_step_verifies_flat_pens(self):
        b = body("static void pens_run(")
        self.assertLess(b.index("flat_pens_verify(sl"), b.index("flat_pens_follow_on(sl"))

    def test_unload_releases_on_live_identity(self):
        b = body("static void icl_pens_release(")
        self.assertIn("icl_cache_release(c, cm ? c->screen : NULL, c->epoch", b)   # TE-10.6b: each slot, on its own screen's map
        self.assertIn("LockPubScreenList()", b)
        self.assertIn("flat_pens_tab_reap(&g_pens", body("static void pens_reap("))
        self.assertIn("icl_pens_release();", body("static void icl_free("))


if __name__ == "__main__":
    unittest.main()
