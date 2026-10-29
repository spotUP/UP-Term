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
    def test_the_system_libs_drawer_is_first_again_after_the_reboot(self):
        calls = []
        wasabi = mock.Mock()
        wasabi.arexx.return_value = (0, "", "running")
        wasabi.WasabiError = RuntimeError
        tabstate = types.SimpleNamespace(open_host=lambda name: None)
        with mock.patch.object(irg, "rig", lambda *a: calls.append(("rig",) + a)), \
             mock.patch.object(irg.ir, "run", lambda cmd, *a, **k: calls.append(("run", cmd)) or (0, "")), \
             mock.patch.object(irg.ami, "_m", lambda: wasabi), \
             mock.patch.object(irg.ami, "_conn", contextlib.nullcontext), \
             mock.patch.object(irg.ami, "key", lambda *a: None), \
             mock.patch.object(irg, "amarchy_window", lambda: True), \
             mock.patch.object(irg.time, "sleep", lambda s: None), \
             mock.patch.dict(sys.modules, {"tabstate": tabstate}):
            irg.reboot_amarchy_check(irg.Checks(), [])
        reboot = calls.index(("rig", "reboot"))
        after = [c[1] for c in calls[reboot + 1:] if c[0] == "run"]
        self.assertIn("Assign LIBS: %sLibs" % irg.ir.SYSVOL, after)
        first = after.index("Assign LIBS: %sLibs" % irg.ir.SYSVOL)
        self.assertTrue(any(c.startswith("Assign LIBS: VTC:pkgs/ncurses") and c.endswith(" ADD") for c in after[first + 1:]))


if __name__ == "__main__":
    unittest.main()
