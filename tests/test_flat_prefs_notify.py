"""FLAT-PREFS-NOT-NOTIFIED: a flat switch written to ENV:Patina/prefs by any writer (not only a desk apply) must reach
patina-flat. The commodity's prefs-file notify ends in reload() (patina.c); reload() must call desk_dos_flat_sync(),
which writes the running block only on a change, else starts patina-flat BOOT when a flat switch is on.
Rig 10 (3.2, 2026-11-01): a direct prefs write with both flat keys on left patina-flat absent (flat=absent after 6 s)."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def body(src, sig):
    i = src.index(sig)
    j = src.index("\n}\n", i)
    return src[i:j]


class FlatPrefsNotify(unittest.TestCase):
    def test_reload_syncs_the_flat_block(self):
        src = (ROOT / "patina/patina.c").read_text()
        self.assertIn("desk_dos_flat_sync();", body(src, "static void reload(void)"))

    def test_notify_and_rexx_reload_go_through_reload(self):
        src = (ROOT / "patina/patina.c").read_text()
        self.assertRegex(src, r"GetMsg\(notify_port\)[\s\S]{0,80}reload\(\);")

    def test_sync_writes_only_on_change_and_starts_when_absent(self):
        src = (ROOT / "patina/desk_dos.c").read_text()
        b = body(src, "void desk_dos_flat_sync(void)")
        self.assertIn("dk_flat_sync_with(1)", b)
        # TE-6 moved the write into dk_flat_sync_theme (the theme editor's live apply passes its own theme); dk_flat_sync_with
        # loads the current theme and hands changed_only on
        self.assertIn("dk_flat_sync_theme(&th, changed_only);", body(src, "static void dk_flat_sync_with(int changed_only)"))
        w = body(src, "static void dk_flat_sync_theme(const gtheme *thp, int changed_only)")
        self.assertRegex(w, r"!changed_only \|\| memcmp\(&next, &r->st, sizeof next\) != 0")
        self.assertIn("dk_flat_start(flat, gad, th);", w)
        self.assertIn('pl_start(PL_BIN_DIR "/patina-flat BOOT"', body(src, "static void dk_flat_start(int flat, int gad, const gtheme *th)"))
        self.assertIn("void desk_dos_flat_sync(void);", (ROOT / "patina/desk_dos.h").read_text())

    def test_desk_apply_still_writes_unconditionally(self):
        src = (ROOT / "patina/desk_dos.c").read_text()
        self.assertIn("dk_flat_sync_with(0)", body(src, "static void dk_flat_sync(void)"))


if __name__ == "__main__":
    unittest.main()


class FlatFallback(unittest.TestCase):
    """FLAT-FALLBACK: patina-flat that never publishes its semaphore leaves flat pens with no flat program. Both start callers
    (boot gate, desk sync) go through dk_flat_start, which polls and then forces the stock-pen restore for this boot only."""
    def setUp(self):
        self.src = (ROOT / "patina/desk_dos.c").read_text()

    def test_start_polls_then_forces_stock_pens_without_touching_prefs(self):
        b = body(self.src, "static void dk_flat_start(int flat, int gad, const gtheme *th)")
        self.assertIn("FindSemaphore((STRPTR)FLAT_SEM_NAME)", b)
        self.assertIn("DK_FLAT_WAIT_TICKS", b)
        self.assertIn("dk_flat_failed = 1;", b)
        self.assertIn("dk_flat_pens(th);", b)
        self.assertNotIn("amconf_set", b)

    def test_both_callers_pass_a_theme(self):
        self.assertIn("dk_flat_start(flat, gad, th);", body(self.src, "static void dk_flat_sync_theme(const gtheme *thp, int changed_only)"))
        self.assertIn("dk_flat_start(flat, gad, &th);", body(self.src, "int desk_dos_flat_ensure(void)"))

    def test_pens_force_restore_on_failure(self):
        b = body(self.src, "static int dk_flat_pens(const gtheme *th)\n{")
        self.assertIn("&& !dk_flat_failed", b)
        self.assertIn("restore = !flat && (taken || dk_flat_failed);", b)
        self.assertIn("restore && taken", b)
