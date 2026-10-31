"""amarchy_menu_rig dopus_absent starts the commodity and forces the drawn menu before it opens the root menu.

`--only dopus_absent` on a fresh --no-dopus rig failed its last check with an empty row list: the row called
open_terminal, which never launches the commodity, so Super+Space opened nothing (rig 5, 2026-10-31).
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m
import amarchy_rig


class DopusAbsentStartsCommodity(unittest.TestCase):
    def test_commodity_started_before_root_menu(self):
        order = []
        with mock.patch.object(m, "dopus_port", lambda: None), \
             mock.patch.object(m, "dopus_windows", lambda: []), \
             mock.patch.object(m, "DOPUS_SCRIPTS", []), \
             mock.patch.object(m.c, "run", lambda *a, **k: (0, "")), \
             mock.patch.object(m, "ensure_terminal", lambda: order.append("ensure")), \
             mock.patch.object(m, "open_terminal", lambda: order.append("open")), \
             mock.patch.object(m, "x9_drawn", lambda *a: order.append("drawn")), \
             mock.patch.object(m.ami, "read_file", lambda p: b""), \
             mock.patch.object(m.ami, "restore_file", lambda p, b: None), \
             mock.patch.object(amarchy_rig, "arexx", lambda *a: ""), \
             mock.patch.object(m, "root_menu_rows", lambda: order.append("rows") or (True, ["Apps"])), \
             mock.patch.object(m.time, "sleep", lambda s: None):
            ok, _ = m.r_dopus_absent()
        self.assertTrue(ok)
        self.assertEqual(order, ["ensure", "drawn", "rows"])


if __name__ == "__main__":
    unittest.main()
