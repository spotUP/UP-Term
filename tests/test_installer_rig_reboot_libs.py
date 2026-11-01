"""fresh-all's reboot check gives LIBS: back to the system's drawer, on the host (no rig).

prepare() assigns LIBS: without the rig's VTC:ixp6 so Install and Uninstall work on
the system's ixemul.library. The reboot in reboot_amarchy_check boots the rig's
LIBS: again, VTC:ixp6 first, and the Uninstall checks after it listed VTC:ixp6's
stale .orig files: "the original ixemul back" failed with Uninstall right
(FRESH-ALL-AFTER-END, 2026-10-29).
"""
import contextlib, pathlib, sys, types, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as irg


class RebootGivesLibsBack(unittest.TestCase):
    def run_check(self):
        calls = []
        wasabi = mock.Mock()
        wasabi.arexx.return_value = (0, "", "running")
        wasabi.WasabiError = RuntimeError
        tabstate = types.SimpleNamespace(open_host=lambda name: None)
        with mock.patch.object(irg, "rig", lambda *a: calls.append(("rig",) + a)), \
             mock.patch.object(irg.ir, "run", lambda cmd, *a, **k: calls.append(("run", cmd)) or (0, "")), \
             mock.patch.object(irg.ami, "_m", lambda: wasabi), \
             mock.patch.object(irg.ami, "_conn", contextlib.nullcontext), \
             mock.patch.object(irg.ami, "key", lambda *a: calls.append(("key",) + a)), \
             mock.patch.object(irg.amw, "drawn_menu_prefs", lambda: calls.append(("drawn",)) or "saved"), \
             mock.patch.object(irg.amw, "restore_prefs", lambda saved: calls.append(("restore", saved))), \
             mock.patch.object(irg, "amarchy_window", lambda: True), \
             mock.patch.object(irg.time, "sleep", lambda s: None), \
             mock.patch.dict(sys.modules, {"tabstate": tabstate}):
            irg.reboot_amarchy_check(irg.Checks(), [])
        return calls

    def test_the_system_libs_drawer_is_first_again_after_the_reboot(self):
        calls = self.run_check()
        reboot = calls.index(("rig", "reboot"))
        after = [c[1] for c in calls[reboot + 1:] if c[0] == "run"]
        self.assertIn("Assign LIBS: %sLibs" % irg.ir.SYSVOL, after)
        first = after.index("Assign LIBS: %sLibs" % irg.ir.SYSVOL)
        self.assertTrue(any(c.startswith("Assign LIBS: VTC:pkgs/ncurses") and c.endswith(" ADD") for c in after[first + 1:]))

    def test_left_amiga_space_over_the_terminal_opens_the_drawn_menu(self):
        """Over f1host's terminal the default menu-in-terminal = text opened amarchy-menu, no window titled
        Amarchy (fresh-all check 32, rig 9 2026-10-29): the check sets graphical before the keys, restores after."""
        calls = self.run_check()
        space = calls.index(("key", 0x40, 0x40))
        self.assertIn(("drawn",), calls[:space])
        self.assertIn(("restore", "saved"), calls[space:])


if __name__ == "__main__":
    unittest.main()
