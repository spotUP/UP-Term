#!/usr/bin/env python3
"""AMI-ASSIGN-REMOVES: ami.py refuses a command line that removes an assign (`Assign NAME:`, `Assign NAME: LIST`,
`Assign NAME: REMOVE`) on every send path, unless --allow-assign-remove carries a reason. Transport stubbed."""
import os, pathlib, struct, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import ami

DANGER = ["Assign LIBS:", "assign libs: list", "ASSIGN LIBS: REMOVE", "C:Assign LIBS:", "Echo hi ; Assign LIBS: LIST",
          "Echo a\nAssign LIBS:\nEcho b", "Assign >NIL: LIBS:", "Assign LIBS: DISMOUNT"]
SAFE = ["Assign", "Assign LIBS: EXISTS", "Assign LIBS: SYS:Libs", "Assign LIBS: SYS:Libs ADD", "Assign LIBS: SYS:Libs PATH",
        "Assign LIBS: SYS:Libs DEFER", "Assign LIBS: SYS:Libs REMOVE", "Assign LIST", "Echo Assign LIBS:", "List LIBS: QUICK"]

class AssignGuard(unittest.TestCase):
    def setUp(self):
        os.environ.pop("AMI_ALLOW_ASSIGN_REMOVE", None)
        self.sent = []
        self._conn, self._gate = ami._conn, ami.gate
        ami._conn = lambda *a, **k: self.sent.append("connect") or (_ for _ in ()).throw(AssertionError("network touched"))
        ami.gate = lambda: None

    def tearDown(self):
        ami._conn, ami.gate = self._conn, self._gate
        os.environ.pop("AMI_ALLOW_ASSIGN_REMOVE", None)

    def test_danger_lines(self):
        for c in DANGER: self.assertIsNotNone(ami.assign_danger(c), c)

    def test_safe_lines(self):
        for c in SAFE: self.assertIsNone(ami.assign_danger(c), c)

    def test_offending_line_returned(self):
        self.assertEqual(ami.assign_danger("Echo a\nAssign LIBS:\nEcho b"), "Assign LIBS:")

    def test_exec_path_refused_before_network(self):
        with self.assertRaises(SystemExit) as e:
            ami.req(0x02, struct.pack(">H", 5) + b"Assign LIBS: LIST")
        self.assertIn("AMI-ASSIGN-REMOVES", str(e.exception))
        self.assertIn("Assign LIBS: LIST", str(e.exception))
        self.assertEqual(self.sent, [])

    def test_type_and_put_paths_refused(self):
        with self.assertRaises(SystemExit): ami.req(0x08, bytes([4]) + b"Assign LIBS:\r")
        name = b"RAM:x"
        with self.assertRaises(SystemExit): ami.req(0x04, struct.pack(">H", len(name)) + name + b"Echo x\nAssign LIBS:\n")
        with tempfile.NamedTemporaryFile("w", suffix=".script") as f:
            f.write("Assign LIBS: REMOVE\n"); f.flush()
            with self.assertRaises(SystemExit) as e: ami.put(f.name, "RAM:y")
        self.assertIn("AMI-ASSIGN-REMOVES", str(e.exception))
        self.assertEqual(self.sent, [])

    def test_override_needs_reason(self):
        with self.assertRaises(SystemExit) as e: ami.take_allow_assign_remove(["exec", "x", "--allow-assign-remove", " "])
        self.assertIn("non-empty reason", str(e.exception))
        self.assertEqual(ami.take_allow_assign_remove(["exec", "x", "--allow-assign-remove", "rebuild LIBS"]), ["exec", "x"])
        ami.guard_assign("Assign LIBS:")   # reason set: allowed, no raise

if __name__ == "__main__":
    unittest.main()
