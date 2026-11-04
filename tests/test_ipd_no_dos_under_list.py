"""The icon pen holder reads no file while it holds the public screen list (source guard, FLAT-BOOT-FREEZE).

iconpen_dos.c locks the public screen list across a pen sync so the Workbench screen cannot close under its colour map.
At boot, amarchy-flat runs that sync the first second after LoadWB opens the Workbench. The theme read (amconf_load,
gt_load, Open/Read) then runs on a volume that is not ready yet, and DOS puts up a requester. A requester needs the
public screen list to open its window, the sync holds it: the whole system stops (wasabid lost in 4 of 4 boots, serial
trace stops between gt_load's start and its return with the list locked).

So every function that calls LockPubScreenList must not call a DOS reader before UnlockPubScreenList, and the theme op it
hands to ip_sync / ip_recolour must be the cached one (theme read before the lock), never the file reader pens_theme.
"""
import pathlib, re, unittest

SRC = pathlib.Path(__file__).resolve().parent.parent / "amarchy" / "iconpen_dos.c"
DOS = ("amconf_load(", "gt_load(", "pens_theme(", "Open(", "Read(", "Lock(", "Examine(")


def functions(src):
    """(name, body) for every top-level function definition."""
    out = []
    for m in re.finditer(r"^[a-zA-Z][^\n;]*?\b(\w+)\s*\([^;{]*\)\s*\n\{\n(.*?)^\}", src, re.S | re.M):
        out.append((m.group(1), m.group(2)))
    return out


class IpdNoDosUnderList(unittest.TestCase):
    def test_no_file_read_while_the_list_is_locked(self):
        src = SRC.read_text(errors="replace")
        bodies = functions(src)
        self.assertTrue(any("LockPubScreenList()" in b for _, b in bodies), "parser found no LockPubScreenList caller")
        for name, body in bodies:
            for m in re.finditer(r"LockPubScreenList\(\)(.*?)UnlockPubScreenList\(\)", body, re.S):
                for call in DOS:
                    self.assertNotIn(call, m.group(1), f"{name}: {call} runs with the public screen list locked")
            if "LockPubScreenList()" in body:
                self.assertNotRegex(body, r"\.theme\s*=\s*pens_theme\s*;",
                                    f"{name}: ip ops theme reads files and runs under the list lock")

    def test_cached_theme_op_reads_no_file(self):
        src = SRC.read_text(errors="replace")
        body = dict(functions(src)).get("pens_theme_cached")
        self.assertIsNotNone(body, "iconpen_dos.c has no pens_theme_cached")
        for call in DOS:
            self.assertNotIn(call, body, f"pens_theme_cached calls {call}")


if __name__ == "__main__":
    unittest.main()
