"""tools/rig/rig.py: a dev rig starts UP-Term the way an installed kit does (R9).

Regression (2026-10-13): the icon in VTCX:UP-Term said "unable to open your tool vsh" (its tool
is UP-Term:bin/vsh and the rig's UP-Term: held only glyph pages), SYS:System had no UP-Term
icon, and the boot set a global TERM vtcon that no kit sets.
"""
import pathlib
import shlex
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
    check(lines.index("C:Assign >NIL: UP-Term: VTC:Apps") < i, "UP-Term: is assigned before the icon is copied")

with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / "vsh").write_bytes(b"vsh-binary")
    if hasattr(rig, "stage_upterm"):
        rig.stage_upterm(tmp / "Apps", tmp / "vsh")
        check((tmp / "Apps/bin/vsh").read_bytes() == b"vsh-binary", "UP-Term:bin/vsh is vsh")
        check(b"UP-Term:bin/vsh" in (tmp / "Apps/UP-Term.info").read_bytes(), "the icon's tool is UP-Term:bin/vsh")
        check((tmp / "Apps/UP-Term").is_file(), "the icon has its file")
        drawer = (tmp / "Apps.info").read_bytes() if (tmp / "Apps.info").is_file() else b""
        check(len(drawer) > 78 + 62, "the app drawer gets a drawer icon, VTC:Apps.info (Workbench could not open it)")
        check(drawer[:4].hex() == "e3100001" and drawer[48:49] == b"\x02",
              "UP-Term.info is a WBDRAWER disk object (magic, version, do_Type 2)")
        # do_DefaultTool 0, do_DrawerData present: the 62-byte DrawerData follows the 78-byte DiskObject
        check(drawer[50:54] == b"\0\0\0\0" and drawer[66:70] != b"\0\0\0\0",
              "the drawer icon has no default tool and carries a DrawerData")
        check(b"UP-Term Prefs" in (tmp / "Apps/UP-Term-Prefs.info").read_bytes(), "the Prefs icon's tool is C:UP-Term Prefs")
    else:
        check(False, "rig.stage_upterm exists")

# install removes what no committed code makes at the root of the host drawer, and an orphan drawer icon
import subprocess
with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    mk = lambda out, *a: subprocess.run([sys.executable, str(HERE / "mkicon.py"), str(out), *a], check=True)
    mk(tmp / "UP-Term.info")
    (tmp / "UP-Term").write_text("old launcher\n")
    mk(tmp / "Apps.info", "--drawer")        # its drawer is gone
    (tmp / "Apps.info.uaem").write_text("x")
    mk(tmp / "kept.info", "--drawer")        # its drawer is there
    (tmp / "kept").mkdir()
    (tmp / "Disk.info").write_bytes(b"\xe3\x10\x00\x01" + bytes(44) + b"\x01" + bytes(40))   # a volume icon
    (tmp / "upterm").mkdir()
    if hasattr(rig, "clean_stale_icons"):
        rig.clean_stale_icons(tmp)
        left = sorted(f.name for f in tmp.iterdir() if f.is_file())
        check(left == ["Disk.info", "kept.info"], "stale VTC:UP-Term, UP-Term.info and the orphan Apps.info go, the rest stays: %s" % left)
    else:
        check(False, "rig.clean_stale_icons exists")

# the drawer is VTC:Apps now; install renames
# an old VTC:upterm, and does not take the new drawer or its icon for the stale launcher
with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    check(rig.paths.UPTERM_DRAWER == "Apps", "the rig's drawer is called Apps")
    (tmp / "upterm/bin").mkdir(parents=True)
    (tmp / "upterm/bin/vsh").write_bytes(b"v")
    (tmp / "upterm.info").write_bytes(b"old drawer icon")
    if hasattr(rig, "migrate_upterm_drawer"):
        rig.migrate_upterm_drawer(tmp)
        names = sorted(f.name for f in tmp.iterdir())
        check(names == ["Apps"], "VTC:upterm and its icon become VTC:Apps: %s" % names)
        check((tmp / "Apps/bin/vsh").read_bytes() == b"v", "the renamed drawer keeps its files")
        subprocess.run([sys.executable, str(HERE / "mkicon.py"), str(tmp / "Apps.info"), "--drawer"], check=True)
        rig.clean_stale_icons(tmp)
        check(sorted(f.name for f in tmp.iterdir()) == ["Apps", "Apps.info"], "the new drawer and its icon are not stale")
    else:
        check(False, "rig.migrate_upterm_drawer exists")

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
        w = [] if l.startswith((";", "Echo")) else shlex.split(l)
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
            args = [a for a in w[1:] if a not in (">NIL:", "CLONE")]
            dst = args[-1].rstrip("/").lower()
            for a in args[:-1]:
                if ask(a):
                    files.add(dst + "/" + a.split(":", 1)[1].lower())
    return assigns, files, requesters


ICON = {"vtc:apps/up-term.info", "vtc:apps/up-term"}
# the rig with no kit: UP-Term: is the rig's drawer and the icon goes to SYS:System
a, f, req = boot(rig.GO, {}, ICON)
check(not req and a.get("UP-TERM") == "VTC:Apps", "no kit: UP-Term: is VTC:Apps")
check("sys:system/up-term.info" in f, "no kit: the icon is copied to SYS:System")
# W-R3: the Prefs editor's plain icon goes to SYS:Prefs, a kit's own stays
PICON = ICON | {"vtc:apps/up-term-prefs.info", "vtc:apps/up-term-prefs"}
a, f, req = boot(rig.GO, {}, PICON)
check("sys:prefs/up-term-prefs.info" in f, "no kit: the Prefs icon is copied to SYS:Prefs")
a, f, req = boot(rig.GO, {}, PICON | {"sys:prefs/up-term-prefs.info"})
check("sys:prefs/up-term-prefs" not in f, "a kit's Prefs icon in SYS:Prefs stays")
check('C:Copy >NIL: VTC:upprefs "C:UP-Term Prefs" CLONE' in [l.strip() for l in rig.GO.splitlines()], "GO installs C:UP-Term Prefs")

# regression (rig 1, 2026-10-13): an installer_rig run left ENVARC:up-term/Dir naming
# SYS:UP-Term, a drawer it had deleted; the boot asked for the volume UP-Term: and froze
# after "go started"
a, f, req = boot(rig.GO, {}, ICON | {"envarc:up-term/dir"})
check(not req, "a stale ENVARC:up-term/Dir: the boot asks for no volume (asked for %s)" % req)
check(a.get("UP-TERM") == "VTC:Apps", "a stale ENVARC:up-term/Dir: UP-Term: is still assigned")
# a kit whose User-Startup block ran first keeps its own assign and icon
a, f, req = boot(rig.GO, {"UP-TERM": "SYS:UP-Term"}, {"sys:system/up-term.info", "envarc:up-term/dir"})
check(not req and a["UP-TERM"] == "SYS:UP-Term", "a kit's assign stays")

# standalone_rig.py takes the rig's own UP-Term: away for its stock-Workbench runs, never a kit's
import standalone_rig  # noqa: E402
check(standalone_rig.rig_upterm_assign("Volumes:\nUP-Term         VTCX:Apps\nGG  VTCX:\n"), "the rig's UP-Term: assign is recognised")
check(not standalone_rig.rig_upterm_assign("UP-Term         DH0:UP-Term\n"), "a kit's UP-Term: assign is not the rig's")
check(not standalone_rig.rig_upterm_assign("GG  VTCX:\n"), "no UP-Term: assign is not the rig's")

print("[%s] rig_upterm: %d failed" % ("FAIL" if fails else "OK", fails))
sys.exit(1 if fails else 0)
