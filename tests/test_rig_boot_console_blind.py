#!/usr/bin/env python3
"""RIG-BOOT-CONSOLE-BLIND: rig starts never read what the boot printed before the Workbench opened, so "device XCON:
is already mounted" (rig 96, owner, 2026-11-05) passed while boot.log said "XCON mounted". Invariant: the
Startup-Sequence output lands in BOOTX:boot.console on every machine, and boot_console_errors finds the error lines."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

CLEAN = "Mounting CD0:\nSetPatch 43.6\nWelcome to the Amiga\n"
XCON = "Mounting XCON:\ndevice XCON: is already mounted\nWorkbench loading\n"


class BootConsole(unittest.TestCase):
    def test_xcon_already_mounted_is_an_error_line(self):
        self.assertEqual(["device XCON: is already mounted"], rig.boot_console_errors(XCON))

    def test_other_amigados_shapes(self):
        for line in ("Foo: object not found", "Unknown command Foo", "DH9: is not a valid DOS disk", "Foo failed returncode 20"):
            self.assertEqual([line], rig.boot_console_errors("ok\n" + line + "\n"), line)

    def test_clean_boot_passes(self):
        self.assertEqual([], rig.boot_console_errors(CLEAN))

    def test_startup_sequence_runs_in_a_cli_whose_output_is_boot_console(self):
        # BOOT-CONSOLE-CAPTURE: `Execute script >file` captures nothing (0 bytes on rig 150, 2026-11-05): Execute only
        # hands the script to the calling shell's input and returns, the shell closes the file then and runs the
        # script's lines with its own output. `Run >file` redirects Run alone. The Startup-Sequence runs through
        # bootcap, which starts a CLI with SYS_Output = the file; no shell redirect on that line.
        line = next(l for l in rig.STARTUP.splitlines() if "S/Startup-Sequence" in l and "Execute" in l)
        self.assertEqual(["BOOTX:bootcap", "BOOTX:boot.console", "C:Execute", "DH0:S/Startup-Sequence"], line.split())
        self.assertTrue((ROOT / "tools/rig/bootcap.c").is_file())

    def test_os_image_startup_sequence_runs_through_bootcap(self):
        # BOOT-CONSOLE-OS: an --os rig boots the image copy's own S:Startup-Sequence; without the wrapper every
        # --os start failed the marker gate (rig 96, 2026-11-05). Idempotent: a second start keeps the original.
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            s = pathlib.Path(d) / "S"
            s.mkdir()
            (s / "Startup-Sequence").write_text("SetPatch QUIET\nLoadWB\nEndCLI >NIL:\n")
            for _ in range(2):
                rig.os_tree_bootcap(d)
                self.assertEqual(["BOOTX:bootcap", "BOOTX:boot.console", "C:Execute", "S:Startup-Sequence.os"],
                                 (s / "Startup-Sequence").read_text().splitlines()[0].split())
                self.assertEqual("SetPatch QUIET\nLoadWB\nEndCLI >NIL:\n", (s / "Startup-Sequence.os").read_text())

    def test_boot_shell_ends_after_the_startup_sequence_so_no_amigados_window_stays_open(self):
        # RIG-BOOT-SHELL-OPEN (rig 146, owner, 2026-11-05): bootcap runs the Startup-Sequence in a child CLI, so the
        # sequence's own EndCLI ends the child only. The boot shell reached the end of its script, went interactive
        # and opened its "AmigaDOS" window on the Workbench; with that window open IPrefs' "reset the Workbench
        # screen" requester stayed and the install reboot failed (RIG-IPREFS-SYSREQ). The boot shell ends itself:
        # the bootcap line is followed by EndCLI and nothing else, in the plain boot script and in the --os wrapper.
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            s = pathlib.Path(d) / "S"
            s.mkdir()
            (s / "Startup-Sequence").write_text("LoadWB\nEndCLI >NIL:\n")
            rig.os_tree_bootcap(d)
            for name, script in (("STARTUP", rig.STARTUP), ("--os S:Startup-Sequence", (s / "Startup-Sequence").read_text())):
                lines = [l.strip() for l in script.splitlines() if l.strip()]
                at = [n for n, l in enumerate(lines) if l.startswith("BOOTX:bootcap ")]
                self.assertEqual(1, len(at), name)
                self.assertEqual(["EndCLI >NIL:"], lines[at[0] + 1:], name)

    def test_marker_tells_capture_from_no_capture(self):
        self.assertTrue(rig.boot_console_captured("boot console open\nSetPatch 43.6\n"))
        self.assertTrue(rig.boot_console_captured("boot console open\n"))   # a clean boot prints nothing else
        self.assertFalse(rig.boot_console_captured(""))                     # the 0-byte file of the Execute redirect
        self.assertFalse(rig.boot_console_captured("[CLI 3]\n"))           # what `Run >file` captured
        self.assertIn("boot console open", open(ROOT / "tools/rig/bootcap.c").read())


if __name__ == "__main__":
    unittest.main()
