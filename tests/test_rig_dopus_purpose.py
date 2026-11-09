"""RIG-DOPUS-PURPOSE: `rig.py dopus --purpose "<who: what>"` puts the caller's purpose in the install rig's window title.

dopus_rig.install() starts the rig with its own `rig.py start --purpose`; that text was a hard-coded constant
("vtcon-05: DOpus 5.104 install into the rig's VTC:"), so every install window named an old session whatever the
caller passed (rig 96, 2026-11-07). The purpose given on the dopus command line now wins; the constant is the fallback.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import dopus_rig as d


class Purpose(unittest.TestCase):
    def test_caller_purpose_wins(self):
        with mock.patch.object(d.rig, "PURPOSE", "up-term-49: rig 96 DOpus reinstall"):
            self.assertEqual(d.purpose(), "up-term-49: rig 96 DOpus reinstall")

    def test_fallback_when_none_given(self):
        with mock.patch.object(d.rig, "PURPOSE", None):
            self.assertEqual(d.purpose(), d.PURPOSE)


class StaleBase(unittest.TestCase):
    """`rig.py dopus --allow-stale-base "<reason>"` reaches the `rig.py start` the install boots with (it was dropped)."""
    def cmd(self, argv):
        with mock.patch.object(d.sys, "argv", ["rig.py", "dopus"] + argv), mock.patch.object(d, "system_args", return_value=[]):
            return d.start_cmd()

    def test_reason_is_forwarded(self):
        c = self.cmd(["--allow-stale-base", "other agent owns the merge"])
        i = c.index("--allow-stale-base")
        self.assertEqual(c[i + 1], "other agent owns the merge")
        self.assertEqual(c[c.index("start") - 1].endswith("rig.py"), True)

    def test_absent_flag_adds_nothing(self):
        self.assertNotIn("--allow-stale-base", self.cmd(["--force"]))


if __name__ == "__main__":
    unittest.main()
