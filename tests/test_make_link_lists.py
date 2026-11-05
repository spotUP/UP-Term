#!/usr/bin/env python3
"""Link lists carry the provider of every symbol leaf_core.c needs.

amarchy/leaf_core.c calls gte_dirty, gte_row and gte_row_label, defined in
amarchy/gthemes_core.c. build/amiga/wbrun and build/amiga/jobprobe linked
leaf_core.c without it, so `make rig-helpers` failed with "undefined symbol
_gte_dirty". This test expands the real link recipes with `make -n -B` (variables
resolved, nothing compiled) and checks, for each (user, provider) pair in PAIRS,
that every link line naming the user also names the provider exactly once.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
TARGETS = ["build/amiga/" + t for t in ("Amarchy", "amarchy-menu", "amarchy-gui", "jobprobe", "wbrun")]
PAIRS = [("amarchy/leaf_core.c", "amarchy/gthemes_core.c")]
MIN_LINES = 5


def link_lines():
    out = subprocess.run(["make", "-n", "-B"] + TARGETS, cwd=ROOT, capture_output=True,
                         text=True).stdout
    lines = []
    for ln in out.splitlines():
        m = re.search(r" -o (\S+)", ln)
        if m and ".c" in ln and not re.search(r" -c ", ln):
            lines.append((m.group(1), ln.split()))
    return lines


lines = link_lines()
bad = []
found = {}
for user, provider in PAIRS:
    users = [(t, w) for t, w in lines if user in w]
    found[user] = len(users)
    if len(users) < MIN_LINES:
        bad.append("only %d link lines name %s, need %d (parser found nothing?)"
                   % (len(users), user, MIN_LINES))
    for target, words in users:
        n = words.count(provider)
        if n == 0:
            bad.append("%s links %s without %s" % (target, user, provider))
        elif n > 1:
            bad.append("%s names %s %d times (double definition)" % (target, provider, n))
if bad:
    print("[FAIL] make_link_lists:\n  " + "\n  ".join(bad))
    sys.exit(1)
print("[OK] make_link_lists: %d link lines name leaf_core.c, each with gthemes_core.c once"
      % found[PAIRS[0][0]])
