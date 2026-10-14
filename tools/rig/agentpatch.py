"""The rig's amiagent: where it comes from and the --stock stack patch.

The rig runs the 68020 build of amiagent 0.13.2 (Aminet comm/net/amiagent,
vendored in tools/rig/amiagent-0.13.2, SOURCE.txt has the sha256s). The agent
gives every command process a 256 KB stack (NP_StackSize 262144); on the 2 MB
--stock machine that took an eighth of memory per command, so --stock runs a
copy with 16384 there (a stock Shell has 4 KB; 16 KB keeps Avail honest).

The constant sits in two places, found by the tag NP_StackSize (0x800003f3)
followed by 262144, not by a bare offset:
  move.l #$800003f3,(a0)+ ; move.l #262144,(a0)+   (the value: tag+6)
  a TagItem { 0x800003f3, 262144 }                  (the value: tag+4)
In amiagent.020 0.13.2 the values are at 0x744 and 0x13e88 (the 68000 build:
0x774 and 0x1509c). The patch refuses any build whose sha256 is not listed
here: a new amiagent gets its offsets looked at and listed, never guessed."""
import hashlib, pathlib, re

DIST = pathlib.Path(__file__).resolve().parents[2] / "tools/rig/amiagent-0.13.2"
SOURCE = DIST / "amiagent.020"     # what the rig installs as boot/amiagent

# sha256 of the build -> the offsets of the 262144 values
BUILDS = {
    "de6ad5acd4ee88930bda25ce865773c08583d74639f88447d43b688bce04c0ac": (0x744, 0x13e88),   # 0.13.2, 68020
}
STOCK_STACK = 16384
_TAG = re.compile(rb"\x80\x00\x03\xf3(?:\x20\xfc)?\x00\x04\x00\x00")


def stack_offsets(data):
    """The two places holding 262144 after NP_StackSize, by the tag."""
    return [m.end() - 4 for m in _TAG.finditer(data)]


def stock_patch(data):
    """data with 16384 in both places; SystemExit for a build not in BUILDS or
    whose tag search does not land on the listed offsets."""
    digest = hashlib.sha256(data).hexdigest()
    if digest not in BUILDS:
        raise SystemExit(f"rig: amiagent sha256 {digest} is not the 0.13.2 68020 build; "
                         "the --stock patch needs its offsets looked at (tools/rig/agentpatch.py)")
    offs = stack_offsets(data)
    if tuple(offs) != BUILDS[digest]:
        raise SystemExit(f"rig: amiagent has NP_StackSize 262144 at {[hex(o) for o in offs]}, "
                         f"expected {[hex(o) for o in BUILDS[digest]]}")
    a = bytearray(data)
    for off in offs:
        a[off:off + 4] = STOCK_STACK.to_bytes(4, "big")
    return bytes(a)


def install(dest):
    """Copy the vendored build to dest (the rig's boot/amiagent) unless it is already that."""
    src = SOURCE.read_bytes()
    if not dest.exists() or dest.read_bytes() != src:
        dest.write_bytes(src)
    dest.chmod(0o755)
