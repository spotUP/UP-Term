# RIP-7b glue implementation notes (worktree vtcon-rip7b-glue, branch rip7b-glue)

Commits: step1 bb8b91cb (engine vt_feed_stop/vt_fed), step2 c5c20fa5 (handler/dlsave), step3 e4e10987 (handler/ripglue).
Suites: ripglue 52/0 failed, dlsave 24/0, rip 726 (step 1), ripxfer 24.
Mutation proof: glue_loop do_stop=1 -> 0 gives "ripglue: 52 checks, 6 failed" (test_ripglue.c:251,252,272, both rip modes). Restored.

Reused: ripxfer_receive, zm_receive/zm_send, test_ripxfer socketpair/fork harness, dl_clean_name, vt_dl_sink, watch_start pattern (planned).

## Open: step 4 (vtcon_handler.c wiring)
Line map: struct con ~175; in_append ~954; host table ~1061 (add rip_block); watch_start/stop ~1524-1585; config read ~1749-1773;
open_window ~3484 (vt_set_rip(t,2), vt_set_download_sink(t, dl_save, host from vt_cwd or "local")); output() ~3805; ACTION_WRITE ~8571;
Wait mask ~9065-9102; cleanup ~9237; fill ~2380.
Design: worker process (stack 16384) + msg port; ACTION_WRITE while c->xfer queues packet to worker, reply only after consumed;
if vtwin_write fed < len, remainder is first chunk. XFER_SEND -> in_append+service_reads; XFER_DONE clears xfer. Skip OPOST during xfer.
Close/exit: CTRL_C to worker, reply held packets -1. rip_block cb: ripglue_arm, vt_feed_stop, start worker.
Makefile: add ripxfer.o, zmodem.o, dlsave.o (-DDLSAVE_DOS), ripglue.o to handler objects (~896, ~1865-1906).
Prefs row "rip" (off/on/auto, default auto), template SDI_CLIPBOARD:
 settings_def.h enum (line 32) + settings_def.c value table (~36) + row (~167, FO(rip), PG_RIP) + write-order list (~354);
 prefs_ui.h PG id (~43); prefs_ui.c pg_items (~177, PGK_CYCLE, 3 values); upprefs.c case (~318);
 prefs_core.h field (~48), prefs_core.c default (~57) and compare (~589); handler read ~1773 (sd_conf_key_index("rip",...)), fill ~2380; slash.c tables.
## Open: step 5 vbcc build -warnings-as-errors.
## Unverified / edge cases
- Protocol id not checked at arm time (U2); non-ZMODEM stream after stop ends in zm_receive timeout.
- Stop at CR: parser pending-LF eol state may skip a leading LF of next feed.
- Auto mode: halt at '|' inside replayed held line drops rest of that line.
- Prefs row footprint (test_settings_def row counts, slash.c) not surveyed to the end.

## s69 steps 4-5

Commits: 3ba11f62 (step 4: worker process, ACTION_WRITE diversion, rip_state behind c->rs), step 5 commit (see git log, "Add per-profile rip setting"). Plan and notes were untracked in the main checkout; this copy now lives in the worktree vtcon-rip7b-glue.

Suites (host): ripglue 84, dlsave 29, rip 726, ripxfer 24, slash 436, settings_def 1847, prefs 10475, menudef 9, all 0 failed. vbcc `make build/amiga/vtcon-handler` rc=0 (proves the wiring compiles only; the host cannot run the handler, routing is tested as the pure function ripglue_write through the real engine).

Step 5 shape: SDI_RIP row "rip" (off/on/auto, default auto) on the Advanced page, label "_File" (the row needs a unique shortcut; r, i, p are taken; label width is capped at 4 characters because it widens the Use Workbench's button column). Placed row 4 col 2. Page width 640 -> fits 618 only after the Link command field went 32 -> 28 characters (side effect on another feature). Also /rip off|on|auto, menu ids MENU_SET_RIP_*, goldens regenerated (+ `rip = auto`).

Risk dispositions:
- Pending LF after stop at CR: covered in route_case, passes.
- Auto mode halting at '|' in a replayed held line drops the rest of that line: untested; the level-9 command is last on its line and protocol bytes follow the CR. Noted only.
- U2 (non-ZMODEM stream ends in zm_receive timeout): worker-level, Amiga side; ripglue_arm does not check the protocol id. Noted only.

Left: no rig verification; cooked-mode raw delivery unverified; no user notice when mode On or an unsupported block is refused; dl_save registered with host "local" (dl_host_of_cwd exists, OSC 7 host not fed, O2 not fully wired); no menu-bar item for rip (slash and prefs only); keymap suite had 11 DOpus toolbar failures not checked against a clean tree; stray untracked render/jumpscroll.h belongs to another session's stash (stash@{0} jumpscroll), not committed.
