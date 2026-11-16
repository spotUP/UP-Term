"""rig.stale_names: the files of build/amiga the rig's UP-Term:bin holds in another version (not the ones it lacks)."""
import pathlib, sys, tempfile, unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import rig


class StaleNames(unittest.TestCase):
    def test_differs_listed_missing_and_equal_not(self):
        d = pathlib.Path(tempfile.mkdtemp())
        for n, b in (("a", b"1"), ("b", b"2"), ("c", b"3")):
            (d / n).write_bytes(b)
        held = {"a": b"1", "b": b"old"}
        self.assertEqual(rig.stale_names(d, held.get), ["b"])


if __name__ == "__main__":
    unittest.main()
