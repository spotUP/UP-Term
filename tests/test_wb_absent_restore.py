"""AM-ICON-NOBORDER-OFF: flat off (dk_orig_off) and revert (dk_wb_restore) put ENV:Sys/Workbench.prefs back to "absent" when the user had none.
Deleting the file alone does not tell workbench.library: on rig 56 (s31, OS 3.2) the icons stayed frameless after flat off. amarchy/desk_dos.c is
Amiga-only code no host test links, so this test reads its structure: every delete of the ENV: Workbench.prefs goes through dk_wb_absent, which
USEs the editor's stock file (dk_put) before the delete."""
import pathlib
import re
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


class WbAbsentRestore(unittest.TestCase):
    def test_absent_restore_uses_stock_before_delete(self):
        b = body(SRC, "static void dk_wb_absent(void)")
        self.assertIn("desk_wb_stock(", b)
        put, dele = b.find("dk_put(\"Workbench\""), b.find("DeleteFile((STRPTR)DK_WB_ENV)")
        self.assertGreaterEqual(put, 0)
        self.assertGreater(dele, put, "the stock USE must come before the delete")

    def test_no_bare_delete_of_env_workbench_prefs(self):
        rest = SRC.replace(body(SRC, "static void dk_wb_absent(void)"), "")
        bare = re.findall(r"DeleteFile\(\(STRPTR\)\s*(?:DK_WB_ENV|DESK_WBPREFS_ENV|\"ENV:Sys/Workbench\.prefs\")\)", rest)
        self.assertEqual(bare, [], "ENV:Sys/Workbench.prefs deleted without the stock USE (icons stay frameless)")

    def test_both_restore_paths_call_it(self):
        self.assertIn("dk_wb_absent();", body(SRC, "static int dk_wb_restore(void)"))
        self.assertIn("dk_wb_absent();", body(SRC, "static void dk_orig_off(unsigned desk_mask, int flat)"))


if __name__ == "__main__":
    sys.exit(unittest.main())
