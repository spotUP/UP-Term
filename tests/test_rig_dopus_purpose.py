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


if __name__ == "__main__":
    unittest.main()
