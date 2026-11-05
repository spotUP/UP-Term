"""s57 boot hang: amarchy-flat's icon-label path cached ObtainBestPen pens (and the raw ColorMap pointer icl_cm) across Text
calls. After IPrefs closed and reopened the Workbench screen, ReleasePen(icl_cm, ...) ran on the freed map and waited for
ever on its dead PalExtra semaphore while Workbench held the layer locks. Pens must be released per call, on the map
they were obtained on: no ColorMap pointer may be kept across calls."""
import pathlib, re, unittest

SRC = (pathlib.Path(__file__).resolve().parent.parent / "amarchy/flat.c").read_text()


class FlatIclNoCrossCallPens(unittest.TestCase):
    def test_no_static_colormap_pointer(self):
        self.assertIsNone(re.search(r"^static\s+struct\s+ColorMap\s*\*", SRC, re.M), "a ColorMap pointer outlives a call")
        self.assertNotIn("icl_cm", SRC)

    def test_release_only_on_this_calls_map(self):
        self.assertNotIn("ReleasePen(icl_", SRC)

    def test_label_call_drains_before_return(self):
        self.assertIn("icl_cache_drain(&icl_pc", SRC)


if __name__ == "__main__":
    unittest.main()
