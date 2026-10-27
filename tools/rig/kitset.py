"""The set of files the kit installs on a rig, DERIVED from what installs them (RIG-KIT-DERIVE, owner 2026-10-28).
Invariant: every file the kit installs on a rig equals what the current build would install. The set is
  - dist/install.dos Copy lines whose sources are build/amiga programs or dist/ files (a1200_drift.install_map),
  - the Unix userland the kit ships in UP-Term:bin (build/dist/up-term/Files/userland/bin),
  - the early-loaded libraries the rig stages (ixemul/ixnet from paths.*, the vendor AmiSSL of stage_amissl),
nothing hand-listed. rig.KIT_SHADOWS (the boot's shadow rows) and rig.sys_hdf_kit (the clone refresh and its md5
check) both read it, so a file the install gains is covered at once."""
import pathlib, re
import a1200_drift

# Amiga volume -> place in the DH0: clone (None: not on the system disk; the boot shadows it from VTC:)
CLONE_DIRS = {"C:": "C/", "L:": "L/", "DEVS:": "Devs/", "LIBS:": "Libs/"}
# the boot spells these places by path (go runs before the Startup-Sequence assigns ENVARC:/ENV:)
SPELL = {"ENVARC:": "SYS:Prefs/Env-Archive/", "ENV:": "RAM:Env/"}

class Entry:
    def __init__(self, dest, src, clone=None, check=None, shadow=True, vtc=None, bak=None):
        self.dest, self.src, self.clone, self.check, self.shadow, self.vtc = dest, pathlib.Path(src), clone, check, shadow, vtc
        self._bak = bak
    @property
    def bak(self):
        return self._bak or re.sub(r"[:/]", "-", self.dest)

def clone_path(dest):
    for vol, d in CLONE_DIRS.items():
        if dest.startswith(vol):
            return d + dest[len(vol):]
    if dest == "ENVARC:up-term/keymap":
        return "Prefs/Env-Archive/up-term/keymap"
    return None

def derive(root, userland_src, userland_stage, vendor, ixemul, ixnet, m060, ixnet_check):
    root = pathlib.Path(root)
    built = root / "build/amiga"
    names = [f.name for f in built.iterdir() if f.is_file()] + list(a1200_drift.DIST_SOURCES)
    out, seen = [], set()
    def add(e):
        if e.dest not in seen:
            seen.add(e.dest)
            out.append(e)
    for name, dest in a1200_drift.install_map((root / "dist/install.dos").read_text(), names):
        src = root / ("dist" if name in a1200_drift.DIST_SOURCES else "build/amiga") / name
        spelled = dest.startswith(tuple(SPELL))   # the boot never names ENVARC:/ENV: (a System Request hangs it)
        add(Entry(dest, src, clone_path(dest), shadow=not spelled, vtc=name))
        for vol, sp in SPELL.items():     # the boot also shadows the spelled places (the old rows' ENV/ENVARC pair)
            if dest.startswith(vol):
                add(Entry(sp + dest[len(vol):], src, None, vtc=name, bak=re.sub(r"[:/]", "-", dest)))
    if pathlib.Path(userland_src).is_dir():
        for f in sorted(pathlib.Path(userland_src).iterdir()):
            if f.is_file() and re.fullmatch(r"[A-Za-z0-9._+-]+", f.name):   # "[" is an AmigaDOS pattern
                add(Entry("UP-Term:bin/" + f.name, f, None, vtc="%s/%s" % (userland_stage, f.name)))
    add(Entry("Libs/ixnet.library", ixnet, "Libs/ixnet.library", ixnet_check, shadow=False))
    add(Entry("Libs/ixemul.library", ixemul, "Libs/ixemul.library", shadow=False))
    amis = pathlib.Path(vendor) / "amissl-5.27/AmiSSL/Libs/AmigaOS3"
    add(Entry("AmiSSL/Libs/amisslmaster.library", amis / "amisslmaster.library", "AmiSSL/Libs/amisslmaster.library", shadow=False))
    add(Entry("AmiSSL/Libs/AmiSSL/amissl_v362.library", amis / "AmiSSL" / ("68060" if m060 else "68020-40") / "amissl_v362.library",
              "AmiSSL/Libs/AmiSSL/amissl_v362.library", shadow=False))
    return out
