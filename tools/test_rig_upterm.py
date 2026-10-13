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

def boot(go, assigns, files):
    """Run GO's If/EndIf, Assign and Copy lines over a model of the disk. Returns
    (assigns, files, requesters): a requester is every path whose volume is
    neither a device nor an assign when a line asks for it, where AmigaDOS puts
    up "Please insert volume" and the boot waits for ever."""
    assigns, files = dict(assigns), set(files)
    devices = {"DH0", "VTCX", "BOOTX", "SYS", "C", "S", "L", "LIBS", "DEVS", "FONTS", "ENV", "ENVARC", "T", "VTC"}
    requesters = []

    def ask(path):
        vol = path.split(":", 1)[0].upper()
        if vol not in devices and vol not in assigns:
            requesters.append(path)
            return None
        if vol in assigns:   # through the assign to the drawer it names
            path = assigns[vol].rstrip("/") + "/" + path.split(":", 1)[1]
        return path.lower() in files

    stack, warn = [True], False
    for l in (l.strip() for l in go.splitlines()):
        w = l.split()
        if not w or w[0].startswith(";"):
            continue
        live = all(stack)
        if w[0] == "If":
            neg = w[1] == "NOT"
            cond = w[2:] if neg else w[1:]
            if not live:
                stack.append(False)
                continue
            val = warn if cond == ["WARN"] else bool(ask(cond[1])) if cond[0] == "EXISTS" else True
            stack.append(val != neg)
        elif w[0] == "EndIf":
            stack.pop()
        elif not live:
            continue
        elif w[0] == "C:Assign" and len(w) >= 4 and w[1] == ">NIL:":
            name = w[2].rstrip(":").upper()
            if w[3] == "EXISTS":
                warn = name not in assigns
            elif name in ("UP-TERM",):
                assigns[name] = w[3]
        elif w[0] == "C:Copy":
            srcs = [a for a in w[1:] if ":" in a and not a.upper().startswith("SYS:")]
            for a in srcs:
                if ask(a):
                    files.add("sys:system/" + a.split(":", 1)[1].lower())
    return assigns, files, requesters


ICON = {"vtc:upterm/up-term.info", "vtc:upterm/up-term"}
# the rig with no kit: UP-Term: is the rig's drawer and the icon goes to SYS:System
a, f, req = boot(rig.GO, {}, ICON)
check(not req and a.get("UP-TERM") == "VTC:upterm", "no kit: UP-Term: is VTC:upterm")
check("sys:system/up-term.info" in f, "no kit: the icon is copied to SYS:System")
# regression (rig 1, 2026-10-13): an installer_rig run left ENVARC:up-term/Dir naming
# SYS:UP-Term, a drawer it had deleted; the boot asked for the volume UP-Term: and froze
# after "go started"
a, f, req = boot(rig.GO, {}, ICON | {"envarc:up-term/dir"})
check(not req, "a stale ENVARC:up-term/Dir: the boot asks for no volume (asked for %s)" % req)
check(a.get("UP-TERM") == "VTC:upterm", "a stale ENVARC:up-term/Dir: UP-Term: is still assigned")
# a kit whose User-Startup block ran first keeps its own assign and icon
a, f, req = boot(rig.GO, {"UP-TERM": "SYS:UP-Term"}, {"sys:system/up-term.info", "envarc:up-term/dir"})
check(not req and a["UP-TERM"] == "SYS:UP-Term", "a kit's assign stays")

# standalone_rig.py takes the rig's own UP-Term: away for its stock-Workbench runs, never a kit's
import standalone_rig  # noqa: E402
check(standalone_rig.rig_upterm_assign("Volumes:\nUP-Term         VTCX:upterm\nGG  VTCX:\n"), "the rig's UP-Term: assign is recognised")
check(not standalone_rig.rig_upterm_assign("UP-Term         DH0:UP-Term\n"), "a kit's UP-Term: assign is not the rig's")
check(not standalone_rig.rig_upterm_assign("GG  VTCX:\n"), "no UP-Term: assign is not the rig's")

print("[%s] rig_upterm: %d failed" % ("FAIL" if fails else "OK", fails))
sys.exit(1 if fails else 0)
