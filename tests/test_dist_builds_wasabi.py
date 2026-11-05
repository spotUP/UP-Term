#!/usr/bin/env python3
"""make dist builds wasabi itself (make_hygiene).

dist-base and dist-claude copy wasabid, the client and wasabi_mcp.py from build/wasabi
but only tested for them, so a fresh worktree stopped with "make wasabi first". They now
name the wasabi stamp of the pinned revision (tools/nas/wasabi.rev) as a prerequisite, as
dist-base names bebbossh. Checks make's own rule table, not the Makefile text.
"""
import os, subprocess

ROOT = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
ENV = {k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}
with open(os.path.join(ROOT, "tools", "nas", "wasabi.rev")) as f:
    STAMP = "build/wasabi/.stamp-" + f.read().strip()

r = subprocess.run(["make", "-pq", "-C", ROOT, "--no-print-directory", "dist-base"],
                   capture_output=True, text=True, env=ENV)
rules = {}
for l in r.stdout.splitlines():
    if l.startswith(("dist-base:", "dist-claude:")):
        name, deps = l.split(":", 1)
        rules[name] = deps.split()
for target in ("dist-base", "dist-claude"):
    assert target in rules, target + ": no rule in make -p"
    assert STAMP in rules[target], target + " does not build " + STAMP + ": " + " ".join(rules[target])
print("[OK] dist-base and dist-claude build wasabi " + STAMP.rsplit("-", 1)[1])

# Local wasabi fixes (tools/nas/wasabi-patches) survive a re-clone: the stamp rebuilds when a patch changes, and the
# recipe applies every patch after the checkout. A re-clone without them lost the MUI AAText labels (Selecta I1092d).
MAKEFILE = os.environ.get("WASABI_TEST_MAKEFILE", "Makefile")
PATCHES = sorted("tools/nas/wasabi-patches/" + n for n in os.listdir(os.path.join(ROOT, "tools", "nas", "wasabi-patches"))
                 if n.endswith(".patch"))
assert PATCHES, "no wasabi patches found"
r = subprocess.run(["make", "-pq", "-f", MAKEFILE, "-C", ROOT, "--no-print-directory", STAMP], capture_output=True, text=True, env=ENV)
deps = next((l.split(":", 1)[1].split() for l in r.stdout.splitlines() if l.startswith(STAMP + ":")), [])
for p in PATCHES:
    assert p in deps, STAMP + " does not depend on " + p + ": " + " ".join(deps)
r = subprocess.run(["make", "-n", "-B", "-f", MAKEFILE, "-C", ROOT, "--no-print-directory", STAMP, "AMISSL_SDK=/nonexistent"],
                   capture_output=True, text=True, env=ENV)
checkout_at = r.stdout.find("checkout -q")
apply_at = r.stdout.find(" apply ", checkout_at)
assert checkout_at >= 0 and apply_at > checkout_at, "the wasabi recipe applies no patch after the checkout:\n" + r.stdout
print("[OK] wasabi applies %d local patch(es) after the checkout" % len(PATCHES))

# Patch 0002 (Selecta I1092d): an AAText inside a container group is a label, not a dropped child of a "group button".
# It must exist, apply on a copy of the pinned source after 0001, and keep its marker.
P2 = "tools/nas/wasabi-patches/0002-mui-aatext-in-container.patch"
assert P2 in PATCHES, "patch 0002 missing"
assert PATCHES.index(P2) > PATCHES.index("tools/nas/wasabi-patches/0001-mui-aatext-label.patch"), "0002 must sort after 0001"
with open(os.path.join(ROOT, P2)) as f:
    assert "aat = TRUE;" in f.read(), "patch 0002 lost its AAText label marker"
print("[OK] patch 0002 present, ordered after 0001")
