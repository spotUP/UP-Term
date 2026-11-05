#!/usr/bin/env python3
"""Reachability rig for the Amarchy Theme Editor (plan 2026-11-05-amarchy-theme-editor.md).

TE-1 skeleton: start the rig, open the editor page, read the sentinels. Each later phase adds one --case that drives
amarchy-gui through its real entry point (Left Amiga + Space > Settings > Theme Editor), makes the edit by keys, reads the
theme file back from ENVARC:Amarchy/themes/, and checks desktop pixels or pens.

Rig start (owner rule): UPTERM_RIG=<n> rig.py start --max --purpose "<session>: theme editor <case>"
"""
import argparse
import sys

CASES = {}


def case(name):
    def deco(fn):
        CASES[name] = fn
        return fn
    return deco


@case("open")
def case_open(args):
    """TE-1 skeleton: the editor page opens and its sentinels read back. Filled in by TE-3 (the page exists then)."""
    print("[INFO] open: page not built yet (TE-3); skeleton only")
    return 2


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--case", choices=sorted(CASES), default="open")
    ap.add_argument("--rig", type=int, default=0, help="rig number, passed as UPTERM_RIG by the caller")
    args = ap.parse_args(argv)
    return CASES[args.case](args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
