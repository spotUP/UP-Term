#!/usr/bin/env python3
"""A failed recipe must not leave its target behind (M2).

build/amiga/wbrun was once a 0-byte leftover of a failed build that make then
treated as up to date. .DELETE_ON_ERROR makes make delete a target whose recipe
failed. Checks: (1) a recipe that writes its target and then fails leaves no
file under a makefile with the line, and does leave it without (the control);
(2) every Makefile named on the command line, or vtcon's own by default, has the
line as a rule of its own at column 0.
"""
import os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
MAKEFILES = sys.argv[1:] or [os.path.join(HERE, "..", "Makefile")]
RULE = "out:\n\t@echo partial > out\n\t@false\n"


def run(text):
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "Makefile"), "w") as f:
            f.write(text)
        r = subprocess.run(["make", "-s", "-C", d, "out"], capture_output=True)
        return r.returncode, os.path.exists(os.path.join(d, "out"))


rc, left = run(".DELETE_ON_ERROR:\n" + RULE)
assert rc != 0 and not left, "make kept the target of a failed recipe despite .DELETE_ON_ERROR"
rc, left = run(RULE)
assert rc != 0 and left, "control: without the line make keeps the partial target"
for m in MAKEFILES:
    with open(m) as f:
        assert re.search(r"^\.DELETE_ON_ERROR:", f.read(), re.M), m + " lacks .DELETE_ON_ERROR:"
print("[OK] make_hygiene: .DELETE_ON_ERROR removes a failed recipe's target; present in %d Makefile(s)" % len(MAKEFILES))
