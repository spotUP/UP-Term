# S61 Apply to Desktop freeze hunt (AmigaOS 3.2 rig)

Rig 171, `--max --os 32`, 060, worktree vtcon-te10-8 branch te9-1 at eec3bc56 (started with
`--allow-stale-base`, because origin/main d37dc08e was 1 commit ahead). Date: 2026-11-06. Two rig boots were used.

## Verdict: cause UNPROVEN

The freeze is not a CPU crash. It looks like a blocked task that holds a lock wasabid later needs.
It is also not specific to Apply: boot 2 froze at Amarchy start during `--setup`, with no Apply.

## Evidence

Boot 1 ran `amarchy_gui_rig.py --setup`, then `amarchy_theme_editor_rig.py --case te91` (Apply to Desktop).
- At Apply + 4 s, sentinel AMARCHY_EDIT_APPLIES = None, while the daemon still answered.
- 16:23:40: `wasabi ps` was OK and listed every task in wait. amarchy-gui (CLI 1) showed stack 262144 and FREE 260882.
- 16:24:01: `run GetEnv` returned "daemon did not answer for 15 s".
- 16:24:16 (debug.log): "nothing heard for 20 s - the machine is frozen, rebooted, or busy with something long".
- build/rig171/logs/fs-uae.log.txt had 0 lines of `IDE1 DATA write without request` / `PC=0000`.
  Compare 3.1 (s32-evidence/31-freeze-culprit.md), where the CPU ran wild at PC $7A. That did not happen here.
- The host fs-uae process stayed at about 53 % CPU, and TCP accept still worked.

Boot 2 was a fresh boot, then `amarchy_gui_rig.py --setup --no-build`.
- "ok setup: the earlier amarchy-flat resident unloaded on Break", then the kit was copied.
  md5 was skipped because VTCX:Apps/bin/md5sum is missing on 3.2.
- Then wasabi TimeoutError at 16:28:52 in `start_amarchy` (tools/rig/amarchy_gui_rig.py:1307):
  `c.run('Run >NIL: %s/Amarchy' % SETUP_BIN, 20)`.
- debug.log: last line heard at 16:26:19, then "nothing heard for 20 s" at 16:28:52.

## Candidates

- C1, lock-order deadlock between `r->ss` (pen record) and the LockPubScreenList taken by amarchy-flat
  draw_sync: RULED OUT by code read.
  - flat.c:1812-1832 draw_sync takes the list lock, then calls ipd_epoch / ipd_epoch_of_screen.
    iconpen_dos.c:431 and :444 use Forbid + FindSemaphore only and never take `r->ss`.
  - Every `r->ss` user takes `r->ss` first and the list lock second:
    pens_sync near :346, ipd_recolour :514-544, ipd_role_write :546-563.
  - The icl_ss order in pens_reap (flat.c:1795) and icl_pens_release (flat.c:704) is reversed, but both run
    in the same flat main task. The Text hook (flat.c:848) only uses AttemptSemaphore.
- C2, palette write (desk_dos_apply, Palette / DT_PALETTE, ipd_recolour): OPEN.
  It is not a CPU crash on 3.2 (no wild PC). It may still be the step that blocks.
  Boot 2 froze with no palette write, at an Amarchy start, which points at a shared step instead.
- C3, amarchy-flat unload or patch removal on Break: OPEN.
  Break unloaded it cleanly ("ok"). The freeze came at the next Amarchy start, after the new amarchy-flat was copied.
- C4, stack overflow: RULED OUT.
  ps after Apply showed FREE 260882 of 262144 in amarchy-gui, and the frame sizes are small.
  Note: flat.c has `__stack = 16384`.

## Smallest reproduction (from boot 2)

1. Fresh rig 171 `--os 32 --max`.
2. Run `amarchy_gui_rig.py --setup --no-build`.
3. The guest stops answering wasabi at `Run >NIL: <SETUP_BIN>/Amarchy` (amarchy_gui_rig.py:1307).

## Next probe (not run, boot budget and tool budget spent)

- Before step 3, start `wasabi snoop --entry --task 'Amarchy*' --ignore-wasabi --log <scratch>` in the
  background with `--for 120`. The last `->` line that has no `=` result is the call that never returns.
- Run it a second time without amarchy-flat resident, to separate C3 from the Amarchy-start path.
- Watch out for snoop starvation of wasabid (31-freeze-culprit.md), which makes the guest look frozen.

## Fix shape

None until the blocking call is known. Its owner is whichever of iconpen_dos.c / gthemes_dos.c / flat.c
issues the call that never returns. A host test would drive that function with a stub semaphore
or list lock and assert that it returns.

## UNVERIFIED

- Whether the guest is really frozen or only wasabid is blocked. No screen check was made.
- That boot 1 and boot 2 share one cause.
- Which task holds the lock. No snoop `--entry` capture exists.
- Side finding: the `flat_boot` setup check expects `flat=absent` when the flat prefs are off,
  but amarchy-flat stays resident with flat=on. This is a script expectation mismatch, not a product bug (unverified).
