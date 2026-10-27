"""amarchy_menu_rig.x9_level counts its Down keys from where the menu puts the cursor.

A level opens with the cursor on its first item, one row below the scan status row when one is
shown (amarchy/menu.c default_item, shift). x9_level counted from the top row, so with a status row
it pressed one Down too many and opened the group below the one asked for: app_index check
"4 Games level lists the project" saw "Scanning... starting Rescan apps" (rig 5, 2026-10-25).
"""
import pathlib, re, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m


def screen(rows):
    return "\n".join(["Amarchy", "+------+"] + ["|  %s  |" % r for r in rows] + ["Filter: "])


class LevelCursor(unittest.TestCase):
    def downs(self, rows, label):
        keys = []
        import amarchy_gui_rig as g
        with mock.patch.object(m, "screen_text", return_value=screen(rows)), \
             mock.patch.object(g, "rows_now", return_value=[r for r in rows if not m.SCAN_STATUS.match(r + "x")]), \
             mock.patch.object(m.c, "run", return_value=(0, "")), \
             mock.patch.object(m.ami, "key", side_effect=lambda k, *a: keys.append(k)), \
             mock.patch.object(m.time, "sleep"):
            m.x9_level(label)
        self.assertEqual(keys[-1], m.RAW["return"])
        return keys.count(m.RAW["down"])

    def test_no_status_row(self):
        self.assertEqual(self.downs(["Apps", "Games", "System"], "Games"), 1)

    def test_status_row_is_above_the_cursor(self):
        for status in ("Scanning... starting", "Scanning... VTCX:Apps", "Scan done: 258 apps, 3 games",
                       "Scan did not start", "Scan stopped at VTCX:", "Scan could not save the list"):
            self.assertEqual(self.downs([status, "Apps", "Games", "System"], "Games"), 1, status)

    def test_status_texts_are_the_ones_the_menu_writes(self):
        src = (ROOT / "amarchy/index_core.c").read_text()
        for text in re.findall(r'"(Scan[^"]*)"', src):
            self.assertTrue(m.SCAN_STATUS.match(text + "x"), text)


if __name__ == "__main__":
    unittest.main()
