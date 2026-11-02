"""AM-ICON-NOBORDER-OFF: flat off (dk_orig_off) and revert (dk_wb_restore) put Workbench.prefs back to "absent" when the user had none.
workbench.library learns a change only when IPrefs reads ENV:Sys/Workbench.prefs on its change notice, 1.5-7 s after the editor's USE (rig 56,
s31, OS 3.2, wasabi snoop). 2ae68838 USEd the stock file and then deleted the ENV: file at once: IPrefs' open failed (Error 205) and the icons
stayed frameless. So the stock file the editor writes must stay in ENV: (ENVARC: carries the persistent "absent"), and nothing on the off path may
delete it afterwards (dk_env_fix_file with an absent original deletes). amarchy/desk_dos.c is Amiga-only code no host test links, so this test
reads its structure."""
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


ENV_DELETE = r"DeleteFile\(\(STRPTR\)\s*(?:DK_WB_ENV|DESK_WBPREFS_ENV|\"ENV:Sys/Workbench\.prefs\")\)"


class WbAbsentRestore(unittest.TestCase):
    def test_absent_restore_uses_stock_and_keeps_env_file(self):
        b = body(SRC, "static void dk_wb_absent(void)")
        self.assertIn("desk_wb_stock(", b)
        self.assertIn("dk_put(\"Workbench\"", b)
        self.assertEqual(re.findall(ENV_DELETE, b), [], "deleting the ENV: file after the USE beats IPrefs' read: icons stay frameless")

    def test_no_delete_of_env_workbench_prefs(self):
        self.assertEqual(re.findall(ENV_DELETE, SRC), [], "ENV:Sys/Workbench.prefs deleted: IPrefs cannot read the stock USE")

    def test_off_does_not_env_fix_an_absent_original(self):
        b = body(SRC, "static void dk_orig_off(unsigned desk_mask, int flat)")
        fix = b.index("dk_env_fix_file(DK_ORIG, DESK_WBPREFS_ENV)")
        guard = b.rindex("if (", 0, fix)
        cond = b[guard:b.index(")", guard) + 1]
        self.assertEqual(cond, "if (wbfix)", "the fix runs only for a saved original; for an absent one it deletes the stock ENV: file")
        self.assertIn("wbfix = dk_exists(name);", b)

    def test_both_restore_paths_call_it(self):
        self.assertIn("dk_wb_absent();", body(SRC, "static int dk_wb_restore(void)"))
        self.assertIn("dk_wb_absent();", body(SRC, "static void dk_orig_off(unsigned desk_mask, int flat)"))


if __name__ == "__main__":
    sys.exit(unittest.main())
