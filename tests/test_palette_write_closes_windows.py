"""RL15-IPREFS-SYSREQ: toggling flat look and then picking a theme raised IPrefs' "System Request" ("Intuition is attempting to reset
the Workbench screen. Please close the following windows: ...", Retry/Cancel) and blocked each pick 40-70 s (rig 124, s57). A palette.prefs
USE whose pen map differs from the live DrawInfo pens resets the Workbench screen; IPrefs' CloseWorkBench fails while Patina's windows are
open. dk_apply closed them around its palette write (desk_pens_reset); dk_flat_pens, run after it on every pick, did not. Invariant: every
dk_put_palette call in desk_dos.c sits between dk_windows_close and dk_windows_open, guarded by the same pen-reset test.
patina/desk_dos.c is Amiga-only code no host test links, so this test reads its structure."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = (ROOT / "patina" / "desk_dos.c").read_text()


def body(src, head):
    j = src.index(head + "\n{") + len(head)   # the definition, not a forward declaration
    depth, k = 0, j + 1
    while True:
        c = src[k]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[j + 1:k + 1]
        k += 1


class PaletteWriteClosesWindows(unittest.TestCase):
    def test_flat_pens_closes_windows_around_a_pen_reset(self):
        b = body(SRC, "static int dk_flat_pens(const gtheme *th)")
        put = b.index("dk_put_palette(")
        close = b.rfind("dk_windows_close()", 0, put)
        self.assertNotEqual(close, -1, "dk_flat_pens writes the palette with Patina windows open: IPrefs System Request on the reset")
        self.assertNotEqual(b.find("dk_windows_open(", put), -1, "dk_flat_pens never reopens the windows it closed")
        self.assertIn("desk_pens_reset(live, nlive, flat, depth)", b, "close only when the pen map changes (a colour-only write resets nothing)")

    def test_every_palette_write_is_bracketed(self):
        callers = []
        for m in re.finditer(r"^(?:static\s+)?(?:const\s+|unsigned\s+)*\w+\s*\**\s*(\w+)\([^;{]*\)\n\{", SRC, re.M):
            name = m.group(1)
            if name == "dk_put_palette":
                continue
            b = body(SRC, SRC[m.start():SRC.index("\n{", m.start())])
            if "dk_put_palette(" in b:
                callers.append(name)
                put = b.index("dk_put_palette(")
                self.assertNotEqual(b.rfind("dk_windows_close()", 0, put), -1, name + " writes the palette with windows open")
                self.assertNotEqual(b.find("dk_windows_open(", put), -1, name + " never reopens the windows")
        self.assertIn("dk_flat_pens", callers)
        self.assertIn("dk_apply", callers)

    def test_boot_start_writes_no_palette(self):
        # AGA-BOOT-DEADLOCK: a boot palette.prefs write made IPrefs reset the Workbench while DOpus started (planar AGA froze)
        b = body(SRC, "int desk_dos_start_apply(void)")
        self.assertNotIn("dk_put_palette(", b, "the boot start-apply writes palette.prefs again: IPrefs resets the Workbench at boot")
        self.assertIn("dk_live_palette(pal, pallen)", b, "the boot theme colours no longer go live on the Workbench screen")

    def test_boot_start_resets_no_workbench_for_font(self):
        # AGA-BOOT-DEADLOCK: a boot font/pattern/pointer USE made IPrefs reset the Workbench while DOpus ran (planar AGA froze)
        b = body(SRC, "int desk_dos_start_apply(void)")
        for editor in ('"Font"', '"WBPattern"', '"Pointer"'):
            self.assertNotIn("dk_put(" + editor, b, "the boot start-apply feeds the %s editor: IPrefs resets the Workbench at boot" % editor)
        for name in ("font", "wbpattern", "pointer"):
            self.assertIn('dk_put_boot("ENVARC:Sys/%s.prefs"' % name, b, "the boot %s part no longer lands in ENVARC: for the next boot" % name)
        self.assertIn("dk_start_persist(plan & ~DK_START_ARC", b, "the start persist copies the old ENV: bytes over the ENVARC:-only parts")


class RuntimePickUnderDopusResetsNothing(unittest.TestCase):
    """S92-DKAPPLY: a runtime theme pick (dk_apply) wrote palette/pattern/pointer/font/icontrol into ENV:, IPrefs reset the Workbench and
    DOpus raced the close/reopen (the AGA-BOOT-DEADLOCK race at runtime). Invariant: with DOpus up no pick writes an ENV: file IPrefs
    watches; the colours go live, the files go to ENVARC: only."""

    def test_apply_sets_arc_only_when_dopus_runs(self):
        b = body(SRC, "int desk_dos_apply(const gtheme *th, char *why, int cap)")
        flag = b.find("dk_arc_only = rexx_port_exists(DOPUS_PORT)")
        self.assertNotEqual(flag, -1, "desk_dos_apply does not switch to ENVARC:-only writes while DOpus runs")
        self.assertLess(flag, b.index("dk_apply(th, why, cap)"), "the DOpus check comes after dk_apply's writes")
        self.assertLess(b.index("dk_wbborder()"), b.index("dk_arc_only = 0"), "dk_wbborder's Workbench.prefs write runs outside the DOpus guard")
        self.assertIn("if (r > 0 && !arc)\n        dk_confirm_ask", b, "the Keep/Revert confirm runs under DOpus: its Revert resets the Workbench")
        r = body(SRC, "int desk_dos_flat_pens_refresh(void)")
        self.assertLess(r.index("dk_arc_only = rexx_port_exists(DOPUS_PORT)"), r.index("dk_flat_pens(&th)"))

    def test_put_writes_envarc_only(self):
        b = body(SRC, "static int dk_put(const char *editor, const char *work, const unsigned char *buf, long len)")
        arc = b.find("if (dk_arc_only)")
        self.assertNotEqual(arc, -1, "dk_put feeds the stock editor (ENV: write, Workbench reset) while DOpus runs")
        self.assertLess(arc, b.index("dk_editor(editor, work)"))
        self.assertIn('strcpy(env, "ENVARC:Sys/")', b[arc:b.index("dk_editor(editor, work)")])

    def test_palette_goes_live_under_dopus(self):
        b = body(SRC, "static int dk_put_palette(const unsigned char *buf, long len)")
        self.assertIn("dk_arc_only)\n        am_sentinel(\"PATINA_DESK_ARC_LIVE_PAL\", (long)dk_live_palette(buf, len))", b,
                      "the pick's colours do not reach the screen without the IPrefs reset")
        i = body(SRC, "static int dk_icontrol(int thin, const gtheme *th)")
        self.assertIn('(dk_arc_only || dk_write("ENV:Sys/icontrol.prefs"', i, "icontrol.prefs still lands in ENV: under DOpus")

    def test_no_reset_bracket_under_dopus(self):
        b = body(SRC, "static int dk_apply(const gtheme *th, char *why, int cap)")
        z = b.find("if (dk_arc_only)\n        reset = old_ws = 0;")
        self.assertNotEqual(z, -1, "dk_apply closes windows and waits for a Workbench reset that must not happen under DOpus")
        self.assertLess(z, b.index("if (reset || old_ws) {"))
        f = body(SRC, "static int dk_flat_pens(const gtheme *th)")
        self.assertIn("int reset = !dk_arc_only && desk_pens_reset(", f)
        self.assertIn("dk_put_palette(buf, len) && !dk_arc_only", f, "dk_flat_pens waits out a reset that never comes under DOpus")
        o = body(SRC, "static void dk_orig_off(unsigned desk_mask, int flat)")
        self.assertIn("if (off && !dk_arc_only)\n        dk_restore_targets(off, DK_ORIG);", o, "switching a part off restores ENV: via editors under DOpus")


if __name__ == "__main__":
    sys.exit(unittest.main())
