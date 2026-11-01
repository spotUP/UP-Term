#!/usr/bin/env python3
"""RIG-EMU-DEFAULT (owner 2026-11-02): Amiberry is the default rig emulator. No flag = amiberry, --emu fsuae = fsuae,
the emulator a rig recorded in <rig>/emu wins for stop/status/reboot, and the FS-UAE-only flags and scripts exit with
the Amiberry-gap message. Each case runs paths.py (or rig.py) in a fresh interpreter: the emulator is read from argv."""
import os, pathlib, subprocess, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
RIGDIR = ROOT / "tools/rig"
GAP = "needs FS-UAE: add --emu fsuae (Amiberry gap, AMIBERRY-RIGS-GAPS)"

def emu(argv, env=None, recorded=None):
    """paths.EMU of `python3 paths.py-import` with sys.argv = ['rig.py'] + argv; `recorded` is written to <rig>/emu."""
    e = {k: v for k, v in os.environ.items() if k not in ("UPTERM_RIG_EMU", "UPTERM_RIG")}
    e.update(env or {})
    with tempfile.TemporaryDirectory() as t:
        e["UPTERM_RIG"] = "99"
        code = ("import sys; sys.argv = %r; sys.path.insert(0, %r); import paths\n"
                "if %r is not None:\n"
                "    paths.EMU_FILE.parent.mkdir(parents=True, exist_ok=True)\n"
                "print(paths.EMU)") % (["rig.py"] + argv, str(RIGDIR), recorded)
        return subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True, env=e, cwd=t)

class EmuDefault(unittest.TestCase):
    def test_no_flag_is_amiberry(self):
        self.assertEqual(emu(["start", "--max"]).stdout.strip(), "amiberry")

    def test_emu_flag_fsuae(self):
        self.assertEqual(emu(["start", "--emu", "fsuae"]).stdout.strip(), "fsuae")

    def test_env_fsuae(self):
        self.assertEqual(emu(["setup"], env={"UPTERM_RIG_EMU": "fsuae"}).stdout.strip(), "fsuae")

    def test_recorded_emu_wins_for_stop_and_status(self):
        code = ("import sys, pathlib; sys.path.insert(0, %r); import paths\n"
                "f = pathlib.Path(sys.argv[1]); f.write_text('fsuae\\n')\n"
                "for c in ('stop', 'status', 'reboot', 'start', 'setup'):\n"
                "    print(c, paths.resolve_emu(['rig.py', c], {}, f))\n"
                "print('flag', paths.resolve_emu(['rig.py', 'stop', '--emu', 'amiberry'], {}, f))") % str(RIGDIR)
        with tempfile.TemporaryDirectory() as t:
            out = subprocess.run([sys.executable, "-I", "-c", code, str(pathlib.Path(t) / "emu")], capture_output=True, text=True,
                                 env={**os.environ, "UPTERM_RIG": "99"}).stdout.split("\n")
        self.assertEqual(out[:6], ["stop fsuae", "status fsuae", "reboot fsuae", "start amiberry", "setup amiberry", "flag amiberry"])

    def test_fsuae_only_flags_exit_with_message(self):
        code = ("import sys; sys.argv = ['rig.py', 'start', '%s']; sys.path.insert(0, %r); import paths; paths.gate_fsuae_flags(sys.argv)")
        for flag in ("--debugger", "--a500", "--stock"):
            p = subprocess.run([sys.executable, "-I", "-c", code % (flag, str(RIGDIR))], capture_output=True, text=True,
                               env={**os.environ, "UPTERM_RIG": "99"})
            self.assertEqual(p.returncode, 1, flag)
            self.assertIn("rig: %s %s" % (flag, GAP), p.stderr)

    def test_fsuae_only_flags_pass_on_fsuae(self):
        code = "import sys; sys.argv = ['rig.py', 'start', '--debugger', '--emu', 'fsuae']; sys.path.insert(0, %r); import paths; paths.gate_fsuae_flags(sys.argv)" % str(RIGDIR)
        p = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True, env={**os.environ, "UPTERM_RIG": "99"})
        self.assertEqual(p.returncode, 0, p.stderr)

    def test_rig_py_start_debugger_exits_through_the_entry_point(self):
        """The product entry point: rig.py start --debugger refuses before anything is written."""
        p = subprocess.run([sys.executable, "-I", str(RIGDIR / "rig.py"), "start", "--debugger"], capture_output=True, text=True,
                           env={**os.environ, "UPTERM_RIG": "99"}, cwd=ROOT)
        self.assertEqual(p.returncode, 1)
        self.assertIn("rig: --debugger " + GAP, p.stderr)
        self.assertFalse((ROOT / "build/rig99").exists())

    def test_muforce_and_standalone_exit_with_message(self):
        for code in ("import sys; sys.argv = ['rig.py', 'muforce', 'on']; sys.path.insert(0, %r); import muforce; muforce.main(['on'])",
                     "import sys; sys.argv = ['standalone_rig.py']; sys.path.insert(0, %r); import standalone_rig; standalone_rig.main()"):
            p = subprocess.run([sys.executable, "-I", "-c", code % str(RIGDIR)], capture_output=True, text=True,
                               env={**os.environ, "UPTERM_RIG": "99"}, cwd=ROOT)
            self.assertEqual(p.returncode, 1, p.stderr[-300:])
            self.assertIn(GAP, p.stderr)

    @unittest.skipUnless((ROOT / "build/amiga").is_dir(), "rig.py imports need build/amiga (known worktree gap)")
    def test_reboot_pins_the_rigs_emulator(self):
        sys.path.insert(0, str(RIGDIR))
        import rig
        self.assertEqual(rig.reboot_args(["--max"]), ["--max", "--emu", rig.paths.EMU])
        self.assertEqual(rig.reboot_args(["--emu", "fsuae"]), ["--emu", "fsuae"])


if __name__ == "__main__":
    unittest.main()
