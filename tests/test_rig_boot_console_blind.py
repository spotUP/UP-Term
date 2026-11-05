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

    def test_startup_sequence_output_goes_to_boot_console(self):
        # the sentinel: the line that runs the system's Startup-Sequence redirects into the host-read file
        line = next(l for l in rig.STARTUP.splitlines() if "S/Startup-Sequence" in l and "Execute" in l)
        self.assertIn(">BOOTX:boot.console", line)
        self.assertNotIn("NIL:", line)


if __name__ == "__main__":
    unittest.main()
