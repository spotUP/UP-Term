#!/usr/bin/env python3
"""K36-OFF-DIFF: kitty_anim_rig off-mode diffed the whole screen and failed on pointer/repaint residue outside the
XCON window (777 px on AGA, 8 px on RTG) with PROF switches = 0. The diff covers the window rectangle only."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import kitty_anim_rig as k

W, H = 640, 256


def shot(*dots):
    px = bytearray(W * H * 3)
    for x, y in dots:
        i = (y * W + x) * 3
        px[i:i + 3] = b'\xff\xff\xff'
    return bytes(px)


def off_pass(shots):
    return all(d == 0 for d in k.window_diffs(shots, W, H)[1])


class WindowDiff(unittest.TestCase):
    def test_residue_outside_window_passes_off_mode(self):
        shots = [shot(), shot((500, 200)), shot((k.WX + k.WW + 5, 100), (3, 5))]
        self.assertTrue(off_pass(shots))
        whole = [sum(1 for i in range(0, len(s) - 2, 3) if s[i:i + 3] != shots[0][i:i + 3]) for s in shots[1:]]
        self.assertTrue(any(whole))  # the old whole-screen diff would have failed here

    def test_change_inside_window_fails_off_mode(self):
        shots = [shot(), shot(), shot((k.WX + 10, k.WY + 10))]
        self.assertFalse(off_pass(shots))
        self.assertEqual(k.window_diffs(shots, W, H)[0], [0, 1])


if __name__ == '__main__':
    unittest.main()
