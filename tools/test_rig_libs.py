"""tools/rig/rig.py: the rig's boot keeps AmiSSL's drawer in LIBS:.

Regression (2026-10-13): every https request of amiga-pi on rigs 2 and 3 ended with
"Connection error.". The system disk's S:User-Startup adds AmiSSL:Libs to LIBS: (AmiSSL's
installer block), but BOOTX:go runs 15 s later and assigns LIBS: anew (the patched ixemul
first), so amisslmaster.library was not found and no TLS connection could start. After a
fresh boot `Assign LIBS: EXISTS` listed no System:AmiSSL/Libs and `Version
amisslmaster.library` said "object not found"; with the drawer added back the same binary
got Anthropic's HTTP 401 for a key that is not one.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import rig  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


AMISSL = "LIBS: DH0:AmiSSL/Libs ADD"
lines = [l.strip() for l in rig.GO.splitlines()]
libs = [i for i, l in enumerate(lines) if "Assign" in l and " LIBS: " in l]
# the last assign that replaces LIBS: (no ADD) drops whatever S:User-Startup added
last_new = max(i for i in libs if not lines[i].endswith(" ADD"))
amissl = [i for i in libs if lines[i].endswith(AMISSL)]
check(amissl and amissl[-1] > last_new,
      "GO adds AmiSSL's drawer after it assigns LIBS: anew (line %d): %r" % (last_new, lines[last_new]))
# install_rig.py puts LIBS: back from RIG_LIBS after its cases: AmiSSL stays there too
check(any(l.endswith(AMISSL) for l in rig.RIG_LIBS), "RIG_LIBS carries AmiSSL's drawer")
# by its volume, never through the AmiSSL: assign (a rig image without it would ask for a volume and stop the boot)
check(not any("AmiSSL:" in l for l in rig.RIG_LIBS), "RIG_LIBS does not name the AmiSSL: assign")

print("[%s] rig_libs: %d failed" % ("FAIL" if fails else "OK", fails))
sys.exit(1 if fails else 0)
