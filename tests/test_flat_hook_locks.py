"""No draw hook of amarchy-flat takes the Intuition lock (FLAT-HOOK-NO-IBASE, s71 freeze).

The sysiclass draw hook runs on whatever task draws a gadget image, inside Intuition and layers calls that already hold
locks later in Intuition's lock order. c7_screen_which called LockIBase there (to find the screen of a title-bar layer):
Directory Opus' clock task, redrawing the screen title, waited for the Intuition lock while a lister task held it and
waited for the lock the clock held. Both tasks hung, then amarchy-flat's main task (aspect_update) queued behind them and
no longer answered Ctrl-C, and the machine froze (waiter chain measured on rig 68, evidence s70-evidence "Waiter chain").

So: LockIBase lives on the main task only (aspect_update, which holds no other lock), and the hook finds the screen from
the layer's own Layer_Info (embedded in struct Screen), accepted only when the main task has published that screen.
"""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "amarchy" / "flat.c"
MAIN_TASK_ONLY = {"aspect_update"}


def code():
    return re.sub(r"/\*.*?\*/", "", SRC.read_text(errors="replace"), flags=re.S)


def funcs(src):
    """name -> body of every function definition at column 0"""
    return {m.group(1): m.group(2) for m in re.finditer(r"^[a-zA-Z][^\n;]*?\b(\w+)\([^\n;]*\)\n\{\n(.*?)^\}", src, re.S | re.M)}


class FlatHookLocks(unittest.TestCase):
    def test_lockibase_only_on_the_main_task(self):
        users = {name for name, body in funcs(code()).items() if "LockIBase(" in body}
        self.assertTrue(users, "the scan found no LockIBase user at all: the function pattern is broken")
        self.assertEqual(users, MAIN_TASK_ONLY, "LockIBase outside the main-task functions (a draw hook must never take it)")

    def test_title_bar_lookup_reads_the_published_screens(self):
        body = funcs(code())["c7_screen_which"]
        self.assertIn("flat_aspect_known(", body, "the screen of a bar layer must be checked against the main task's table")
        self.assertIn("LayerInfo", body, "the screen comes from the layer's own Layer_Info, not from a screen list walk")
        self.assertNotIn("FirstScreen", body, "walking IntuitionBase->FirstScreen needs the Intuition lock")


if __name__ == "__main__":
    unittest.main()
