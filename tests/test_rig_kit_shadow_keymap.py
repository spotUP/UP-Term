#!/usr/bin/env python3
"""rig.py stages the build's dist/keymap and refreshes the UP-Term drawer's bin (RIG-KEYMAP-STAGE, vtcon-a1 P7,
2026-10-27). Before: every start left the master kit's keymap in ENVARC:/ENV:up-term/keymap, and `rig.py install`
never refreshed VTC:Apps/bin, so the start [WARN] drift line kept listing those programs. Drives rig.main("install")
on a fake ROOT and rig drawer; the boot lines are checked as text."""
import contextlib, io, pathlib, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import paths, rig


class KitShadowKeymap(unittest.TestCase):
    def test_boot_puts_vtc_keymap_over_the_kits_in_envarc_and_env(self):
        go = rig.kit_shadow_lines(False)
        for dest, bak in (("SYS:Prefs/Env-Archive/up-term/keymap", "ENVARC-up-term-keymap"),
                          ("RAM:Env/up-term/keymap", "ENV-up-term-keymap")):
            self.assertIn("C:Copy >NIL: VTC:keymap %s CLONE" % dest, go)
            self.assertIn("C:Copy >NIL: VTC:kit-orig/%s %s CLONE" % (bak, dest), go)   # kit-restore puts the kit's back
        self.assertNotIn("keymap.user", go)

    def test_boot_never_names_env_or_envarc(self):
        # go is Run before DH0:S/Startup-Sequence assigns ENV: and ENVARC:; naming either opened a
        # System Request and hung every boot (rig 8, 2026-10-27)
        go = rig.kit_shadow_lines(False) + rig.kit_shadow_lines(True)
        self.assertNotRegex(go, r"\bENV(ARC)?:")

    def test_install_stages_dist_keymap_and_refreshes_apps_bin(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        root, vtc = tmp / "root", tmp / "rig9/vtc"
        (root / "build/amiga").mkdir(parents=True)
        (root / "dist").mkdir()
        (root / "dist/keymap").write_text("[bind]\nnew.row = A-x\n")
        (root / "build/amiga/vtcon-handler").write_bytes(b"handler-new")
        (root / "build/amiga/md5sum").write_bytes(b"md5sum-new")
        bindir = vtc / paths.UPTERM_DRAWER / "bin"
        bindir.mkdir(parents=True)
        (bindir / "md5sum").write_bytes(b"md5sum-old")
        with mock.patch.object(rig, "ROOT", root), mock.patch.object(rig, "RIG", tmp / "rig9"), \
             mock.patch.object(rig.paths, "RIG_HOME", tmp), mock.patch.object(rig, "stage_upterm", lambda d, v: None), \
             mock.patch.object(rig, "KIT_SHADOWS", [x for x in rig.KIT_SHADOWS if x[0] in ("keymap", "vtcon-handler")]), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(rig.build_drift(vtc), [paths.UPTERM_DRAWER + "/bin/md5sum"])
            self.assertEqual(rig.main(["rig.py", "install"]), 0)
            self.assertEqual(rig.build_drift(vtc), [])
        self.assertEqual((vtc / "keymap").read_bytes(), (root / "dist/keymap").read_bytes())
        self.assertEqual((bindir / "md5sum").read_bytes(), b"md5sum-new")

    def test_install_adds_upterm_a_rig_vtc_lacks(self):
        # typewin.quiet_screen_text runs VTC:UPTerm; a VTC: without it made every screen_text() Select all + Copy
        tmp = pathlib.Path(tempfile.mkdtemp())
        root, vtc = tmp / "root", tmp / "rig9/vtc"
        (root / "build/amiga").mkdir(parents=True)
        (root / "dist").mkdir()
        (root / "build/amiga/vtcon-handler").write_bytes(b"handler-new")
        (root / "build/amiga/UPTerm").write_bytes(b"upterm-new")
        (vtc / paths.UPTERM_DRAWER / "bin").mkdir(parents=True)
        with mock.patch.object(rig, "ROOT", root), mock.patch.object(rig, "RIG", tmp / "rig9"), \
             mock.patch.object(rig.paths, "RIG_HOME", tmp), mock.patch.object(rig, "stage_upterm", lambda d, v: None), \
             mock.patch.object(rig, "KIT_SHADOWS", [x for x in rig.KIT_SHADOWS if x[0] in ("keymap", "vtcon-handler")]), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(rig.main(["rig.py", "install"]), 0)
        self.assertEqual((vtc / "UPTerm").read_bytes(), b"upterm-new")
        self.assertFalse((vtc / paths.UPTERM_DRAWER / "bin" / "UPTerm").exists())   # only VTC: is required

    def test_boot_shadows_every_amarchy_program_the_kit_installs(self):
        # RIG-KIT-SHADOWS: no rows for them left stale kit copies that the boot's User-Startup Run line started.
        # The set is the one dist/install.dos copies into UP-Term:bin, read from that file.
        dos = (ROOT / "dist/install.dos").read_text()
        names = next(l.split()[1:-2] for l in dos.splitlines() if l.startswith("Copy Amarchy "))
        self.assertEqual(sorted(names), ["AmTile", "Amarchy", "amarchy-gui", "amarchy-index", "amarchy-menu"])
        up = rig.kit_shadow_lines(True)
        for n in names:
            self.assertIn("C:Copy >NIL: VTC:%s UP-Term:bin/%s CLONE" % (n, n), up)
            self.assertIn("C:Copy >NIL: VTC:kit-orig/UP-Term-bin-%s UP-Term:bin/%s CLONE" % (n, n), up)
            self.assertEqual(rig.kit_shadow_src(n), rig.build_amiga() / n)


if __name__ == "__main__":
    unittest.main()
