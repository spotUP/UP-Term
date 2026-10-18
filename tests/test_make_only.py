#!/usr/bin/env python3
"""make test ONLY=<name> runs that suite and only that suite (make_hygiene).

The recipe once kept a hand-written list of the names that are not C suites, to keep
vttest_host from being asked for them. The list fell behind: ONLY=packages, ONLY=addons
and ONLY=vshrc_net failed with "[ERROR] no suite named", and ONLY=hl skipped the C suite
hl. Now vttest_host answers --has itself and the other names come from the recipe lines.
Checks: a name that only a recipe line knows runs and passes; a C suite that also has a
recipe line runs both; a name nobody knows fails.
"""
import os, subprocess, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ENV = {k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}


def make_test(name):
    r = subprocess.run(["make", "-s", "-C", ROOT, "test", "ONLY=" + name],
                       capture_output=True, text=True, env=ENV)
    return r.returncode, r.stdout + r.stderr


rc, out = make_test("vshrc_net")
assert rc == 0 and "no suite named" not in out, "ONLY=vshrc_net (a recipe line only):\n" + out
rc, out = make_test("hl")
assert rc == 0 and "] hl: " in out, "ONLY=hl runs the C suite hl as well:\n" + out
rc, out = make_test("no_such_suite")
assert rc != 0 and "[ERROR] make test: no suite named no_such_suite" in out, \
    "ONLY=no_such_suite must fail:\n" + out
print("[OK] make test ONLY= picks its suites")
