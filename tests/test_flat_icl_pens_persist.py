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
        self.assertIn("icl_cache_bind(&icl_pc, w->WScreen, ipd_epoch())", body("static int icl_label("))

    def test_no_static_colormap_pointer(self):
        self.assertIsNone(re.search(r"^static\s+struct\s+ColorMap\s*\*", SRC, re.M), "a ColorMap pointer outlives a call")
        self.assertNotIn("ReleasePen(icl_", SRC)

    def test_unload_releases_on_live_identity(self):
        self.assertIn("icl_cache_release(&icl_pc", body("static void icl_pens_release("))
        self.assertIn("icl_pens_release();", body("static void icl_free("))


if __name__ == "__main__":
    unittest.main()
