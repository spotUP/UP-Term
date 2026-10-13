"""tools/rig/rig.py: a dev rig starts UP-Term the way an installed kit does (R9).

Regression (2026-10-13): the icon in VTCX:UP-Term said "unable to open your tool vsh" (its tool
is UP-Term:bin/vsh and the rig's UP-Term: held only glyph pages), SYS:System had no UP-Term
icon, and the boot set a global TERM vtcon that no kit sets.
"""
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import rig  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


COPY = "C:Copy UP-Term:UP-Term UP-Term:UP-Term.info SYS:System CLONE"
lines = [l.strip() for l in rig.GO.splitlines()]
check(not any(l.startswith("C:SetEnv TERM ") for l in lines), "GO sets no global TERM")
check(COPY in lines, "GO puts the UP-Term icon in SYS:System")
if COPY in lines:
    i = lines.index(COPY)
    check("If NOT EXISTS SYS:System/UP-Term.info" in lines[:i], "a kit's own icon in SYS:System stays")
    check(lines.index("C:Assign >NIL: UP-Term: VTC:upterm") < i, "UP-Term: is assigned before the icon is copied")

with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / "vsh").write_bytes(b"vsh-binary")
    if hasattr(rig, "stage_upterm"):
        rig.stage_upterm(tmp / "upterm", tmp / "vsh")
        check((tmp / "upterm/bin/vsh").read_bytes() == b"vsh-binary", "UP-Term:bin/vsh is vsh")
        check(b"UP-Term:bin/vsh" in (tmp / "upterm/UP-Term.info").read_bytes(), "the icon's tool is UP-Term:bin/vsh")
        check((tmp / "upterm/UP-Term").is_file(), "the icon has its file")
    else:
        check(False, "rig.stage_upterm exists")

# standalone_rig.py takes the rig's own UP-Term: away for its stock-Workbench runs, never a kit's
import standalone_rig  # noqa: E402
check(standalone_rig.rig_upterm_assign("Volumes:\nUP-Term         VTCX:upterm\nGG  VTCX:\n"), "the rig's UP-Term: assign is recognised")
check(not standalone_rig.rig_upterm_assign("UP-Term         DH0:UP-Term\n"), "a kit's UP-Term: assign is not the rig's")
check(not standalone_rig.rig_upterm_assign("GG  VTCX:\n"), "no UP-Term: assign is not the rig's")

print("[%s] rig_upterm: %d failed" % ("FAIL" if fails else "OK", fails))
sys.exit(1 if fails else 0)
