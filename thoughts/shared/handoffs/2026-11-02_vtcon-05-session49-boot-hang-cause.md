---
date: 2026-11-02
session: vtcon-05 session 49
topic: OS 3.1 boot hang root cause (Amarchy LockPubScreen deadlock)
status: cause found, fix handed to vtcon-16 (Amarchy owner)
---

# Handoff: vtcon-05 session 49

## Done
- Boot hang root cause found and committed (3a1cafc3, evidence file
  thoughts/shared/research/s30-evidence/amiberry-hang-vtcon05.md Steps 10-15).
  - Amarchy wb_mode_id() (amarchy/wbmode.c:38) calls LockPubScreen("Workbench") via the W43
    wbm_check poll (amarchy.c:577, :267). When IPrefs has the Workbench closed for its OS 3.1
    boot RTG switch, intuition 40.85 deadlocks: Amarchy holds IntuitionBase+$B3A and waits on
    +$8DA; IPrefs holds +$8DA and spins on +$B3A ($FD13A6). Input and wasabid block behind it.
  - Not Amiberry-specific (fs-uae rig 2 hit it too). OS 3.2 rigs never do the RTG switch (vtcon-16
    ledger row RIG-OS32-NO-RTG).
  - Picasso96 rtg.library 43.787 owns the VBlank patch and is innocent (Step 14).
- Fix handed to vtcon-16 with the proposed change (LockPubScreenList + shared wb_screen() walk from
  amarchy/iconpen_dos.c:84) and the full list of same-pattern call sites.
- Rig 2 returned to musicplayer-16, stopped. Rigs 37 and 48 deleted earlier (owner OK).
- Owner flagged 20 GB of leftover rig folders. Own rigs 31 and 35 deleted; musicplayer-16's idle rigs deleted
  with owner OK. Root fix 8df99f0b: `rig.py stop` clears rig N>1 (unless --keep or emulator still running);
  `rig.py start` and `rig.py gc` sweep other rigs' folders idle >2 h with no emulator and no held start.lock
  (log build/rig-gc.log). Follow-up 58b83636: rig start tests stub gc/guard_stale_base/refuse_port_clash so a
  unit test can never sweep the real build/ folders.
- vtcon-16 landed the Amarchy fix: origin/main 13955b72 (fix 5145f5a5, ipd_lock_wb_screen()); forwarded to
  dctelnet-db and musicplayer-16.

## Open
1. vtcon-16 runs 3+ fresh OS 3.1 RTG boots on rig 50. On 0 hangs, mark ledger lines 7 (AMIBERRY-DEFAULT)
   and 8 (FSUAE-WASABID-NO-ANSWER) done. Ask musicplayer-16 whether upkg_browse_rig.py:150 needs --keep.
2. BLOCKED on owner (agent spawn denied by permission classifier). Secondary wasabid defect: cmd_screenctl sends on the socket while holding LockIBase
   (build/wasabi/src/wasabid.c:5783-5802). Buffer first like WINDOWS (~2591). Write a PR to
   creep-ltx/wasabi (spotUP is a collaborator; merge own PRs there).
3. dctelnet-db's 3.1 + Amiberry sample with ~91% CPU and wasabid never up may be a second hang.
   Ask for the log if it recurs after the Amarchy fix.
4. Amiberry 8.5.0 focus-lost main-thread stall (SDL_SYS_SetThreadPriority) - possible upstream report.
5. upterm-ports main checkout sits on port/htop, 157 behind origin/main; a vtcon-16 agent ran
   make kit-stage there. Tell the owning session.

## Queue after the hang
Kitty T-E (needs owner D12/D13), Kitty T-F/G/H, timg port, chafa + GLib, nnn kitty preview,
picture tools P3-P5, make dist so icat is in the kit, rest of AMIBERRY-RIGS-GAPS.
Owner decision open: whether --a500/--stock stay blocked under Amiberry.
