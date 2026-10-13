#!/usr/bin/env python3
"""The rig's amiagent (tools/rig/agentpatch.py), on the host: the vendored 0.13.2 68020 build is
the one the rig installs, its --stock patch changes exactly the two NP_StackSize 262144 values
(offsets 0x744 and 0x13e88) and nothing else, and any other build -- the 68000 one, 0.13.0 -- is
refused (the old patch took any file with two such places)."""
import hashlib, pathlib, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import agentpatch as ap

D68K = ROOT / "dist/net/amiagent-0.13.2/amiagent"
D020 = ROOT / "dist/net/amiagent-0.13.2/amiagent.020"


class StockPatch(unittest.TestCase):
    def test_the_rig_installs_the_vendored_020_build(self):
        self.assertEqual(ap.SOURCE, D020)
        with tempfile.TemporaryDirectory() as t:
            dest = pathlib.Path(t) / "amiagent"
            dest.write_bytes(b"an old 68000 agent")
            ap.install(dest)
            self.assertEqual(dest.read_bytes(), D020.read_bytes())
            self.assertTrue(dest.stat().st_mode & 0o100)

    def test_fingerprint_and_offsets_of_the_020_build(self):
        data = D020.read_bytes()
        self.assertEqual(len(data), 82636)
        self.assertEqual(hashlib.sha256(data).hexdigest(), "de6ad5acd4ee88930bda25ce865773c08583d74639f88447d43b688bce04c0ac")
        self.assertEqual(ap.stack_offsets(data), [0x744, 0x13e88])
        # the first is move.l #262144 after move.l #tag, the second a TagItem
        self.assertEqual(data[0x744 - 6:0x744 + 4], bytes.fromhex("800003f320fc00040000"))
        self.assertEqual(data[0x13e88 - 4:0x13e88 + 4], bytes.fromhex("800003f300040000"))

    def test_patch_changes_exactly_the_two_values(self):
        data = D020.read_bytes()
        out = ap.stock_patch(data)
        diff = [i for i in range(len(data)) if data[i] != out[i]]
        self.assertEqual(len(out), len(data))
        self.assertEqual(diff, [0x744 + 1, 0x744 + 2, 0x13e88 + 1, 0x13e88 + 2])   # 0x00040000 -> 0x00004000
        for off in (0x744, 0x13e88):
            self.assertEqual(out[off:off + 4], (16384).to_bytes(4, "big"))

    def test_another_build_is_refused(self):
        for p in (D68K,):
            with self.assertRaises(SystemExit):
                ap.stock_patch(p.read_bytes())       # two places too, other offsets: the old rule took it
        with self.assertRaises(SystemExit):
            ap.stock_patch(D020.read_bytes()[:-1])
        moved = bytearray(D020.read_bytes()); moved[0x744:0x748] = b"\x00\x00\x40\x00"
        with self.assertRaises(SystemExit):
            ap.stock_patch(bytes(moved))             # right size, wrong fingerprint


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
