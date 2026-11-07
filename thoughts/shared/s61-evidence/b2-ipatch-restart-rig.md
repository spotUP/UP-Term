# B2-RIG: Amarchy restart series over the hardened ipatch gates (2988e11c)

Rig 96 (`--max --os 32 --mmu`, `muforce on`), HEAD 1bf128ca (contains 2988e11c). Script: tools/rig/amarchy_restart_rig.py
(`--only a,b`, `--failed`, `--fresh`, `--no-build`; verdicts in build/rig-reports/rig96/amarchy_restart/). The Amarchy
build was current on the rig (stage_rig: current, SHA verified against the host build).

## Rows (14 of 14 ok)
- round_1..round_10: all ok. Each: Run Amarchy, port AMARCHY up, STATUS answered
  `hits=0 rows=71 enabled=1 active=0 wins=0`, QUIT, port gone, 2 s pause.
  Timings (port up / port gone seconds): 0.8/1.4, 0.8/1.2, 0.8/1.2, 0.8/1.2, 0.8/1.4, 0.8/1.5, 0.8/1.4, 0.8/1.4, 0.8/1.2, 0.8/1.2.
- no_guru: ok, `no Guru recorded (T:lastguru absent)`.
- no_muforce_hit: ok, `rig.py muforce log` = `no hits` (0 lines for Amarchy).
- no_busy_wait_sentinel: ok. AMARCHY_WBRESET_BUSY_WAIT, AMARCHY_STRUT_BUSY_WAIT, AMARCHY_BORDERBLANK_BUSY_WAIT all unset
  (GetEnv rc != 0; names from amarchy/{wbreset,strut,borderblank}_patch.c). Positive control: AMARCHY_BORDERBLANK_INSTALLED
  rc=0 value '0' (the remove wrote it), so GetEnv does see sentinels.
- patch_chain_restored: ok. No Amarchy task in `wasabi ps`; `Run Clock` opened a window (seen after 0.3 s, OpenWindow
  through the restored chain), closed by `Break <proc> C` after 0.1 s; no MuForce hit, no Guru.

## History
The first full run gave 12 of 14: no_busy_wait_sentinel and patch_chain_restored FAILed because of script bugs, not
the product: (1) unset GetEnv prints `object not found` and I counted that text as "set"; now the row uses the return
code plus a positive control; (2) `rig.py wasabi windows` does not exist; now ami.list_windows. After the fix both rows
passed against the same unchanged rig state (rounds were not re-run; their verdicts stand from the first run).

## Not wired
No tools/rig/*_rig.py script (amarchy_rig.py, amarchy_bar_rig.py included) is wired in the Makefile (grep of
`amarchy_rig|amarchy_bar_rig|bar_rig` in Makefile found nothing), so no Makefile line was added.

## Files
tools/rig/amarchy_restart_rig.py (new), this file. No source edited.
