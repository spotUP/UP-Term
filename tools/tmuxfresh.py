#!/usr/bin/env python3
"""Refuse a kit tmux binary older than the tmux-amiga sources (TMUX-KIT-REFRESH, 2026-11-02).

The kit copied tmux-amiga/build/tmux-bin as it stood; a 17 Oct binary shipped without the pane console fix
(tmux-amiga 4beb716) because nothing compared it with the sources. The binary is stale when the last commit
touching the build inputs is newer than it, or a tracked build input has uncommitted changes or a newer mtime.

usage: tmuxfresh.py <tmux-amiga dir> <tmux-bin>     exit 0 fresh, 1 stale or missing
"""
import pathlib, subprocess, sys

INPUTS = ("Makefile.amiga", "amiga", "tmux", "libevent")


def stale_reason(repo, binary):
    repo, binary = pathlib.Path(repo), pathlib.Path(binary)
    if not binary.is_file():
        return f"{binary} missing"
    built = binary.stat().st_mtime

    def git(*a):
        return subprocess.run(["git", "-C", str(repo), *a], capture_output=True, text=True, check=True).stdout.strip()

    last = git("log", "-1", "--format=%ct %h", "--", *INPUTS).split()
    if last and float(last[0]) > built:
        return f"{binary} is older than tmux-amiga commit {last[1]}"
    dirty = [l[3:] for l in git("status", "--porcelain", "--untracked-files=no", "--", *INPUTS).splitlines()]
    if dirty:
        return f"tmux-amiga has uncommitted changes: {' '.join(dirty)}"
    return None


if __name__ == "__main__":
    why = stale_reason(sys.argv[1], sys.argv[2])
    if why:
        print(f"dist: stale tmux: {why}: make -f Makefile.amiga in {sys.argv[1]}")
        sys.exit(1)
