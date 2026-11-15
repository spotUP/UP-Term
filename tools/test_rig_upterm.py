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
    (tmp / "Patina").write_bytes(b"patina-binary")
    (tmp / "patina-menu").write_bytes(b"menu-binary")
    if hasattr(rig, "stage_upterm"):
        rig.stage_upterm(tmp / "Apps", tmp / "vsh")
        # 2026-10-22: the rig had no Patina (install refreshed only what VTC: held), so no rig check could start it
        check((tmp / "Apps/bin/Patina").is_file() and (tmp / "Apps/bin/Patina").read_bytes() == b"patina-binary",
              "UP-Term:bin/Patina is the built Patina, where the kit puts it")
        check((tmp / "Apps/bin/patina-menu").is_file() and (tmp / "Apps/bin/patina-menu").read_bytes() == b"menu-binary",
              "UP-Term:bin/patina-menu is the built patina-menu")
        check((tmp / "Apps/bin/vsh").read_bytes() == b"vsh-binary", "UP-Term:bin/vsh is vsh")
        check(b"UP-Term:bin/vsh" in (tmp / "Apps/UP-Term.info").read_bytes(), "the icon's tool is UP-Term:bin/vsh")
        check((tmp / "Apps/UP-Term").is_file(), "the icon has its file")
        check(b"XCON:0/20/0/400/UP-Term/CLOSE" in (tmp / "Apps/UP-Term.info").read_bytes(),
              "the icon window is XCON:0/20/0/400/UP-Term/CLOSE (width 0: as wide as the screen)")
        drawer = (tmp / "Apps.info").read_bytes() if (tmp / "Apps.info").is_file() else b""
        check(len(drawer) > 78 + 62, "the app drawer gets a drawer icon, VTC:Apps.info (Workbench could not open it)")
        check(drawer[:4].hex() == "e3100001" and drawer[48:49] == b"\x02",
              "UP-Term.info is a WBDRAWER disk object (magic, version, do_Type 2)")
        # do_DefaultTool 0, do_DrawerData present: the 62-byte DrawerData follows the 78-byte DiskObject
        check(drawer[50:54] == b"\0\0\0\0" and drawer[66:70] != b"\0\0\0\0",
              "the drawer icon has no default tool and carries a DrawerData")
        check(b"C:UP-Term-Prefs" in (tmp / "Apps/UP-Term-Prefs.info").read_bytes(), "the Prefs icon's tool is C:UP-Term-Prefs")
    else:
        check(False, "rig.stage_upterm exists")

# 2026-10-22 (rig 4): UP-Term: was Rushhours:Tools/UP-Term, the binaries were staged in VTC:Apps/bin, and the
# rig scripts started UP-Term:bin/Patina and found nothing. They start from where stage_upterm writes.
import paths
check(rig.paths.UPTERM_BIN == "VTC:%s/bin" % rig.paths.UPTERM_DRAWER, "UPTERM_BIN is the staged drawer's bin")
for script in ("patina_rig.py", "patina_menu_rig.py"):
    src = (HERE / "rig" / script).read_text()
    check("UP-Term:bin/Patina" not in src, "%s does not start Patina from UP-Term: (a kit install moves it)" % script)
    check("Run >NIL: %s/Patina" in src and "paths.UPTERM_BIN" in src,
          "%s starts Patina from paths.UPTERM_BIN, where stage_upterm stages it" % script)

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
check('C:Copy >NIL: VTC:upprefs C:UP-Term-Prefs CLONE' in [l.strip() for l in rig.GO.splitlines()], "GO installs C:UP-Term-Prefs")

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

# a harness that drops UP-Term: gets back the target it found, not a fixed drawer (S85: rig 84 lost the kit's assign)
import install_rig as ir  # noqa: E402


def fake_amiga(assign):
    sent = []

    def run_fn(cmd):
        sent.append(cmd)
        if cmd == "Assign UP-Term: EXISTS":
            return (0, "UP-Term         %s\n" % "\n                 ".join(assign)) if assign else (5, "")
        if cmd.startswith("Assign >NIL: UP-Term: EXISTS"):
            return (0 if state["on"] else 5, "")
        if cmd == "Assign UP-Term:" or cmd == "Assign >NIL: UP-Term:":
            state["on"] = False
        elif cmd.startswith("Assign UP-Term:"):
            state["on"] = True
        return (0, "")
    state = {"on": bool(assign)}
    return run_fn, sent, state


ir._UPTERM_WAS = None
fn, sent, st = fake_amiga(["Rushhours:Tools/UP-Term", "Other:Add"])
ir.drop_upterm_assign(fn)
ir.drop_upterm_assign(fn)   # the second drop must not record the already dropped state
ir.restore_upterm_assign(fn)
check('Assign UP-Term: "Rushhours:Tools/UP-Term"' in sent and 'Assign UP-Term: "Other:Add" ADD' in sent, "restore re-makes the kit target and its ADDs: %s" % sent)
check(not any("VTC:" in c for c in sent), "restore names no VTC drawer")
check(st["on"], "UP-Term: exists after the restore")
# it was absent before the drop: a check that re-assigned it leaves it absent again
fn, sent, st = fake_amiga([])
ir.drop_upterm_assign(fn)
fn('Assign UP-Term: VTC:Apps')
ir.restore_upterm_assign(fn)
check(not st["on"], "an assign that was absent is absent again after the restore")
check(ir._UPTERM_WAS is None, "restore clears the record")
# rig.py's boot guard: the kit's target from the User-Startup block, the VTC drawer only without one
check(rig.upterm_assign_cmds('Assign UP-Term: "Rushhours:Tools/UP-Term"\n') == ['Assign >NIL: UP-Term: "Rushhours:Tools/UP-Term"'],
      "guard re-assigns the kit's User-Startup target")
check(rig.upterm_assign_cmds("") == ["Assign >NIL: UP-Term: VTC:" + rig.paths.UPTERM_DRAWER], "guard falls back to the VTC drawer without a kit block")

print("[%s] rig_upterm: %d failed" % ("FAIL" if fails else "OK", fails))
sys.exit(1 if fails else 0)
