#!/usr/bin/env python3
"""B4: TERM describes the window. vtcon declares bce; the AMIGA personality does
not erase to the program's background, so its entry vtcon-amiga is vtcon
without bce. The kit's terminfo (Makefile KIT_TERMINFO) compiles both."""
import os, pathlib, subprocess, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]


class TerminfoAmiga(unittest.TestCase):
    def test_vtcon_amiga_compiles_and_has_no_bce(self):
        with tempfile.TemporaryDirectory() as d:
            env = dict(os.environ, TERMINFO=d)
            for f in ("vtcon", "vtcon-amiga"):
                subprocess.run(["tic", "-x", "-o", d, str(ROOT / "terminfo" / (f + ".terminfo"))],
                               check=True, env=env)
            def caps(n):
                return subprocess.run(["infocmp", "-x", "-1", n], check=True, env=env,
                                      capture_output=True, text=True).stdout
            self.assertIn("\tbce,", caps("vtcon"))
            amiga = caps("vtcon-amiga")
            self.assertNotIn("bce", amiga)
            self.assertIn("pairs#0x7fff", amiga)  # the rest is vtcon's

    def test_vtcon_amiga_has_no_xenl_because_an_amiga_window_wraps_at_once(self):
        # W-N1: nvim in an AMIGA window drew every second row in the window's
        # background. The window wraps at once (console.device): a full row
        # followed by CR LF moves two rows. Measured on the engine, then the entry.
        out = subprocess.run([str(ROOT / "build" / "vtdump"), "4", "3", "amiga"],
                             input=b"abcd\r\nX", check=True, capture_output=True).stdout.decode()
        self.assertEqual(out.splitlines()[:3], ["abcd", "    ", "X   "])
        with tempfile.TemporaryDirectory() as d:
            env = dict(os.environ, TERMINFO=d)
            for f in ("vtcon", "vtcon-amiga"):
                subprocess.run(["tic", "-x", "-o", d, str(ROOT / "terminfo" / (f + ".terminfo"))],
                               check=True, env=env)
            def caps(n):
                return subprocess.run(["infocmp", "-x", "-1", n], check=True, env=env,
                                      capture_output=True, text=True).stdout.split()
            self.assertIn("xenl,", caps("vtcon"))
            amiga = caps("vtcon-amiga")
            self.assertNotIn("xenl,", amiga)
            self.assertIn("am,", amiga)

    def test_kit_builds_both(self):
        mk = (ROOT / "Makefile").read_text()
        self.assertIn("terminfo/vtcon-amiga.terminfo", mk.split("KIT_TERMINFO :=")[1].splitlines()[0])


if __name__ == "__main__":
    unittest.main()
