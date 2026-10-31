"""amarchy_menu_rig dopus_quit starts Directory Opus again at its end, also when a check raises.

The row quits DOpus through Opus > Quit... and used to leave the rig without DOPUS.1, so every later DOpus row on the same rig
was BLOCKED (rig 5, 2026-10-31). restore_dopus runs the boot's own `C:LoadWB` and waits for the DOPUS.1 port.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m


class DopusQuitRestores(unittest.TestCase):
    def test_loadwb_when_quit_left_no_port(self):
        # the quit takes DOPUS.1 away: simulate by a menu body that flips the port off
        state = {"up": True, "cmds": []}

        def port():
            return state["up"]

        def run(cmd, *a, **k):
            state["cmds"].append(cmd)
            if cmd == "C:LoadWB":
                state["up"] = True
            return 0, ""

        def body(fn):
            state["up"] = False
            return True, "3 of 3"
        with mock.patch.object(m, "dopus_port", port), mock.patch.object(m.c, "run", run), \
             mock.patch.object(m, "with_drawn_menu", body), mock.patch.object(m.time, "sleep", lambda s: None):
            m.r_dopus_quit()
        self.assertEqual(state["cmds"], ["C:LoadWB"])
        self.assertTrue(state["up"])

    def test_loadwb_also_when_a_check_raises(self):
        state = {"up": True, "cmds": []}

        def run(cmd, *a, **k):
            state["cmds"].append(cmd)
            state["up"] = True
            return 0, ""

        def body(fn):
            state["up"] = False
            raise RuntimeError("boom")
        with mock.patch.object(m, "dopus_port", lambda: state["up"]), mock.patch.object(m.c, "run", run), \
             mock.patch.object(m, "with_drawn_menu", body), mock.patch.object(m.time, "sleep", lambda s: None):
            with self.assertRaises(RuntimeError):
                m.r_dopus_quit()
        self.assertEqual(state["cmds"], ["C:LoadWB"])


if __name__ == "__main__":
    unittest.main()
