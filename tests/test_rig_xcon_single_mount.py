#!/usr/bin/env python3
"""RIG-XCON-DOUBLE-MOUNT: on an installed rig the Startup-Sequence mounts DEVS:DOSDrivers/XCON, then the rig's
S:User-Startup block (BOOTX:shadows-up) ran `C:Mount XCON: FROM BOOTX:Mountlist` unconditionally, so the boot printed
"device XCON: is already mounted" before the Workbench opened (rig 96, owner, 2026-11-05) and boot.log still said
"XCON mounted". Invariant: XCON: is mounted once per boot, and a failed Mount is logged as a failure."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


def mount_branches(script):
    """(lines inside the DOSDrivers branch, lines inside the Else branch) of the XCON mount guard."""
    lines = [l.strip() for l in script.splitlines()]
    start = lines.index("If EXISTS DEVS:DOSDrivers/XCON")
    els = lines.index("Else", start)
    depth, end = 0, None
    for i in range(els + 1, len(lines)):
        if lines[i].startswith("If "):
            depth += 1
        elif lines[i] == "EndIf":
            if depth == 0:
                end = i
                break
            depth -= 1
    return lines[start + 1:els], lines[els + 1:end]


class XconMountedOnce(unittest.TestCase):
    def test_installed_rig_does_not_mount_xcon_a_second_time(self):
        installed, plain = mount_branches(rig.SHADOWS_UP_SCRIPT)
        self.assertFalse([l for l in installed if "Mount" in l and "Echo" not in l])
        self.assertEqual(1, sum("Mount" in l and "XCON:" in l and not l.startswith("Echo") for l in plain))

    def test_every_mount_in_the_script_is_guarded(self):
        mounts = [l for l in rig.SHADOWS_UP_SCRIPT.splitlines() if "Mount" in l and not l.strip().startswith(("Echo", ";"))]
        self.assertEqual(1, len(mounts))
        _, plain = mount_branches(rig.SHADOWS_UP_SCRIPT)
        self.assertIn(mounts[0].strip(), plain)

    def test_a_failed_mount_is_logged_as_failed(self):
        _, plain = mount_branches(rig.SHADOWS_UP_SCRIPT)
        self.assertIn("If WARN", plain)
        self.assertIn('Echo >>BOOTX:boot.log "XCON mount FAILED"', plain)


if __name__ == "__main__":
    unittest.main()
