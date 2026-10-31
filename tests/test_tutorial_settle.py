#!/usr/bin/env python3
"""tutorial_rig.settled() waits for the lesson to stop printing, and the step 5 retry re-reads candidate 0 after a failed
copy (s35 rig 4: settled() returned while the lesson still printed; the retry moved to skip=1 on a changed screen and
dragged "Return:"). Fake snapshots drive both; no rig needed."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import tutorial_rig as t


class Fake:
    def __init__(self, rows, bottom):
        self.rows, self.b = rows, bottom
    def task_rows(self):
        return self.rows
    def bottom(self):
        return self.b


class Settled(unittest.TestCase):
    def run_seq(self, seq, want=None):
        it = iter(seq)
        n = []
        def snap():
            n.append(1)
            return next(it)
        sc = t.settled(secs=30, want=want, snap=snap, pause=0)
        return sc, len(n)

    def test_waits_for_bottom_to_stop_moving(self):
        rows = [(88, 11)]
        seq = [Fake(rows, 100), Fake(rows, 100), Fake(rows, 120), Fake(rows, 130), Fake(rows, 130), Fake(rows, 130)]
        sc, n = self.run_seq(seq)
        self.assertEqual((sc.b, n), (130, 6))     # two matching snapshots with a moving bottom used to return at 2

    def test_waits_for_expected_word(self):
        seq = [Fake([(88, 40)], 100)] * 4 + [Fake([(88, 11)], 100)] * 3
        sc, n = self.run_seq(seq, want=t.up_word_ready)
        self.assertEqual((sc.task_rows(), n), ([(88, 11)], 7))


class Retry(unittest.TestCase):
    def test_changed_screen_rereads_candidate_zero(self):
        tried = set()
        a = [(216, 11), (200, 70)]
        self.assertEqual(t.next_row(a, None, tried), 0)
        b = [(224, 11), (208, 70)]                 # the screen moved after the failed copy
        self.assertEqual(t.next_row(b, a, tried), 0)

    def test_same_screen_skips_tried(self):
        tried = set()
        a = [(216, 11), (200, 70)]
        self.assertEqual(t.next_row(a, None, tried), 0)
        self.assertEqual(t.next_row(a, a, tried), 1)
        self.assertIsNone(t.next_row(a, a, tried))


if __name__ == "__main__":
    unittest.main()
