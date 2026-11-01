"""A warm rig reboot skips the kit shadow pass while DH0: already holds this kit (RIG-SLOW-BOOT, 2026-10-29).

Before: BOOTX:go ran every KIT_SHADOWS row on every boot, about 1400 UAE host-filesystem round trips on rig 9,
~10 minutes of a warm reboot under host load. Now the host writes VTC:kit-hash (kitstamp), go copies it to
VTC:kit-shadowed after a full pass and skips the pass while that stamp exists; the host drops the stamp when the
kit, go or DH0: changed, so a changed kit still shadows on the next boot.
"""
import pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import kitstamp
import rig


class Stamp(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.boot, self.vtc = self.d / "boot", self.d / "vtc"
        self.boot.mkdir()
        self.vtc.mkdir()
        (self.boot / "go").write_text("go v1\n")
        (self.boot / kitstamp.LIST).write_text("vsh\nhl\n")
        (self.vtc / "vsh").write_bytes(b"vsh 1")

    def full_pass(self):
        # what go does after its shadow pass: copy VTC:kit-hash to VTC:kit-shadowed
        shutil.copy(self.vtc / kitstamp.HASH, self.vtc / kitstamp.STAMP)

    def test_unchanged_kit_skips_the_next_pass(self):
        self.assertFalse(kitstamp.check(self.boot, self.vtc))
        self.full_pass()
        self.assertTrue(kitstamp.check(self.boot, self.vtc))
        self.assertTrue((self.vtc / kitstamp.STAMP).exists())

    def test_changed_kit_file_shadows_again(self):
        kitstamp.check(self.boot, self.vtc)
        self.full_pass()
        (self.vtc / "vsh").write_bytes(b"vsh 2")
        self.assertFalse(kitstamp.check(self.boot, self.vtc))
        self.assertFalse((self.vtc / kitstamp.STAMP).exists())

    def test_a_listed_file_appearing_shadows_again(self):
        kitstamp.check(self.boot, self.vtc)
        self.full_pass()
        (self.vtc / "hl").write_bytes(b"")
        self.assertFalse(kitstamp.check(self.boot, self.vtc))

    def test_changed_go_shadows_again(self):
        kitstamp.check(self.boot, self.vtc)
        self.full_pass()
        (self.boot / "go").write_text("go v2\n")
        self.assertFalse(kitstamp.check(self.boot, self.vtc))

    def test_no_list_drops_the_stamp(self):
        kitstamp.check(self.boot, self.vtc)
        self.full_pass()
        (self.boot / kitstamp.LIST).unlink()
        self.assertFalse(kitstamp.check(self.boot, self.vtc))
        self.assertFalse((self.vtc / kitstamp.STAMP).exists())


class GoScript(unittest.TestCase):
    def lines(self):
        return [l.strip() for l in rig.GO.splitlines()]

    def test_shadow_pass_sits_behind_the_stamp(self):
        g = self.lines()
        skip = g.index("If EXISTS VTC:%s" % kitstamp.STAMP)
        self.assertEqual(g[skip + 1], 'Echo >>BOOTX:boot.log "kit shadows skipped"')
        self.assertEqual(g[skip + 2], "Else")
        self.assertIn("C:Copy >NIL: VTC:vsh C:vsh CLONE", g[skip:])

    def test_restore_branch_drops_the_stamp(self):
        g = self.lines()
        restore = g.index("If EXISTS VTC:kit-restore")
        self.assertIn("C:Delete >NIL: VTC:%s QUIET" % kitstamp.STAMP, g[restore:g.index("Else", restore)])

    def test_stamp_written_after_both_passes(self):
        g = self.lines()
        write = g.index("C:Copy >NIL: VTC:%s VTC:%s" % (kitstamp.HASH, kitstamp.STAMP))
        shadow_copies = {"C:Copy >NIL: VTC:%s %s CLONE" % (src, rig.q(dest)) for src, dest, _ in rig.KIT_SHADOWS}
        self.assertLess(max(i for i, l in enumerate(g) if l in shadow_copies), write)
        self.assertLess(write, g.index('Echo >>BOOTX:boot.log "UP-Term shadows done"'))


class HostDropsTheStamp(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        (self.d / "vtc").mkdir()
        (self.d / "vtc" / kitstamp.STAMP).write_text("x\n")
        p = mock.patch.object(rig, "RIG", self.d)
        p.start()
        self.addCleanup(p.stop)

    def test_kit_restore(self):
        rig.kit_restore()
        self.assertFalse((self.d / "vtc" / kitstamp.STAMP).exists())

    def test_fresh_sys_hdf(self):
        def cp(cmd, **k):
            pathlib.Path(cmd[-1]).write_bytes(b"hdf")
            return subprocess.CompletedProcess(cmd, 0, "", "")
        with mock.patch.object(rig.paths, "require_input"), mock.patch.object(rig.subprocess, "run", cp):
            rig.fresh_sys_hdf()
        self.assertFalse((self.d / "vtc" / kitstamp.STAMP).exists())


if __name__ == "__main__":
    unittest.main()
