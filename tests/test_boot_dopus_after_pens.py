"""S92-TBCOLOUR (rig 98, KS 3.1 planar AGA depth 4, DOpus 5 as Workbench): the boot apply sent the DOpus theme before dk_live_pens, so
Opus remapped its toolbar brushes (once, L_RemapImage) against pens that changed under it, and the Workbench ColorMap had no free pen
for the icon holder, so icon slots 8..15 kept the graphics defaults (pen 10 7f007f magenta). Invariants: (1) desk_dos_start sends the
DOpus theme only after dk_live_pens, with dopus_tb_ops.force set so the kept bank is set again; (2) dk_live_pens writes the icon pens
8..15 (ipd_icon_pens_direct) for a theme, after its role pens; (3) dopus_theme_send honours force. Amiga-only code: read as structure."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
DESK = (ROOT / "patina" / "desk_dos.c").read_text()
MT = (ROOT / "patina" / "menu_theme.c").read_text()
IPD = (ROOT / "patina" / "iconpen_dos.c").read_text()

sys.path.insert(0, str(ROOT / "tests"))
from test_revert_live_pens_deferred import body  # noqa: E402


class BootDopusAfterPens(unittest.TestCase):
    def test_boot_send_after_live_pens_forced(self):
        k = DESK.index("PATINA_DESK_START_LIVE_PAL")
        tail = DESK[k:DESK.index("PATINA_DESK_START_WRITES", k)]
        live = tail.index("dk_live_pens(")
        self.assertNotIn("dopus_theme_send(", tail[:live], "boot sends the DOpus theme before the pens are final")
        self.assertNotIn("desk_dos_dopus_theme(", tail[:live], "boot sends the DOpus theme before the pens are final")
        send = tail.index("dopus_theme_send(", live)
        self.assertIn("tb.force = 1", tail[live:send], "the boot send must force the bank re-set (Opus remaps once)")

    def test_live_pens_writes_icon_pens_after_role_pens(self):
        b = body(DESK, "static void dk_live_pens(const gtheme *th, int flat)")
        self.assertLess(b.rindex("SetRGB32("), b.index("ipd_icon_pens_direct()"), "icon pens 8..15 written before the role pens")

    def test_icon_pens_direct_rules(self):
        b = body(IPD, "int ipd_icon_pens_direct(void)")
        self.assertIn("ipd_pen_not_exclusive(", b, "a pen another client holds exclusive must be skipped")
        self.assertIn("depth == 4 || depth == 5", b, "only depth 4/5 maps slots 8..15 onto pens 8..15 (ip_fold)")
        self.assertLess(b.index("ipd_icon_rgb("), b.index("LockPubScreenList()"), "theme read (DOS) under the list lock")

    def test_theme_send_honours_force(self):
        head = "int dopus_theme_send(const gtheme *th, long (*send)(const char *port, const char *line, char *result, int cap, void *ud),\n" \
               "                     void *ud, const dopus_tb_ops *tb)"
        self.assertIn("tb->force", body(MT, head))


if __name__ == "__main__":
    unittest.main()
