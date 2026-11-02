"""The rig boot gives wasabid a real stack before it Runs it (RIG-AGA-WASABID-STACK, 2026-11-02).

BOOTX:go started wasabid with `C:Run`, which hands the program the boot shell's stack: 4096 bytes (AmigaDOS
Status on rig 80: `Process 6: stk 4096, gv 150, pri 1 Loaded as command: BOOTX:wasabid`). After `rig.py aga` the
AGA boot reached the BOOTX: prompt but wasabid never answered (TCP connect accepted by the host, no reply; errno 60
on the client), while the machine itself kept running. The same AGA boot with `C:Stack 262144` before the Run
answered ping (`wasabid 0.11b2`). The test reads the generated BOOTX:go text, the file the rig boots from.
"""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig


class WasabidStack(unittest.TestCase):
    def test_stack_set_before_wasabid_run(self):
        lines = rig.GO.splitlines()
        run = [i for i, l in enumerate(lines) if re.match(r"\s*C:Run\b.*BOOTX:wasabid\b", l)]
        self.assertEqual(len(run), 1, "BOOTX:go runs wasabid exactly once")
        stacks = [int(m.group(1)) for l in lines[:run[0]] for m in [re.match(r"\s*C:Stack\s+(\d+)", l)] if m]
        self.assertTrue(stacks, "no C:Stack before wasabid's Run: it gets the boot shell's 4096 bytes")
        self.assertGreaterEqual(stacks[-1], 65536)


if __name__ == "__main__":
    unittest.main()
