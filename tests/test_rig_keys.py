#!/usr/bin/env python3
"""rig.HOST_KEYS: some Mac key is the Amiga's Left-Amiga (Amarchy's Super, "rawkey lcommand"), and it is not
a Cmd key (macOS takes Cmd+Space for Spotlight). Before 2026-10-23 both Cmd keys were Right-Amiga and no key
gave Left-Amiga, so no Amarchy binding could be pressed on a rig."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


def mapped(action):
    return [l.split("=")[0].strip() for l in rig.HOST_KEYS if l.split("=")[-1].strip() == action]


class HostKeys(unittest.TestCase):
    def test_amarchy_super_key_is_reachable_without_cmd(self):
        keys = mapped("action_key_amiga_left")
        self.assertEqual(keys, ["keyboard_key_ralt"])

    def test_cmd_keys_stay_right_amiga_for_copy_and_paste(self):
        self.assertEqual(sorted(mapped("action_key_amiga_right")),
                         ["keyboard_key_lsuper", "keyboard_key_rsuper"])

if __name__ == "__main__":
    unittest.main()
