"""amarchy-flat starts once: the single-instance check and the record's AddSemaphore are one Forbid section (KS31-BOOT-CRASH).

At boot two starters launch amarchy-flat within the same second: the S:User-Startup line `amarchy-flat BOOT` and
Amarchy's start gate (desk_dos_flat_ensure, which starts it when FindSemaphore(FLAT_SEM_NAME) finds no record). The
program's guard was Forbid(); FindSemaphore; ...; AddSemaphore; Permit(), with the prefs read (amconf_borrow, DOS
file I/O) in the middle. A DOS call waits, the wait breaks Forbid, and the second starter ran its own FindSemaphore
while the first had not added its record yet: both passed the guard and both patched the system. On KS 3.1 the boot
then reset or hung (4 of 6 boots; 5 of 5 boots OK with amarchy-flat replaced by a stub).

So in main(): between the Forbid() that precedes the guard's FindSemaphore and the Permit() after AddSemaphore, no
call that can wait (prefs read, DOS, output) may run, and the prefs read happens before that Forbid().
"""
import pathlib, re, unittest

SRC = pathlib.Path(__file__).resolve().parent.parent / "amarchy" / "flat.c"
WAITS = ("amconf_borrow(", "amconf_load(", "amconf_return(", "desk_flat_state(", "desk_flat_full(", "gt_dos_load_current(", "boot_wanted(", "Open(", "Read(",
         "Lock(", "PutStr(", "Printf(", "print_status(", "Delay(", "Wait(", "OpenLibrary(")


def nocomments(src):
    return re.sub(r"/\*.*?\*/", "", src, flags=re.S)


def main_body():
    src = nocomments(SRC.read_text(errors="replace"))
    m = re.search(r"^int main\(void\)\n\{\n(.*?)^\}", src, re.S | re.M)
    assert m, "flat.c has no main()"
    return m.group(1)


class FlatSingleInstance(unittest.TestCase):
    def guard(self, body):
        find = body.index("FindSemaphore((STRPTR)FLAT_SEM_NAME)")
        forbid = body.rindex("Forbid();", 0, find)
        add = body.index("AddSemaphore(", find)
        permit = body.index("Permit();", add)
        return forbid, find, add, permit

    def test_two_boot_starters_cannot_both_pass_the_guard(self):
        body = main_body()
        forbid, find, add, permit = self.guard(body)
        section = body[forbid:permit]
        # the dead-owner branch Permits around drop_record and re-Forbids before AllocVec; it ends with return or
        # Forbid() again, so check the live stretch: from the last Forbid() before AddSemaphore to AddSemaphore.
        last = section.rindex("Forbid();", 0, add - forbid)
        live = section[last:add - forbid]
        for call in WAITS:
            self.assertNotIn(call, live, f"main: {call} runs between the instance check and AddSemaphore")
        head = section[:section.index("FindSemaphore(")]
        for call in WAITS:
            self.assertNotIn(call, head, f"main: {call} runs inside the guard's Forbid before FindSemaphore")

    def test_the_prefs_are_still_read_before_the_guard(self):
        body = main_body()
        forbid, _, add, _ = self.guard(body)
        before = body[:forbid]
        self.assertIn("amconf_borrow(", before, "the switch and pens are no longer read from the prefs at start")
        self.assertIn("desk_flat_full(", before, "FLAT-BOOT-THEME-FIELDS: main no longer fills the theme fields (sel_rgb) at start")
        self.assertIn("gt_dos_load_current(", before, "FLAT-BOOT-THEME-FIELDS: main no longer loads the current theme at start")
        self.assertIn("amconf_return(", before, "the borrowed prefs are not returned")
        self.assertRegex(body[forbid:add], r"r->st\s*=\s*\w+\s*;", "the record does not take the state read before the guard")


if __name__ == "__main__":
    unittest.main()
