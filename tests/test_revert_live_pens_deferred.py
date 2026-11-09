"""S66-FREEZE-LIVEPENS: a theme pick left to its 15 s timeout Revert froze the whole KS 3.2 guest (rig 66, s66). The step probes put the
wedge inside dk_live_pens, called by dk_restore_targets inside its windows-closed bracket right after the Palette reset (dk_wait_reset).
dk_live_pens held a visitor lock on the Workbench screen across ipd_role_write, which takes the holder semaphore and then the public screen
list; while the IPrefs reset was still running that lock order closed a cycle. Apply never froze: it defers dk_live_pens (dk_live_want)
until the windows are back. Invariants: (1) no dk_live_pens call inside a dk_windows_close/dk_windows_open bracket, the revert defers it
through dk_live_want to desk_dos_revert, after dk_restore_targets; (2) dk_live_pens holds no Workbench screen lock while ipd_role_write runs.
amarchy/desk_dos.c is Amiga-only code no host test links, so this test reads its structure."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = (ROOT / "amarchy" / "desk_dos.c").read_text()


def body(src, head):
    j = src.index(head + "\n{") + len(head)   # the definition, not a forward declaration
    depth, k = 0, j + 1
    while True:
        c = src[k]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[j + 1:k + 1]
        k += 1


class RevertLivePensDeferred(unittest.TestCase):
    def test_restore_targets_defers_live_pens(self):
        b = body(SRC, "static void dk_restore_targets(unsigned mask, const char *dir)")
        self.assertNotIn("dk_live_pens(", b, "dk_restore_targets sets the live pens inside its windows-closed bracket: KS 3.2 freeze")
        self.assertIn("dk_live_want =", b, "dk_restore_targets must hand the live pens on through dk_live_want")
        self.assertIn("if (!dk_live_want)", b, "a pick's own live-pen want (desk_dos_apply -> dk_orig_off) must not be overwritten")

    def test_revert_runs_live_pens_after_restore(self):
        b = body(SRC, "void desk_dos_revert(void)")
        restore = b.index("dk_restore_targets(")
        live = b.find("dk_live_pens(", restore)
        self.assertNotEqual(live, -1, "desk_dos_revert never sets the live pens after the windows are back")
        self.assertLess(b.index("dk_live_want = 0"), restore, "desk_dos_revert must clear dk_live_want before the restore")
        self.assertLess(live, b.index("dk_rv_theme = 0"), "the live pens follow the previous theme: before dk_rv_theme is cleared")

    def test_no_live_pens_inside_a_windows_bracket(self):
        for head in ("static void dk_restore_targets(unsigned mask, const char *dir)", "void desk_dos_revert(void)",
                     "int desk_dos_apply(const gtheme *th, char *why, int cap)"):
            b = body(SRC, head)
            k = b.find("dk_live_pens(")
            while k != -1:
                close, opn = b.rfind("dk_windows_close()", 0, k), b.rfind("dk_windows_open(", 0, k)
                self.assertFalse(close != -1 and close > opn, head + ": dk_live_pens with Amarchy's windows closed")
                k = b.find("dk_live_pens(", k + 1)

    def test_live_pens_holds_no_screen_lock_across_role_write(self):
        b = body(SRC, "static void dk_live_pens(const gtheme *th, int flat)")
        w = b.index("ipd_role_write(")
        self.assertGreater(b.rfind("UnlockPubScreen(", 0, w), b.rfind("ipd_lock_wb_screen()", 0, w),
                           "dk_live_pens holds the Workbench visitor lock across ipd_role_write (holder semaphore, list lock): KS 3.2 freeze")


if __name__ == "__main__":
    sys.exit(unittest.main())
