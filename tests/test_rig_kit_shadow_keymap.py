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

    def test_up_term_shadows_run_before_the_amarchy_block_and_go_skips_then(self):
        # RIG-BOOT-STALE-BIN: Amarchy (User-Startup) started UP-Term:bin programs before go's shadow Copy
        kit = (b"; head\n;BEGIN UP-Term assign\nAssign UP-Term: Work:UP\n;END UP-Term assign\n"
               b";BEGIN UP-Term Amarchy\n  Run >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\n")
        out = rig.user_startup_with_shadows_block(kit).decode("latin-1")
        self.assertLess(out.index(";END UP-Term assign"), out.index(";BEGIN rig-upterm-shadows"))
        self.assertLess(out.index("Execute BOOTX:shadows-up"), out.index(";BEGIN UP-Term Amarchy"))
        self.assertEqual(rig.user_startup_with_shadows_block(out.encode("latin-1")).decode("latin-1"), out)   # once
        self.assertEqual(rig.user_startup_with_shadows_block(b"; no kit\n"), b"; no kit\n")   # go runs the pass
        script = rig.SHADOWS_UP_SCRIPT
        self.assertIn("C:Copy >NIL: VTC:amarchy-flat UP-Term:bin/amarchy-flat CLONE", script)
        self.assertIn("Echo >%s" % rig.SHADOWS_UP_DONE, script)
        self.assertIn("If EXISTS %s" % rig.SHADOWS_UP_DONE, rig.GO)
        go_else = rig.GO.split("If EXISTS %s" % rig.SHADOWS_UP_DONE, 1)[1].split('"UP-Term shadows done"', 1)[0]
        self.assertIn("C:Copy >NIL: VTC:amarchy-flat UP-Term:bin/amarchy-flat CLONE", go_else)   # go still runs it without the marker

    def test_early_block_assigns_libs_ixp6_before_amarchy_and_go_skips_then(self):
        # AMARCHY-MENU-IXEMUL-BOOT-RACE: amarchy-menu started from User-Startup with no ixemul.library in LIBS:
        script = rig.SHADOWS_UP_SCRIPT
        self.assertIn("If EXISTS VTC:ixp6/ixemul.library", script)
        self.assertIn("C:Assign >NIL: LIBS: VTC:ixp6\n", script)
        self.assertIn("Echo >%s" % rig.RIG_LIBS_DONE, script)
        kit = b";BEGIN UP-Term Amarchy\n  Run >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\n"
        out = rig.user_startup_with_shadows_block(kit).decode("latin-1")
        self.assertLess(out.index("Execute BOOTX:shadows-up"), out.index(";BEGIN UP-Term Amarchy"))
        go = rig.GO
        self.assertIn("If EXISTS %s" % rig.RIG_LIBS_DONE, go)
        go_else = go.split("If EXISTS %s" % rig.RIG_LIBS_DONE, 1)[1]
        self.assertIn("C:Assign >NIL: LIBS: VTC:ixp6\n", go_else)   # go still assigns without the marker
        self.assertEqual(go.count("LIBS: VTC:ixp6\n"), 1)
        self.assertIn("If NOT EXISTS %s" % rig.RIG_LIBS_DONE, go)   # the ncurses ADD is skipped too

    def test_boot_never_names_env_or_envarc(self):
        # go is Run before DH0:S/Startup-Sequence assigns ENV: and ENVARC:; naming either opened a
        # System Request and hung every boot (rig 8, 2026-10-27)
        go = rig.kit_shadow_lines(False) + rig.kit_shadow_lines(True)
        self.assertNotRegex(go, r"\bENV(ARC)?:")

    def test_install_stages_dist_keymap_and_refreshes_apps_bin(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        root, vtc = tmp / "root", tmp / "rig9/vtc"
        (root / "build/amiga").mkdir(parents=True)
        (root / "build/amiga/vsh").write_bytes(b"vsh")   # rig install refuses a build without vsh
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
        (root / "build/amiga/vsh").write_bytes(b"vsh")   # rig install refuses a build without vsh
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
        self.assertEqual(sorted(names), ["AmTile", "Amarchy", "amarchy-flat", "amarchy-gui", "amarchy-icons", "amarchy-index", "amarchy-menu", "amnotify"])
        up = rig.kit_shadow_lines(True)
        for n in names:
            self.assertIn("C:Copy >NIL: VTC:%s UP-Term:bin/%s CLONE" % (n, n), up)
            self.assertIn("C:Copy >NIL: VTC:kit-orig/UP-Term-bin-%s UP-Term:bin/%s CLONE" % (n, n), up)
            self.assertEqual(rig.kit_shadow_src(n), rig.build_amiga() / n)

    def test_install_stages_amarchy_themes_and_font_and_boot_places_them(self):
        # RIG-NO-THEMES: install through rig.main stages them in VTC:, the boot script copies them to the kit places
        tmp = pathlib.Path(tempfile.mkdtemp())
        root, vtc = tmp / "root", tmp / "rig9/vtc"
        (root / "build/amiga").mkdir(parents=True)
        (root / "build/amiga/vsh").write_bytes(b"vsh")   # rig install refuses a build without vsh
        (root / "build/jbmono").mkdir(parents=True)
        (root / "dist/amarchy-themes").mkdir(parents=True)
        (root / "build/amiga/vtcon-handler").write_bytes(b"handler-new")
        (root / "dist/amarchy-themes/nord.theme").write_text("name=nord\n")
        (root / "build/jbmono/JetBrainsMono-Regular.ttf").write_bytes(b"ttf")
        vtc.mkdir(parents=True)
        with mock.patch.object(rig, "ROOT", root), mock.patch.object(rig, "RIG", tmp / "rig9"), \
             mock.patch.object(rig.paths, "RIG_HOME", tmp), mock.patch.object(rig, "stage_upterm", lambda d, v: None), \
             mock.patch.object(rig, "KIT_SHADOWS", [x for x in rig.KIT_SHADOWS if x[0] == "vtcon-handler"]), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(rig.main(["rig.py", "install"]), 0)
        self.assertEqual((vtc / "amarchy-themes/nord.theme").read_text(), "name=nord\n")
        self.assertEqual((vtc / "fonts/JetBrainsMono.ttf").read_bytes(), b"ttf")
        boot = rig.AMARCHY_ASSETS_BOOT
        copy = "C:Copy >NIL: VTC:amarchy-themes/#? UP-Term:Amarchy/Themes CLONE"
        self.assertIn(copy, boot)
        # Every boot refreshes the themes: an older drawer from an earlier kit
        # must not keep stale or partial themes, so the copy is never gated on
        # the drawer being missing.
        head = boot[:boot.index(copy)]
        guard = head.rfind("If NOT EXISTS UP-Term:Amarchy/Themes")
        self.assertTrue(guard < 0 or "EndIf" in head[guard:], "theme copy is gated on a missing drawer")
        self.assertIn("C:Copy >NIL: VTC:fonts/JetBrainsMono.ttf FONTS: CLONE", rig.AMARCHY_ASSETS_BOOT)


if __name__ == "__main__":
    unittest.main()
