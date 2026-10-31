"""amarchy_menu_rig on_dopus_screen: the dopus_present row checks that Open UP-Term here lands on DOpus's own screen (D7).
A window on the Workbench screen while DOpus owns another must read False; one beside a DOpus window reads True."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import amarchy_menu_rig as m


def win(title, task=""):
    return {"title": title, "task": task}


class OnDopusScreen(unittest.TestCase):
    dopus = {"windows": [win("Ram Disk", "dopus_lister"), win("", "DOpus5:DirectoryOpus")]}

    def test_on_the_dopus_screen(self):
        scr = [{"windows": self.dopus["windows"] + [win("UP-Term", "up-term")]}, {"windows": [win("Amarchy bar", "Amarchy")]}]
        self.assertTrue(m.on_dopus_screen(scr, "UP-Term"))

    def test_on_the_workbench_screen(self):
        scr = [self.dopus, {"windows": [win("UP-Term", "up-term")]}]
        self.assertFalse(m.on_dopus_screen(scr, "UP-Term"))

    def test_missing(self):
        self.assertFalse(m.on_dopus_screen([self.dopus], "UP-Term"))


if __name__ == "__main__":
    unittest.main()
