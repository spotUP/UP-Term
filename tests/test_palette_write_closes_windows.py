"""RL15-IPREFS-SYSREQ: toggling flat look and then picking a theme raised IPrefs' "System Request" ("Intuition is attempting to reset
the Workbench screen. Please close the following windows: ...", Retry/Cancel) and blocked each pick 40-70 s (rig 124, s57). A palette.prefs
USE whose pen map differs from the live DrawInfo pens resets the Workbench screen; IPrefs' CloseWorkBench fails while Amarchy's windows are
open. dk_apply closed them around its palette write (desk_pens_reset); dk_flat_pens, run after it on every pick, did not. Invariant: every
dk_put_palette call in desk_dos.c sits between dk_windows_close and dk_windows_open, guarded by the same pen-reset test.
amarchy/desk_dos.c is Amiga-only code no host test links, so this test reads its structure."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = (ROOT / "amarchy" / "desk_dos.c").read_text()


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
        self.assertNotEqual(close, -1, "dk_flat_pens writes the palette with Amarchy windows open: IPrefs System Request on the reset")
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


if __name__ == "__main__":
    sys.exit(unittest.main())
