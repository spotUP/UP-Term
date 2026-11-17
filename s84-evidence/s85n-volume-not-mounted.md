# S85n "UP-Term: volume not mounted" during Patina theme apply (2026-11-16)

Verdict: NOT proven. Source gives one confirmed unguarded path and one strong suspect; the rig repro failed.

## Confirmed in source (unguarded DOS lookups of UP-Term:)
- patina/proclaunch.c:54 pl_stack(): `LoadSeg("UP-Term:bin/patina-icons")` with the process's normal pr_WindowPtr. pl_start (proclaunch.c:66-74)
  is what dk_icons_sync calls (patina/desk_dos.c:632,635) on every theme apply that changes the icon state. The child shell then resolves
  `UP-Term:bin/patina-icons APPLY` (SystemTags, line 72) with the inherited window pointer, also unguarded. If UP-Term: (or the path it points at)
  is not resolvable at that moment, DOS puts up the requester naming the assign. Compare proclaunch.c:33-37 (pl_dup_path), which does guard its Lock.
- patina/icons.c:477-481 patina-icons sets pr_WindowPtr=-1 only AFTER its own startup, so it cannot stop a requester raised while it is loaded/started.
  Other guarded sites: iconpen_dos.c:129,154, gthemes_dos.c:288, menu_font.c:40.
- IPrefs requesters are the system's own (it reads ENV:Sys/*.prefs and anything a prefs file names); no Patina code can guard them, so they point to the
  underlying condition below, not to a second bug.

## Suspect underlying condition (unverified)
UP-Term: is unreachable for a moment around the screen reset (desk_dos.c dk_windows_close/open ~322-345). Candidates, none measured:
1. multi-directory assign VTCX:Apps + ADD kit made by tools/rig/patina_gui_rig.py:1281-1290 --setup (rig-only; Lock() returns the first match only);
2. an assign removed late by a harness (s85-upterm-assign.md list) or pointing at a not-yet-mounted volume (Rushhours:).
No source in patina/ removes or re-points the assign.

## Slow first apply (~23 s to Keep): not investigated. Likely the same: a requester or volume wait before the Keep requester; cold LoadSeg of patina-icons is another candidate.

## Rig attempt
Rig 84 setup + start (purpose "vtcon S85n session: volume-not-mounted requester hunt"): boot.log ran to "wasabid started", shell prompt on serial,
but wasabid never answered (ami.py exec timed out 60-150 s, rig.py status "down" for >6 min). The box also has no PIL, so emuwin.window_text OCR
(tools/rig/emuwin.py) is unavailable and I could not read what the screen showed. This boot may itself be stuck in a volume requester (Dopus 5.104 not
installed in rig 84's VTC: drawer; S:Shell-Startup not read). Not diagnosed. No Assign output captured.

## Proposed fix (not applied)
- Layer: proclaunch.c (shared launcher). Wrap pl_stack's LoadSeg and the pl_start SystemTags call in pr_WindowPtr=-1 (set the pointer in pl_start before
  pl_stack, restore after; the async child gets -1 only if inherited, so also set it in patina-icons earlier: first line of main, before the
  AllocVec, not after OpenLibrary). Same pattern as proclaunch.c:33-37.
- Root cause for IPrefs: find why UP-Term: is unreachable at reset. Cheapest proof: in the next rig run, poll `Assign UP-Term: EXISTS` + `Assign LIST`
  every second through the pick/Keep/reset window and log with timestamps.

## Confidence: low-medium that the unguarded LoadSeg/SystemTags is a requester source; low on the root cause.
## Unverified: everything about rig behaviour, the 23 s delay, whether the child inherits pr_WindowPtr.

## Assign poll run (16 Nov)

(The earlier agent report file did not exist on disk; this file was created by this run. Source: thoughts/spot/todos.md S85n row.)

Result: NOT reproduced. Cause NOT proven.

Method: rig 84 (kept running), 1 Hz poller (scratchpad poll.py) running `Assign UP-Term: EXISTS` + `Assign LIST` (UP-Term lines) and ami.requesters() over wasabid, logging on change. Sequence: `patina_gui_rig.py --setup --no-build`, then 3 theme picks via patina_desk_rig.pick (hotkey, Style, Patina theme, Return) with desktop-theme = on prefs.

Timeline (poll.log, 16 Nov):
- 09:49:31 UP-Term = Rushhours:Tools/UP-Term (kit), REQ=[]
- 09:55:08 same (setup running, Breaks Patina programs)
- 09:55:25 UP-Term = VTCX:Apps + Rushhours:Tools/UP-Term (setup's `Assign UP-Term: VTCX:Apps`, then `ADD kit`; patina_gui_rig.py:1291-1292). The first command REPLACES the target, so no poll second saw the assign missing (sub-second gap not excluded).
- 09:57-09:59 three picks (vantablack): assign unchanged, no requester, EXISTS rc=0 throughout, 45 s after the pick too.

Findings:
- The picks produced no "Patina desktop theme" Keep window (cursor stayed on tokyo-night (current)), so the IPrefs reset path that raises the requester was NOT exercised. The run did not reach the failing step.
- 6b97f707 (15 Nov 22:07) already routes every harness that drops UP-Term: through install_rig.drop_upterm_assign / restore_upterm_assign and re-makes a missing assign in rig.py start; the S85m run (before/around that commit) may have started with a lost or kit-only assign. Unverified.
- Tool gap: the Keep window is not matched by ami.requesters() (its buttons are not in REQUESTER_ANSWERS), so a requester log cannot see it; poll window titles.

Next: reproduce with the patina_confirm_rig.py sequence (prefs desktop-theme + flat on, baseline pick, 20 s wait, watcher clicking Keep) with the poller running, ideally on a freshly booted rig (first press showed the Keep window only at +23 s). Fix stays as decided: pr_WindowPtr guard on LoadSeg/SystemTags in patina/proclaunch.c as a SECOND, separate fix (hides symptom). No product or harness code changed. Confidence harness is not the cause: medium.

## Hotkey repro with assign poll (16 Nov)

Rig 84, current main build (VTCX:Apps/bin, not rebuilt). Setup as S85m: ENV:up-term/keymap.user [bind] super+ctrl+f10 = gtheme:starship, super+ctrl+f9 = gtheme:nord; PATINA RELOAD; ami.key 0x59/0x58 qual 0x48. Poller at 1 Hz: `Assign UP-Term: EXISTS` rc, Assign LIST, all window titles (ami._m().read_ui every_screen). Script/logs: scratchpad s85n_run.py, s85n_A.log, s85n_B.log, s85n_runC.py, s85n_C.log.

- Run A (UP-Term: = VTCX:Apps + Rushhours:Tools/UP-Term, as the rig stood): presses at +4.5 s, +40.2 s, +75.3 s (starship, nord, starship). Keep window ("Patina desktop theme") seen and clicked +7.5 s after each press. EXISTS rc 0 in all 90 polls. Window titles only '', 'Patina bar' (+ the Keep window briefly). No "System Request" in any poll. NOT reproduced.
- Run B (UP-Term: removed, then assigned ONLY to Rushhours:Tools/UP-Term = what boot/go gave today; it has bin/patina-icons): same 3 presses, Keep at +7.6/+7.8/+8.0 s. EXISTS rc 0 in all polls (82 of 82). No System Request. NOT reproduced.
- Run C (extra: UP-Term: fully unassigned, EXISTS rc 5 in every poll; 1 press): Keep window appeared and was clicked +7.7 s; still no System Request in the window list (only '', 'Patina bar', Keep window). The S85m requesters therefore do not come from the unassigned state alone on this build either.
- Assign was never missing or changed during A/B (assign poll constant); in C it was removed by this script, not by any product process.

Verdict: S85n ("UP-Term: volume not mounted" requester after hotkey theme apply) does NOT reproduce on current main in any of the three assign states. The S85m requesters (IPrefs + patina-icons) appeared in a session with an earlier setup history (several patina_gui picks / PATINA_THEME runs before); no product process removing UP-Term: was observed, so no file:line cause. Unproven: whatever removed the assign in S85m (candidates in the earlier session: the rig tooling's own Assign calls, not product code). Caveat: window poll is 1 Hz and read_ui may miss a requester shorter than ~1 s; every Keep click and every window list was clean.
End state: UP-Term: restored to VTCX:Apps + Rushhours:Tools/UP-Term, keymap.user deleted, PATINA RELOAD sent, rig 84 running, no requesters. No product code changed, nothing committed.

## First apply timing (16 Nov)

Rig 84, current main build (VTCX:Apps/bin, not rebuilt). Script: scratchpad s85n_first.py, log first.log. Cold state = `PATINA QUIT`, 3 s, `Run >NIL: UP-Term:bin/Patina`, 8 s settle (no rig reboot). Keymap as S85m. Press 1/2/3 with 35 s gaps, two cycles. Window-title poll back to back (no sleep): real interval 0.062 s (10 polls, 0.62 s); Keep clicked as soon as seen. The "+7.5 s" of the earlier runs was a fixed script wait, as suspected.

| press -> Keep window | 1 (cold) | 2 | 3 |
|---|---|---|---|
| cycle 1 (starship, nord, starship) | 12.7 s | 7.5 s | 6.9 s |
| cycle 2 | 6.1 s | 3.0 s | 7.0 s |

Verdict: the first press is NOT reliably slower. Cycle 1 first press is +5 s over later ones, cycle 2 first press is mid-range (3.0 to 12.7 s spread overall, later presses vary 3.0 to 7.5 s). The S85m +23.0 s first / +0.1 s later pattern does not reproduce on current main; the steady apply cost is 3 to 7 s (not 0.1 s) and carries a noisy extra of a few seconds, no cold-start-specific cost isolated. No cause file:line found; no ENV counter or instrumentation was needed (apply path candidates desk_dos.c dk_icons_sync -> pl_start patina-icons / proclaunch.c:54 LoadSeg were not measured separately). If the 23 s recurs, the next step is a timing print around dk_icons_sync (desk_dos.c:624) and the confirm requester open, plus patina-icons lock wait (icons.c:403-425 "another run is busy, waiting").
End state: keymap.user deleted, PATINA RELOAD sent, Patina restarted fresh, rig 84 running. No product code changed, nothing committed.

## Theme apply profile (16 Nov)

RESULT: NOT MEASURED. Per-step timings were not obtained; rig 84 froze three times with the instrumented Patina. Static path below only, no ms numbers.

Instrumentation (uncommitted, reverted): the existing opt-in facility patina/atiming.h (`SetEnv PATINA_TIMING 1`, at_mark writes "<tag> <DateStamp ticks 1/50 s>" to RAM:patina.timing) plus at_mark calls at: do_row hotkey (patina.c:~415), gt_dos_select (s_enter, s_saved), gt_dos_apply_saved (g_restored, g_loaded, g_termsent, g_deskdone, g_recoloured), dk_apply (a_enter, a_planned, a_snap, a_wbclosed, a_palput, a_reset_done, a_dopus, a_others, a_wbopen), desk_dos_apply (d_applied, d_flatpens, d_wbborder, d_livepens, d_flatsync, d_confirmed), dc_begin (c_reqopen). Diff kept: scratchpad/instr.diff; driver scratchpad/prof.py.

What went wrong: build was fine (415796 B vs clean 413368 B). Deploy of the instrumented Patina to rig 84 was followed by a wasabid that stopped answering ("auth failed ... did not answer") on run 1 (right after the stale-refresh/put), run 2 (fresh boot) and run 3 (after "CYCLE 1", i.e. at PATINA QUIT/Run). Each time the rig needed `rig.py stop` + `start` (this wipes build/rig84; saved files went to build/rig-saved/84/20261116-104738). Cause not isolated: the freeze may be the instrumented binary or the QUIT/put/start sequence on a fresh rig; a clean Patina deploys and runs on the same fresh rig. Note also paths.UPTERM_BIN = VTC:Apps/bin has no Patina on a fresh rig 84; the live copy is Rushhours:Tools/UP-Term/bin/Patina (UP-Term: assign), so the dep.py second path put fails there.

Code path, hotkey to Keep window (candidates, file:line, not timed):
1. patina.c:425 do_row KA_GTHEME -> gthemes_dos.c:218 gt_dos_select: amconf_save (ENVARC + ENV writes).
2. gthemes_dos.c:~190 gt_dos_apply_saved: gt_dos_desk_restore, gt_dos_load, gt_dos_term_send.
3. desk_dos.c:1314 desk_dos_apply -> dk_apply (desk_dos.c:492): dk_read icontrol, desk_plan, dk_orig_save, dk_snapshot (file copies on a rig disk).
4. If the palette changes and pens reset: dk_windows_close, dk_put_palette (SYS:Prefs/Palette editor run), then dk_wait_reset (desk_dos.c:352): polls every Delay(12) = 0.24 s up to DK_RESET_SECS*4 = 24 polls (6 s) for the Workbench screen reset; this is the only step that can plausibly cost 3 to 7 s by itself, and the wait for IPrefs after Font/IControl (dk_wait_iprefs, Delay(1) polls, up to 6 s) adds to it. dk_windows_open follows.
5. DOpus theme send (dk_dopus_theme), dk_wbborder, dk_live_pens, dk_flat_sync -> dk_icons_sync (desk_dos.c:~624, pl_start patina-icons: asynchronous, so only LoadSeg contention on the disk) .
6. dk_confirm_ask (desk_dos.c:1426): dk_marker_write copies the pending set to ENVARC:Patina/pending-revert.d (disk), THEN desk_dos_confirm_begin opens the Keep requester. The requester opens after every step above, so all the slow work sits before it.

Likely slow step (unproved): dk_wait_reset / dk_wait_iprefs waiting for IPrefs' Workbench reset (a fixed Delay(12) poll loop with a 6 s cap) before the Keep requester opens; cold first press adds the first Palette editor load and file-cache misses. Proposed fix (not applied): open the Keep requester (dk_confirm_ask) before the reset wait, or at least before dk_marker_write, and shorten the poll to Delay(1); re-measure with the at_mark diff first.

Next step to get numbers: apply scratchpad/instr.diff, find why the rig freezes (try deploying with Patina left running and only starting a second copy after PATINA QUIT with a 10 s gap, or use rig 83 if the owner allows), then read RAM:patina.timing after each press.

Restore: instrumentation reverted (git status patina/: clean), clean Patina rebuilt through buildq (413368 B), deployed to Rushhours:Tools/UP-Term/bin/Patina on rig 84 (fresh rig, restarted with purpose "vtcon S85n session: profile theme apply"), Patina port up, no requesters, ENV:up-term/keymap.user absent (fresh rig), no PATINA RELOAD needed. VTC:Apps/bin/Patina absent on this rig.

## Apply wait cap counts (16 Nov)

Instrumented Patina (diff: scratchpad/caps.diff; static counters in desk_dos.c, published with am_sentinel at the end of desk_dos_apply as PATINA_WAIT_<RESET|IDLE|IPREFS|DOPUS>_CALLS/_CAPS/_MAXI). Cycle 1 only (2 cold cycles planned): rig 84 stopped answering (wasabid connect timeout) during cycle 2 start; per the brief, stopped there. Cumulative counters since Patina start; per-press = diff. Press 1 of cycle 1 has no "before" read (ENV vars did not exist yet), so its numbers are the "before" of press 2. Keep seen = press to the Keep requester.

| press (cycle 1) | RESET calls/caps/maxi | IDLE (idle mask) | IPREFS | DOPUS | Keep seen |
|---|---|---|---|---|---|
| 1 (starship, cold) | 0 / 0 / 0 | 4 / 0 / 0 | 4 / 0 / 12 | 2 / 0 / 0 | 1.46 s (suspect: matched the old requester, not the new one; unverified) |
| 2 (nord) | +1 / +1 / 24 | +3 / 0 / 0 | +3 / 0 / 12 | +1 / 0 / 0 | 7.45 s |
| 3 (starship) | +1 / +1 / 24 | +3 / 0 / 0 | +3 / 0 / 12 | +1 / 0 / 0 | 7.35 s |

Verdict: dk_wait_reset (desk_dos.c:352, called :569) runs to its cap on every press where it is called (24 of 24 iterations = 6 s); that is the 6 s of the ~7.4 s press-to-Keep time. IDLE, IPREFS (max 12 iterations of 300 = 0.24 s... event ends it) and DOPUS (0 iterations) never hit their caps.

Cause (file:line): desk_core.c:243-247 desk_reset_done is true only when the Workbench screen pointer changed or desk_pens_reset() (desk_core.c:212) is false. At press 2/3 the reset is predicted (desk_dos.c:532, live pens differ), but neither event arrives in 6 s: on 3.2 IPrefs resets in place (screen address stays), so only the pens test can end it, and the live pens read back by dk_wb_screen (desk_dos.c:269) never compare equal to desk_pens_for(flat, ...) for this theme/depth (a slot that does not read back, like the BLOCKPEN case already skipped at desk_core.c:224). Which slot mismatches is UNVERIFIED (not dumped; needs live[] vs want[] dump on the cap hit).

Proposed fix (not applied): end the wait on IPrefs idle (desk_iprefs_idle, desk_core.c:259, the event dk_wait_iprefs already uses; IPrefs back in Wait() with nothing pending = reset finished) as well as on the pens test, and find the mismatching slot in desk_pens_reset so the pens test is exact. Keep still opens after the reset finishes.

Restore: patina/ reverted (git status patina: clean), clean Patina rebuilt via buildq (413368 B, build/amiga/Patina). NOT redeployed: rig 84 is unresponsive (wasabid timeouts, fs-uae process still alive). The instrumented Patina is still on the rig, ENV:up-term/keymap.user and PATINA RELOAD not cleaned there. Needs a rig 84 restart, then redeploy + delete keymap.user.
