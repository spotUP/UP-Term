"""amarchy_gui_rig: the PG.8 package cases assert against the Browse WINDOW (12 of 28 rows on PAL), not all rows.

Rig 3 run r1 (research/2026-10-27-s10-pg8-gui-r1.md): pkg_install and pkg_tags compared the shown window with the whole
feed, pkg_fail and pkg_notfound looked for up-zzfail / up-zzgone in the first window only.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_gui_rig as g

FEED = ["up-%02d" % i for i in range(27)] + ["up-zzfail"]
UHC = "pg8uhc"
FULL = ["[UHC] %s 1.0" % UHC] + ["[upkg] %s 1.0" % n for n in FEED]      # 29 rows
WIDTH = 12


class Window:
    """A fake drawn level: Down moves the bar, the window follows it (the product's behaviour on PAL)."""
    def __init__(self, rows, width=WIDTH):
        self.rows, self.width, self.bar, self.top = rows, width, 0, 0

    def shown(self):
        return self.rows[self.top:self.top + self.width]

    def down(self):
        self.bar = min(self.bar + 1, len(self.rows) - 1)
        if self.bar >= self.top + self.width:
            self.top = self.bar - self.width + 1


class SearchPick(unittest.TestCase):
    """A row beyond the Browse window is reached through Search (AMARCHY_ROWS is only published on a full draw, so a
    scroll cannot read it): the keys typed are the name's raw codes, the row is the one that names the package."""
    def test_raw_keys_cover_package_names(self):
        for name in ("zzfail", "zzgone", "pi", "git", "bitchx", "perl"):
            self.assertTrue(all(ch in g.RAW_KEYS for ch in name), name)
        self.assertEqual([g.RAW_KEYS[c] for c in "zq"], [0x31, 0x10])

    def test_pick_row_takes_the_exact_name(self):
        rows = ["[upkg] up-picocom  1.0", "[upkg] up-pi  1.1"]
        self.assertEqual(g.pick_row("up-pi", rows), (["[upkg] up-pi  1.1"], 1))

    def test_pick_row_none_or_ambiguous_is_minus_one(self):
        self.assertEqual(g.pick_row("up-pi", ["No packages here"])[1], -1)
        self.assertEqual(g.pick_row("up-pi", ["[upkg] up-pi  1.0", "[UHC] up-pi  1.0"])[1], -1)


class TagsCase(unittest.TestCase):
    """case_pkg_tags on a 12 row window of 29 rows: the old assertion needed all rows."""
    def run_case(self):
        verdicts = []
        w = Window(FULL)
        with mock.patch.object(g, "feed_dir", return_value="/nofeed"), \
             mock.patch.object(g, "feed_names", return_value=FEED), \
             mock.patch.object(g, "uhc_catalogue", return_value=lambda: None), \
             mock.patch.object(g, "local_feed", return_value=mock.Mock()), \
             mock.patch.object(g, "refresh_to_browse", return_value=w.shown()), \
             mock.patch.object(g, "close_menu"), mock.patch.object(g.c, "run"), \
             mock.patch.object(g, "check", side_effect=lambda ok, what: verdicts.append(ok) or ok), \
             mock.patch("builtins.open", mock.mock_open(read_data="")):
            g.case_pkg_tags()
        return verdicts

    def test_window_of_tagged_rows_passes(self):
        self.assertEqual(self.run_case(), [True])


if __name__ == "__main__":
    unittest.main()
