"""FLAT-PREFS-NOT-NOTIFIED: a flat switch written to ENV:Amarchy/prefs by any writer (not only a desk apply) must reach
amarchy-flat. The commodity's prefs-file notify ends in reload() (amarchy.c); reload() must call desk_dos_flat_sync(),
which writes the running block only on a change, else starts amarchy-flat BOOT when a flat switch is on.
Rig 10 (3.2, 2026-11-01): a direct prefs write with both flat keys on left amarchy-flat absent (flat=absent after 6 s)."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def body(src, sig):
    i = src.index(sig)
    j = src.index("\n}\n", i)
    return src[i:j]


class FlatPrefsNotify(unittest.TestCase):
    def test_reload_syncs_the_flat_block(self):
        src = (ROOT / "amarchy/amarchy.c").read_text()
        self.assertIn("desk_dos_flat_sync();", body(src, "static void reload(void)"))

    def test_notify_and_rexx_reload_go_through_reload(self):
        src = (ROOT / "amarchy/amarchy.c").read_text()
        self.assertRegex(src, r"GetMsg\(notify_port\)[\s\S]{0,80}reload\(\);")

    def test_sync_writes_only_on_change_and_starts_when_absent(self):
        src = (ROOT / "amarchy/desk_dos.c").read_text()
        b = body(src, "void desk_dos_flat_sync(void)")
        self.assertIn("dk_flat_sync_with(1)", b)
        # TE-6 moved the write into dk_flat_sync_theme (the theme editor's live apply passes its own theme); dk_flat_sync_with
        # loads the current theme and hands changed_only on
        self.assertIn("dk_flat_sync_theme(&th, changed_only);", body(src, "static void dk_flat_sync_with(int changed_only)"))
        w = body(src, "static void dk_flat_sync_theme(const gtheme *thp, int changed_only)")
        self.assertRegex(w, r"!changed_only \|\| memcmp\(&next, &r->st, sizeof next\) != 0")
        self.assertIn('pl_start(PL_BIN_DIR "/amarchy-flat BOOT"', w)
        self.assertIn("void desk_dos_flat_sync(void);", (ROOT / "amarchy/desk_dos.h").read_text())

    def test_desk_apply_still_writes_unconditionally(self):
        src = (ROOT / "amarchy/desk_dos.c").read_text()
        self.assertIn("dk_flat_sync_with(0)", body(src, "static void dk_flat_sync(void)"))


if __name__ == "__main__":
    unittest.main()
