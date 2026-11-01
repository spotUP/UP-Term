"""The rig's kit shadow stamp (RIG-SLOW-BOOT, owner 2026-10-29).

BOOTX:go puts every KIT_SHADOWS file from VTC: over its kit place on each boot: an If EXISTS and a Copy through
the UAE host filesystem per row, about 1400 round trips on rig 9, which took ~10 minutes of a warm reboot under
host load (2026-10-28). DH0: keeps what the last pass wrote across a warm reboot, so the pass only has to run
again when the disk or the files it copies changed.

The host writes VTC:kit-hash: an md5 over BOOTX:go and every VTC: file the shadow rows copy (named in
BOOTX:kit-shadows.list, which rig.py writes beside go). go copies it to VTC:kit-shadowed after a full pass and
skips the pass while VTC:kit-shadowed exists. The host deletes VTC:kit-shadowed when it no longer matches
(a changed kit or go) and when DH0: is new (drop), so a changed kit still shadows on the next boot.
check runs at each rig setup and install and before ami.py reboot; a C:Reboot typed on the Amiga after
copying a new kit file into VTC: by hand skips the pass, as nothing on the host saw the change."""
import hashlib

LIST = "kit-shadows.list"   # in BOOTX:
HASH = "kit-hash"           # in VTC:, written by the host
STAMP = "kit-shadowed"      # in VTC:, written by go after a full pass


def digest(boot, vtc):
    """md5 over BOOTX:go and each listed VTC: file (a missing file counts as missing, not as empty)."""
    h = hashlib.md5((boot / "go").read_bytes())
    for name in (boot / LIST).read_text().splitlines():
        if name:
            p = vtc / name
            h.update(name.encode() + b"\0" + (b"+" + p.read_bytes() if p.is_file() else b"-") + b"\0")
    return h.hexdigest()


def drop(vtc):
    """The next boot runs the full shadow pass (a new DH0:, a kit-restore)."""
    (vtc / STAMP).unlink(missing_ok=True)


def check(boot, vtc):
    """Write VTC:kit-hash for the current go and kit; drop the stamp unless it matches. Returns True when the
    next boot skips the shadow pass."""
    if not (boot / "go").is_file() or not (boot / LIST).is_file():
        drop(vtc)
        return False
    d = digest(boot, vtc)
    (vtc / HASH).write_text(d + "\n")
    stamp = vtc / STAMP
    if stamp.is_file() and stamp.read_text().strip() == d:
        return True
    drop(vtc)
    return False
