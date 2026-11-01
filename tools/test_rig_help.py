"""rig.py accepts --help, -h, help and ? and prints its usage.

2026-10-21: `rig.py --help` ended in `KeyError: '--help'`: the command table was indexed with argv[1] and had no
help forms (the suite rule: every command takes --help, -h, help and ?). No command or an unknown one now prints
the usage to stderr and returns 2. No emulator is run: rig.main is called with each form.
"""
import contextlib
import io
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
saved_argv = sys.argv
sys.argv = ["rig.py", "test"]
import rig  # noqa: E402
sys.argv = saved_argv

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


def run(argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            rc = rig.main(argv)
        except Exception as e:  # the old table raised KeyError / IndexError
            rc = "raised %s" % type(e).__name__
    return rc, out.getvalue(), err.getvalue()


for form in ("--help", "-h", "help", "?"):
    rc, out, err = run(["rig.py", form])
    check(rc == 0, "%s returns 0 (got %r)" % (form, rc))
    check("rig.py start" in out, "%s prints the usage on stdout" % form)

for argv, what in ((["rig.py"], "no command"), (["rig.py", "bogus"], "unknown command")):
    rc, out, err = run(argv)
    check(rc == 2, "%s returns 2 (got %r)" % (what, rc))
    check("rig.py start" in err and what in err, "%s prints the usage and the reason on stderr" % what)

called = []
rig.status = lambda: called.append("status") or False
rc, out, err = run(["rig.py", "status"])
check(called == ["status"] and rc == 0, "a known command runs its handler and returns 0 (got %r, %r)" % (called, rc))

print("[OK] rig_help" if not fails else "[ERROR] rig_help: %d failed" % fails)
sys.exit(1 if fails else 0)
