"""userland_rig tty_compare leaves out the rows a package's check/mask names, and only those.

2026-10-23: nnn's list-quit screen holds the working directory path (row 1) and the highlighted entry's
mtime, mode and size (row 24), which differ between the Mac and the rig. pkgs/<pkg>/check/mask lists such rows
per case. A difference on a masked row passes; the same difference on any other row, or in the attribute part
of the screen, still fails; a case without a mask line is compared whole. No emulator: render is replaced by
a function that returns the stream text as the screen.
"""
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
sys.path.insert(0, str(HERE))
import userland_rig as u  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def screen(rows=None, attrs=None, cur=10):
    t = ["row%d" % i for i in range(1, 25)]
    a = ["attr%d" % i for i in range(1, 25)]
    for k, v in (rows or {}).items():
        t[k - 1] = v
    for k, v in (attrs or {}).items():
        a[k - 1] = v
    return "\n".join(t + a + ["@%d,23" % cur])


def compare(exp_screen, got_screen, mask_lines):
    with tempfile.TemporaryDirectory() as d:
        d = pathlib.Path(d)
        ports, vtc = d / "ports", d / "vtc"
        e, g = ports / "build/expected/pk", vtc / "out/pk"
        (ports / "pkgs/pk/check").mkdir(parents=True)
        e.mkdir(parents=True)
        g.mkdir(parents=True)
        (ports / "pkgs/pk/check/tty").write_text("c1\tprog\n")
        if mask_lines:
            (ports / "pkgs/pk/check/mask").write_text(mask_lines)
        for dd, s in ((e, exp_screen), (g, got_screen)):
            (dd / "c1.txt").write_text("0\n")
            (dd / "c1.stream").write_text(s)
            (dd / "c1.stream.marks").write_text("1 %d\nend 0\n" % len(s.encode()))
        old = (u.PORTS, u.VTC, u.render)
        u.PORTS, u.VTC = ports, vtc
        u.render = lambda stream, upto: stream[:upto].decode()
        try:
            return u.tty_compare("pk", "c1")[0]
        finally:
            u.PORTS, u.VTC, u.render = old


mask = "c1\t1\tpath\nc1\t24\tstatus\n"
base = screen()
check(compare(base, screen({1: "/other/path"}), mask), "difference on masked row 1 passes")
check(compare(base, screen({24: "1K drwx"}, {24: "attr-other"}), mask), "difference on masked row 24 (text and attributes) passes")
check(compare(base, screen(cur=35), mask), "cursor column differs on masked row 24 passes")
check(not compare(base, screen(cur=35).replace("@35,23", "@35,5"), mask), "cursor on an unmasked row still compared")
check(not compare(base, screen({3: "alpha.txt*"}), mask), "difference on unmasked row 3 fails")
check(not compare(base, screen(attrs={3: "bold"}), mask), "attribute difference on unmasked row 3 fails")
check(not compare(base, screen({1: "/other/path"}), ""), "no mask file: the same difference fails")
check(not compare(base, screen({1: "/other/path"}), "c2\t1\tother case\n"), "mask of another case does not apply")
print("FAIL" if fails else "ok")
sys.exit(1 if fails else 0)
