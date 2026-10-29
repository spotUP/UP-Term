"""tools/rig/install_rig.py: the `all` case installs on DH1, and stock_libs keeps AmiSSL on LIBS:.

Regression (s25 2026-10-29, rig 9): case_all called prepare(None, ids), so the 67 MB base, up-nvim (33 MB) and
Python3 all landed on DH0 (106 MB) and filled it ("disk is full"). stock_libs rebuilt LIBS: as the system Libs
plus ncurses and dropped the rig's AmiSSL, so UP-Term:bin/curl could not open amisslmaster.library (up-uhc
reported a network failure).
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import install_rig as ir  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


# case_all reaches prepare() with a DEST on DH1 (the entry point of the case, stopped at its first call)
class Stop(Exception):
    pass


seen = []


def fake_prepare(dest, addons=()):
    seen.append((dest, list(addons)))
    raise Stop()


ir.prepare = fake_prepare
try:
    ir.case_all(lambda *a, **k: None)
except Stop:
    pass
check(len(seen) == 1, "case_all called prepare() (sentinel)")
check(seen and seen[0][0] is not None and seen[0][0].startswith("DH1:"), "case_all installs on DH1: %r" % (seen[:1],))
check(seen and len(seen[0][1]) > 1, "case_all prepares every add-on")

# stock_libs: the commands it sends keep the AmiSSL drawers on LIBS:
sent = []
ir.run = lambda cmd, *a, **k: sent.append(cmd) or (0, "")
ir.stock_libs()
check(any("LIBS:" in c and "AmiSSL" in c for c in sent), "stock_libs adds the system disk's AmiSSL/Libs: %r" % sent)
check(any(c.rstrip().endswith("VTC:amissl ADD") for c in sent), "stock_libs adds VTC:amissl: %r" % sent)
check(not any(">NIL:" in c for c in sent), "stock_libs sends plain Assign lines (no redirect)")
check(ir.amissl_libs() == [l for l in ir.rig_libs() if "amissl" in l.lower()] and len(ir.amissl_libs()) == 2,
      "the AmiSSL lines come from rig.RIG_LIBS, not a second copy")

print("rig_all_case: %s" % ("FAIL" if fails else "PASS"))
sys.exit(1 if fails else 0)
