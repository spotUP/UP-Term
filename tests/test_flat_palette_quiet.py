"""S93-PREFS-FLAT: on 3.1 the flat pen map must reach palette.prefs without an IPrefs reset.
Rig 93/94 (KS 3.1 + DOpus): no palette.prefs in ENV: or ENVARC: after boot, so the Workbench DrawInfo kept the stock bevels (Font prefs
window bevelled, patina-flat STATUS frame flat=0) and any IPrefs reset reopened the Workbench with stock colours.
The start apply keeps the 3.1-safe flat file; the 1 s tick writes it once, DK_QUIET_SECS later, ENVARC: directly and ENV: by rename.
AGA060-C (rig 28 snoop): the rename notifies IPrefs as well (IPrefs opened the file 1 ms later) and the reset deadlocked with DOpus, so
the ENV: rename runs only with no DOpus port, with our windows closed around it. The two 3.1 gates (dk_flat_pens, desk_dos_flat_pens_refresh) stay: they would reset the Workbench at boot."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def body(src, sig):
    i = src.index(sig)
    j = src.index("\n}\n", i)
    return src[i:j]


class FlatPaletteQuiet(unittest.TestCase):
    src = (ROOT / "patina/desk_dos.c").read_text()

    def test_31_gates_stay(self):
        self.assertIn("if (!before || !dk_palette_editor_ok())", body(self.src, "static int dk_flat_pens(const gtheme *th)\n{"))
        self.assertIn("if (!flat || !dk_palette_editor_ok())", body(self.src, "int desk_dos_flat_pens_refresh(void)"))

    def test_start_apply_keeps_the_safe_flat_file_on_31_only(self):
        b = body(self.src, "int desk_dos_start_apply(void)")
        # PREFS-RESET-THEME: theme colours armed on 3.1, flat or not; dk_quiet_arm makes them 3.1-safe
        self.assertIn("if (pallen > 0 && !dk_palette_editor_ok())\n        dk_quiet_arm(pal, pallen);", b)
        # the palette is never written to ENV: in the start apply itself (AGA-BOOT-DEADLOCK)
        self.assertNotIn("dk_rename_in(", b)

    def test_tick_write_is_deferred_once_and_notify_free(self):
        b = body(self.src, "void desk_dos_palette_quiet(long now)")
        self.assertIn("dk_quiet_due = now + DK_QUIET_SECS;", b)
        self.assertIn("dk_rename_in(DK_PAL_ENV, dk_quiet_pal, dk_quiet_len)", b)
        self.assertIn("dk_quiet_len = 0;", b)
        # AGA060-C: the ENV: rename resets the Workbench; never under DOpus, and our windows close around it
        self.assertRegex(b, r"!rexx_port_exists\(DOPUS_PORT\)\) \{[^}]*dk_windows_close\(\);[^}]*dk_rename_in\(DK_PAL_ENV")
        self.assertIn("dk_windows_open(was);", b)
        # no editor run, no plain ENV: write (both notify IPrefs, which resets the Workbench)
        self.assertNotRegex(b, r"dk_put\(|dk_put_palette\(|dk_write\(")
        r = body(self.src, "static int dk_rename_in(const char *name, const unsigned char *buf, long len)")
        self.assertNotRegex(r, r"dk_write\(name")

    def test_gate_ticks_after_the_start(self):
        p = (ROOT / "patina/patina.c").read_text()
        self.assertIn("desk_dos_palette_quiet(now);", body(p, "static void desk_start_gate(long now)"))
        self.assertRegex(p, r"\n    tick_hook\(desk_start_gate\);")
        hooks = len(re.findall(r"\btick_hook\(", p)) + len(re.findall(r"\btick_hook\(", (ROOT / "patina/desk_confirm_dos.c").read_text()))
        cap = int(re.search(r"#define TICK_MAX_HOOKS (\d+)", (ROOT / "patina/tick_core.h").read_text()).group(1))
        self.assertLessEqual(hooks, cap)


if __name__ == "__main__":
    unittest.main()
