"""tools/rig/rig.py: a rig whose system disk holds an installed kit runs the build `install` wrote (W-R1).

Regression (2026-10-13): XCON: is mounted from DEVS:DOSDrivers/XCON with Handler = L:vtcon-handler,
which the kit's install put in SYS:. `rig.py install` wrote only VTC:vtcon-handler, so three rig
checks ran the old handler. The boot (rig.GO) must put every VTC: build over the kit's copy of it,
keep the kit's own in VTC:kit-orig, bring it back on `kit-restore`, and touch nothing where no kit
put a copy (and never ask for a volume that is not there).
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


def boot(files, assigns=None):
    """Run GO's If/Else/EndIf, MakeDir, Assign EXISTS and Copy lines over a model of the disk
    (path, any case, to content). Returns (files, requesters): a requester is a path on a volume
    that is neither a device nor an assign when a line asks for it."""
    files = {k.lower(): v for k, v in files.items()}
    assigns = dict(assigns or {})
    devices = {"VTC", "SYS", "C", "S", "L", "LIBS", "DEVS", "ENVARC", "BOOTX", "T", "ENV", "FONTS"}
    requesters, dirs = [], {"vtc:"}

    def resolve(path):
        vol, _, rest = path.partition(":")
        if vol.upper() in assigns:
            return assigns[vol.upper()].rstrip("/") + "/" + rest
        return path

    def ask(path):
        vol = path.split(":", 1)[0].upper()
        if vol not in devices and vol not in assigns:
            requesters.append(path)
            return False
        p = resolve(path).lower()
        return p in files or p in dirs

    stack, warn = [True], False
    for l in (l.strip() for l in rig.GO.splitlines()):
        w = [] if l.startswith((";", "Echo")) else shlex.split(l)
        if not w or w[0].startswith(";") or w[0] in ("FailAt", "Echo"):
            continue
        live = all(stack)
        if w[0] == "If":
            if not live:
                stack.append(False)
                continue
            neg = w[1] == "NOT"
            cond = w[2:] if neg else w[1:]
            val = warn if cond == ["WARN"] else ask(cond[1]) if cond[0] == "EXISTS" else True
            stack.append(val != neg)
        elif w[0] == "Else":
            stack[-1] = not stack[-1] if all(stack[:-1]) else False
        elif w[0] == "EndIf":
            stack.pop()
        elif not live:
            continue
        elif w[0] == "C:MakeDir":
            dirs.add(w[-1].lower())
        elif w[0] == "C:Assign" and len(w) >= 4 and w[3] == "EXISTS":
            warn = w[2].rstrip(":").upper() not in assigns
        elif w[0] == "C:Copy":
            a = [x for x in w[1:] if x != ">NIL:" and x != "CLONE"]
            if len(a) == 2 and ":" in a[0] and a[0].split(":")[0].upper() in devices | set(assigns):
                src, dst = resolve(a[0]).lower(), resolve(a[1]).lower()
                if src in files:
                    files[dst] = files[src]
    return files, requesters


KIT = {"L:vtcon-handler": "kit-handler", "L:pty-handler": "kit-pty", "C:vsh": "kit-vsh",
       "DEVS:up-console.device": "kit-dev", "SYS:UP-Term/bin/vsh": "kit-vsh"}
BUILD = {"VTC:vtcon-handler": "new-handler", "VTC:pty-handler": "new-pty", "VTC:vsh": "new-vsh",
         "VTC:up-console.device": "new-dev"}

# the kit is installed: every copy the system holds is the build now
f, req = boot({**KIT, **BUILD}, {"UP-TERM": "SYS:UP-Term"})
check(f["l:vtcon-handler"] == "new-handler", "kit: L:vtcon-handler (what XCON: loads) is the VTC: build")
check(f["l:pty-handler"] == "new-pty", "kit: L:pty-handler is the VTC: build")
check(f["c:vsh"] == "new-vsh", "kit: C:vsh is the VTC: build")
check(f["devs:up-console.device"] == "new-dev", "kit: DEVS:up-console.device is the VTC: build")
check(f["sys:up-term/bin/vsh"] == "new-vsh", "kit: UP-Term:bin/vsh (the icon's tool) is the VTC: build")
check(f["vtc:kit-orig/l-vtcon-handler"] == "kit-handler", "kit: the kit's own handler is kept in VTC:kit-orig")
check(not req, "kit: the boot asks for no volume (asked for %s)" % req)

# a second boot after a new build: the backup stays the KIT's file, not the previous build
f2, _ = boot({**f, "VTC:vtcon-handler": "newer-handler"}, {"UP-TERM": "SYS:UP-Term"})
check(f2["l:vtcon-handler"] == "newer-handler", "second boot: the newer build runs")
check(f2["vtc:kit-orig/l-vtcon-handler"] == "kit-handler", "second boot: the kit's original is not overwritten by a build")

# kit-restore: the boot puts the kit's files back, and install turns shadowing on again
f3, _ = boot({**f2, "VTC:kit-restore": ""}, {"UP-TERM": "SYS:UP-Term"})
check(f3["l:vtcon-handler"] == "kit-handler", "kit-restore: L:vtcon-handler is the kit's again")
check(f3["l:pty-handler"] == "kit-pty", "kit-restore: L:pty-handler is the kit's again")

# W-R3: the Prefs editor is C:UP-Term-Prefs (the handler's menu runs that name); a kit's copy
# is refreshed from VTC:upprefs, and the kit's own is kept for kit-restore
PK = {"C:UP-Term-Prefs": "kit-prefs"}
fp, reqp = boot({**KIT, **PK, **BUILD, "VTC:upprefs": "new-prefs"}, {"UP-TERM": "SYS:UP-Term"})
check(fp["c:up-term-prefs"] == "new-prefs", "kit: C:UP-Term-Prefs is the VTC: build")
check(fp["vtc:kit-orig/c-up-term-prefs"] == "kit-prefs", "kit: the kit's Prefs editor is kept in VTC:kit-orig")
fp2, _ = boot({**fp, "VTC:kit-restore": ""}, {"UP-TERM": "SYS:UP-Term"})
check(fp2["c:up-term-prefs"] == "kit-prefs", "kit-restore: the kit's Prefs editor is back")
fp3, reqp3 = boot({**BUILD, "VTC:upprefs": "new-prefs"})
check(fp3.get("c:up-term-prefs") == "new-prefs", "no kit: C:UP-Term-Prefs is installed from VTC:upprefs")
check(not reqp3, "no kit, Prefs: the boot asks for no volume (asked for %s)" % reqp3)

# W-R4: every C: command the kit installs is the build on a rig with no kit too (plain `UPDemo`
# ran an old C:UPDemo while the build sat in VTC:). The list is KIT_SHADOWS' C: rows.
CNAMES = [(src, dest) for src, dest, _ in rig.KIT_SHADOWS if dest.startswith("C:")]
check({d for _, d in CNAMES} >= {"C:UPDemo", "C:UPConsole", "C:vsh", "C:upgetty", "C:ixkill", "C:UP-Term-Prefs"},
      "KIT_SHADOWS names the kit's C: commands (%s)" % [d for _, d in CNAMES])
nk = {"VTC:" + src: "new-" + src for src, _ in CNAMES} | {"VTC:vtcon-handler": "new-handler"}
f4, req4 = boot(nk)
for src, dest in CNAMES:
    check(f4.get(dest.lower()) == "new-" + src, "no kit: %s is the VTC: build" % dest)
check("vtc:kit-orig/c-updemo" not in f4, "no kit: nothing to keep in VTC:kit-orig")
check(not req4, "no kit: the boot asks for no volume (asked for %s)" % req4)
# an old C:UPDemo that no kit put there (rig 1) is the build after the boot, the old one kept
f5, _ = boot({**nk, "C:UPDemo": "old-updemo"})
check(f5["c:updemo"] == "new-UPDemo" and f5["vtc:kit-orig/c-updemo"] == "old-updemo", "no kit, old C:UPDemo: the build replaces it, the old one is kept")
# what no kit holds beyond C: stays: no handler or device copy in SYS:
check("l:vtcon-handler" not in f4 and "devs:up-console.device" not in f4 and "l:pty-handler" not in f4, "no kit: L: and DEVS: get no copy")
# install puts every KIT_SHADOWS source in VTC:, also the ones the drawer never held
import inspect  # noqa: E402
check("KIT_SHADOWS" in inspect.getsource(rig.install), "install stages every KIT_SHADOWS source in VTC:")

# install and kit-restore (the host side)
with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / "vtc").mkdir()
    old = rig.RIG
    rig.RIG = tmp
    try:
        rig.kit_restore()
        check((tmp / "vtc/kit-restore").exists(), "kit-restore writes VTC:kit-restore")
    finally:
        rig.RIG = old

# W-T6: VTC:terminfo (TERMINFO on the rig) holds every entry the kit builds, vtcon-amiga included
import re  # noqa: E402
mk = (HERE.parent / "Makefile").read_text()
names = [pathlib.Path(f).stem for f in re.search(r"KIT_TERMINFO := (.*)", mk).group(1).split()]
check("vtcon-amiga" in names, "the kit builds vtcon-amiga")
with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    built = tmp / "kit-terminfo"
    for n in names:   # what make build/kit-terminfo/stamp leaves: first-letter and hex directories
        for d in (n[0], "%02x" % ord(n[0])):
            (built / d).mkdir(parents=True, exist_ok=True)
            (built / d / n).write_text("entry " + n)
    stale = tmp / "vtc-terminfo"   # the rig's old copy: vtcon and screen only
    for n, d in (("vtcon", "v"), ("vtcon", "76"), ("screen", "s")):
        (stale / d).mkdir(parents=True, exist_ok=True)
        (stale / d / n).write_text("old " + n)
    if hasattr(rig, "stage_terminfo"):
        rig.stage_terminfo(built, stale)
        for n in names:
            for d in (n[0], "%02x" % ord(n[0])):
                check((stale / d / n).is_file(), "VTC:terminfo/%s/%s is staged" % (d, n))
        check((stale / "v/vtcon").read_text() == "entry vtcon", "a stale entry is replaced by the build")
    else:
        check(False, "rig.stage_terminfo exists")

# rig 3, 2026-10-19: a stale VTC:vsh (09:54 build) went over the kit's fresh C:vsh at every boot
# after install_rig's install, so a kit check after the reboot ran the old vsh. The shadow sources
# are the current build: the stale one replaced, a missing one added, and prepare refreshes them.
with tempfile.TemporaryDirectory() as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / "build/amiga").mkdir(parents=True)
    (tmp / "vtc").mkdir()
    (tmp / "build/amiga/vsh").write_bytes(b"vsh 237268")
    (tmp / "build/amiga/UPDemo").write_bytes(b"updemo new")
    (tmp / "vtc/vsh").write_bytes(b"vsh 236784")
    (tmp / "vtc/gg/bin").mkdir(parents=True)
    (tmp / "vtc/gg/bin/sh").write_bytes(b"vsh 213744")
    old = rig.ROOT
    rig.ROOT = tmp
    try:
        if hasattr(rig, "refresh_kit_shadows"):
            rig.refresh_kit_shadows(tmp / "vtc")
            check((tmp / "vtc/vsh").read_bytes() == b"vsh 237268", "a stale VTC:vsh is replaced by the build")
            check((tmp / "vtc/UPDemo").read_bytes() == b"updemo new", "a missing VTC:UPDemo is added")
            # rig 3, 2026-10-24: VTC:gg/bin/sh (GG:bin/sh, the sh of ixemul programs) was an 8 Oct vsh whose
            # pipelines gave an ixemul child a non-interactive stdout (upkg-ui tty:programbox exit 3)
            check((tmp / "vtc/gg/bin/sh").read_bytes() == b"vsh 237268", "a stale VTC:gg/bin/sh is replaced by the build vsh")
        else:
            check(False, "rig.refresh_kit_shadows exists")
    finally:
        rig.ROOT = old
import ast  # noqa: E402
prep = next(f for f in ast.walk(ast.parse((HERE / "rig/install_rig.py").read_text()))
            if isinstance(f, ast.FunctionDef) and f.name == "prepare")
check(any(isinstance(c, ast.Call) and isinstance(c.func, ast.Attribute) and c.func.attr == "refresh_kit_shadows"
          for c in ast.walk(prep)), "install_rig.prepare refreshes the kit shadows in VTC:")

print("[%s] rig_kit: %d failed" % ("FAIL" if fails else "OK", fails))
sys.exit(1 if fails else 0)
