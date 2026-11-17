# VANTABLACK-BOOT-STOCK-PENS on rig 68: NOT REPRODUCED (3 boots)

Question: does a boot with `amarchy-gui-theme = vantablack` leave stock Workbench pens, and where does the desk apply stop?

## Steps run (rig 68 only; read-only for source)
1. Recorded state (prefs, `amarchy-flat STATUS`, `dripens Workbench` = DrawInfo pens + RGB, palette.prefs bytes).
2. Backed up ENVARC/ENV Amarchy prefs and Sys palette/wbpattern/font to the session scratchpad (bak/).
3. Wrote `amarchy-gui-theme = vantablack` to ENVARC:Amarchy/prefs and ENV:Amarchy/prefs.
4. Boot A: `ami.py reboot` (RAM wiped). Boot B: `rig.py reboot` (stop + start --keep-sys, a real emulator restart; no clone, so the installed kit stays). Boot C: same as B after putting the TOKYO palette.prefs into ENVARC:Sys/palette.prefs (the transition case: saved palette differs from the theme being applied; the rig's ENVARC palette was already vantablack from yesterday's applies, so A and B did not exercise a palette change).
5. Restored prefs + palette, `rig.py reboot`, verified.

## Measured
Stock pens would read `0 1 1 2 1 3 1 0 2 1 2 1` (tests/data/desk palette-gui-save.prefs).

| item | before (tokyo-night) | vantablack boots A, B, C | after restore |
|---|---|---|---|
| prefs line | tokyo-night | vantablack | tokyo-night |
| DrawInfo pens 0..11 | 0 1 2 0 0 3 0 0 2 2 0 2 | identical | identical |
| RGB of pens 0,1,2,5 | 1A1B26 414868 A9B1D6 7AA2F7 | 000000 7A7A7A BEBEBE 8D8D8D | as before |
| STATUS | flat=on calls=16, pens text=2 line=3 back=0 outline=3 button=4 | flat=on, same pens line (calls 38/51/38) | flat=on calls=138, same pens line |
| ENV:Sys/palette.prefs md5 | 5603c343... (tokyo) | vantablack colours written | 5603c343... (= backup) |
| ENVARC:Sys/palette.prefs md5 | 6f96d673... | vantablack | 6f96d673... (= backup) |

Boot C started with a tokyo ENVARC palette and still ended with the vantablack flat desk (pens flat, RGB 000000/7A7A7A/BEBEBE/8D8D8D): the apply ran and wrote the palette.

## Verdict: NOT REPRODUCED
The desk apply completes for vantablack in all three boots. No failing step found; no file:line to name. Code read confirms the vantablack special cases are not on any early-return path: gthemes_core.c:207 only swaps a pure #ffffff fg for BEBEBE (the BEBEBE seen on the rig); flat_core.h:492 is a comment on the knob mix. Boot path: amarchy-flat main (flat.c:3381-3388: gt_dos_load_current + desk_flat_full) and desk_dos_start_apply (desk_dos.c:1650; early return at ~1666 only if mask and flat both 0).

## Unverified / hypotheses for the one-off sighting
- Seen once, never reproduced: likely a timing/race (desk_start_gate in amarchy.c:290-308 waits for the screen and desk_dos_wb_ready; a boot checked before the apply and IPrefs reset finished would show stock pens). I did not test an early read; my reads came 60+ s after boot.
- Not tested: a boot with no ENVARC palette at all, or with the prefs line edited while Amarchy runs (amarchy_p2_rig.py:326 path), or OS 3.1 rigs.
- Proposed violated invariant (unproven): the desk apply on boot must finish before the screen is considered ready; if it can lag, "stock pens" is a transient, not a failed apply.

## Rig 68 end state
Restore DONE and VERIFIED: prefs tokyo-night in ENVARC and ENV, palette files byte-equal (md5) to the backups, flat tokyo-night desk (pens and RGB as in step 1), STATUS flat=on. The fs-uae process was restarted (new pid, was 47499); rig purpose string now the one from start.args ("s71 session: restore flat desk, path text census"). RAM:dripens, RAM:p.bak etc. are gone with RAM; ENVARC:Amarchy/prefs.s72bak deleted. Scratch left: bak/ and small logs in the scratchpad (well under 10 MB).
