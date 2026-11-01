---
date: 2026-10-26
topic: Amarchy Style split and Background-with-no-terminal on main and on the A1200; Prefs theme picker on prefs-inline
branch: main (48006aec), prefs-inline (60d97653)
---

# Handoff: Amarchy Style split, AM-BG-NOTERM, Prefs theme picker

Ledger: thoughts/spot/todos.md (rows dated 2026-10-26 starting AM-, A1200 DEPLOY, RIG-1 INCIDENT).
Plans (untracked, edit in place): thoughts/shared/plans/2026-10-26-amarchy-prefs-inline.md, 2026-10-26-amarchy-gui-themes.md.

## State

- main 48006aec: Style menu split (27eda4b7) + ask-then-open for Style > Terminal > Background (48006aec). Rig 7 PASS for both (scratchpad style-rig.md, bgnoterm-rig.md).
- A1200 runs 48006aec builds of Amarchy, amarchy-gui, amarchy-index, amarchy-menu + dist/keymap (backups .bak-20261026n; drift 0). Owner retest pending after reboot: T19 (Left Amiga + Space, Style, Amarchy theme, Enter: list of 23 themes, current marked, Enter applies) and Style > Terminal rows with no terminal open.
- amarchy-menu builds from vtcon alone with UPTERM_ROOT=/Users/spot/Code/UP-Term (libupkgui.a from upterm-ports/build/sysroot). The earlier note that it could not be deployed was wrong.
- prefs-inline (origin, 60d97653): T1-T4, shared conf index, Menu theme list picker (rig 7 PASS, c942120a), gthemes DOS split. Not merged to main, so upprefs is not on the A1200.
- a1200_drift.py lists installed programs that were not built (e38dd8ac).

## Next

1. Owner result of the A1200 retest; on FAIL, repro on rig 7 first.
2. prefs-inline M1-M10, C1, R1-R2 (plan sections "Design", "Checklist"), one sonnet agent at a time; then merge prefs-inline to main and deploy upprefs.

## Gotchas

- Rig number comes only from UPTERM_RIG=<n>; a positional number hits rig 1. Worktree rigs need UPTERM_ROM, UPTERM_SYSTEM_HDF, IXEMUL, vendor/amissl-5.27 symlink, and a copy of the main checkout's build/wasabi. rig.py install fails in a worktree without build/amiga/vsh; copy files by hand.
- Rig 7 has no UP-Term:Amarchy/Themes drawer: stage dist/amarchy-themes.
- Main checkout stays untouched (old main, another session's rig.py diff, stale build/).
- One ami.py put printed "ERR: rename into place failed" while the readback matched; always verify deploys by md5 readback + a1200_drift.
