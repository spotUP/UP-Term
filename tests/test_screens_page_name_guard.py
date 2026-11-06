"""TE-10.8b source guard: the Screens page reads public screen names at full length.

A public screen name holds up to 139 characters (MAXPUBSCREENNAME); a rule line holds the same (GT_SCR_PAT = 140 with the
terminator). The page's loader (gt_dos_screens_load, amarchy/gthemes_dos.c) once sampled the names into 32-byte slots, so a
longer name showed cut on its row and the exact rule saved for it never matched the screen. The sample buffer and the stride
given to ipd_pub_screens must both be GT_SCR_PAT.
"""
import pathlib, re, unittest

SRC = pathlib.Path(__file__).resolve().parent.parent / "amarchy" / "gthemes_dos.c"


def loader_body():
    text = SRC.read_text(encoding="latin-1")
    start = text.index("static void gt_dos_screens_load(void)\n{")      # the definition, not the forward declaration
    return text[start:text.index("\n}\n", start)]


class ScreensPageNameGuard(unittest.TestCase):
    def test_sample_buffer_holds_a_full_screen_name(self):
        m = re.search(r'static\s+char\s+open\s*\[\s*GT_SE_ROWS\s*\]\s*\[\s*(\w+)\s*\]', loader_body())
        self.assertIsNotNone(m, "gt_dos_screens_load no longer declares the open[] sample buffer; update this guard")
        self.assertEqual(m.group(1), "GT_SCR_PAT")

    def test_sampler_stride_is_the_full_name_length(self):
        m = re.search(r'ipd_pub_screens\s*\(\s*open\s*\[\s*0\s*\]\s*,\s*([^,]+?)\s*,', loader_body())
        self.assertIsNotNone(m, "gt_dos_screens_load no longer calls ipd_pub_screens(open[0], ...); update this guard")
        self.assertEqual(m.group(1), "GT_SCR_PAT")


if __name__ == "__main__":
    unittest.main()
