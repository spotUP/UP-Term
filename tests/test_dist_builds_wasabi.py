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
