# RIG97-DEFICONS-BAD-TEMPLATE hunt (vtcon-ee, 2026-11-06)

Status: NOT reproduced. Cause unknown; leads below. Rig 99 (bare `--os 32 --max`, main 5cf53949 build, --allow-stale-base), stopped and cleared.

## Text source (host-side, read from the bare 3.2 tree)
- The text exists in two 3.2 binaries only: `C:DefIcons` ($VER: deficons 47.8 (16.7.2020)) and `Prefs/DefaultIcons`,
  both as "DefIcons: Cannot load preferences because" (+ Fault text, here "bad template" = ERROR_BAD_TEMPLATE 114).
- Both read `ENV:DefIcons.prefs` (DefaultIcons also `ENVARC:DefIcons.prefs`), a 2752-byte IFF file shipped in the
  bare image's ENVARC (Prefs/Env-Archive/deficons.prefs). Not ENV:Sys/def#?.prefs: the rig 97 probe looked in the
  wrong place (it never listed ENV:DefIcons.prefs). Repo (tools, Makefile, amarchy, installer) has no DefIcons reference.
- C:DefIcons strings: `CON:////DefIcons/AUTO/CLOSE/WAIT`, `ENV:DefIcons.prefs`, `QUICKSCAN`, commands `Identify|Flush|Quit`.

## Rig 99 facts (bare boot, nothing touched)
- No requester; windows: only "Workbench".
- A `DefIcons` process runs with no CLI after boot, though no S: script, WBStartup entry or prefs names it
  (grep of the whole tree finds DefIcons only in C/DefIcons and Prefs/DefaultIcons): the ROM / Workbench starts it.
- ENV: lists DefIcons.prefs 2752 (13-Apr-21) via the 3.2 `MakeLink RAM:ENV to ENVARC: FORCE` overlay.
- Running `C:DefIcons` again prints "DefIcons removed." (toggle); running it once more re-installs it as
  "Background CLI (C:DefIcons)". Neither showed the error.

## Tried, no requester
`SetEnv foo bar`; `VTC:vsh -c "Set v=hi"` (R7); `C:DefIcons ?`, `C:DefIcons`; overwriting ENV:DefIcons.prefs with
"x" then restoring it (no reload requester: DefIcons does not watch the file, or reloads quietly).

## Not done (budget)
Full R-check replay (rcheck.py lived in the rig 97 session's scratchpad). Candidates: R3/R6 (`.KEY w/A`, `execute file ?`,
ReadArgs failures that leave IoErr 114 in a process), the `--os 31` reboot of the same rig, Prefs/DefaultIcons opened.

## Leads
1. Error path is a prefs (re)load with a stale IoErr: the only ReadArgs-style 114 source in the R-checks is the
   template failures (R3 "required argument missing", R6 `?`). Next: replay R1-X3 in order on a bare 3.2 rig, `wasabi windows` after each.
2. Fix owner: 3.2 itself (C:DefIcons/ROM) unless the replay shows vsh launching DefIcons or writing ENV:DefIcons.prefs;
   rig image tools/rig ships the stock ENVARC file unchanged.
