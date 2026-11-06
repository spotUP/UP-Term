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
CORE = SRC.with_name("iconpen_core.c")
DOS = ("amconf_load(", "gt_load(", "pens_theme(", "Open(", "Read(", "Lock(", "Examine(")


def nocomments(src):
    return re.sub(r"/\*.*?\*/", "", src, flags=re.S)


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

    def test_theme_read_only_when_pens_are_obtained(self):
        """No unconditional pre-read. The decision lives in iconpen_core.c (ip_screens_sync fills need[] from
        IP_SYNC_NEED_THEME); pens_sync reads a slot's theme only where need[i] is set, after UnlockPubScreenList."""
        core = nocomments(CORE.read_text(errors="replace"))
        sync = dict(functions(core)).get("ip_screens_sync")
        self.assertIsNotNone(sync, "iconpen_core.c has no ip_screens_sync")
        self.assertRegex(sync, r"if\s*\(\s*act\s*==\s*IP_SYNC_NEED_THEME\s*\)\s*\{\s*if\s*\(\s*need\s*\)\s*need\[i\]\s*=\s*1",
                         "ip_screens_sync no longer sets need[i] from IP_SYNC_NEED_THEME")
        body = dict(functions(nocomments(SRC.read_text(errors="replace"))))["pens_sync"]
        self.assertIn("theme_ready", body)
        self.assertIn("need", body)
        first_lock = body.index("LockPubScreenList()")
        for reader in ("pens_theme(", "pens_theme_named(", "amconf_load(", "gt_load("):
            self.assertNotIn(reader, body[:first_lock], f"pens_sync calls {reader} before the lock on every call")
        self.assertNotIn("pens_theme(", body, "pens_sync reads the theme through the unconditional op")
        unlock = body.index("UnlockPubScreenList()")
        self.assertGreater(unlock, first_lock)
        m = re.search(r"if\s*\(\s*need\[i\]\s*\)\s*\{(.*?)\n            \}", body[unlock:], re.S)
        self.assertIsNotNone(m, "pens_sync: no `if (need[i])` block after UnlockPubScreenList")
        self.assertIn("pens_theme_named(", m.group(1), "the per-slot theme read is not under `if (need[i])`")
        self.assertEqual(body.count("pens_theme_named("), 1, "pens_sync reads a theme outside the need[i] block")

    def test_cached_theme_op_reads_no_file(self):
        src = SRC.read_text(errors="replace")
        body = dict(functions(src)).get("pens_theme_cached")
        self.assertIsNotNone(body, "iconpen_dos.c has no pens_theme_cached")
        for call in DOS:
            self.assertNotIn(call, body, f"pens_theme_cached calls {call}")

    def test_theme_read_puts_up_no_requester(self):
        """The requester suppression sits in pens_theme_named, the one function that reads the theme, and every
        theme loader call in iconpen_dos.c is inside it."""
        src = nocomments(SRC.read_text(errors="replace"))
        fns = dict(functions(src))
        body = fns.get("pens_theme_named")
        self.assertIsNotNone(body, "iconpen_dos.c has no pens_theme_named")
        self.assertRegex(body, r"pr_WindowPtr\s*=\s*\(APTR\)\s*-1", "theme read can raise a volume requester (UP-Term: at boot)")
        self.assertRegex(body, r"pr_WindowPtr\s*=\s*oldwin", "pens_theme_named does not restore the old pr_WindowPtr")
        sup = body.index("(APTR)-1")
        loaders = [m.start() for m in re.finditer(r"\b(?:amconf_load|gt_load_model|gt_load)\(", body)]
        self.assertTrue(loaders, "pens_theme_named reads no theme (parser or refactor changed)")
        for pos in loaders:
            self.assertLess(sup, pos, "a theme loader runs before the requester suppression")
        self.assertGreater(body.rindex("oldwin"), max(loaders), "the old pr_WindowPtr is restored before the last read")
        for name, other in fns.items():
            if name == "pens_theme_named":
                continue
            for call in ("gt_load_model(", "gt_load("):
                self.assertNotIn(call, other, f"{name}: {call} reads a theme outside pens_theme_named")
            if "amconf_load(" in other:     # a prefs read must carry its own suppression
                self.assertRegex(other, r"pr_WindowPtr\s*=\s*\(APTR\)\s*-1", f"{name}: amconf_load without the requester suppression")
        self.assertIn("pens_theme_named(", fns["pens_theme"], "pens_theme no longer goes through pens_theme_named")


if __name__ == "__main__":
    unittest.main()
