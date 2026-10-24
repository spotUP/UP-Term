#!/usr/bin/env python3
"""tools/rig/wbrun_rig.wait_window on the host (no emulator). AmigaOS frees a closed window's memory at
once, so a window opened right after another one closed can get its address: the Amarchy menu's window
closes on Return and the Clock window opens at the same address. A newness test on the address alone
missed the Clock (rig case app_index check 8 failed in about half the runs, and left a Clock open each time)."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import wbrun_rig


def win(addr, task, title):
    return {"addr": addr, "task": task, "title": title}


class AddressReuse(unittest.TestCase):
    def setUp(self):
        self.orig = wbrun_rig.windows
        self.addCleanup(setattr, wbrun_rig, "windows", self.orig)

    def test_a_window_at_the_address_of_a_closed_one_is_new(self):
        menu = win("0x1039cff4", "XCON", "menuhost")
        wbrun_rig.windows = lambda: [menu]
        before = wbrun_rig.window_ids()
        clock = win("0x1039cff4", "Clock", "Clock")
        wbrun_rig.windows = lambda: [clock]
        found = wbrun_rig.wait_window(before, lambda w: w.get("task") == "Clock", 2)
        self.assertEqual(found, clock)

    def test_a_window_that_was_already_open_is_not_new(self):
        old = win("0x1032f944", "Clock", "Clock")
        wbrun_rig.windows = lambda: [old]
        before = wbrun_rig.window_ids()
        self.assertIsNone(wbrun_rig.wait_window(before, lambda w: w.get("task") == "Clock", 1.2))


if __name__ == "__main__":
    unittest.main()
