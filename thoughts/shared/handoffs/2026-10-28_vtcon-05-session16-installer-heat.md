---
date: 2026-10-28
session: vtcon-05 session 16 (cross-session name vtcon-f6)
topic: Installer drive passes the nested AmiSSL finish page; outer "Reboot once" message page still stops it; heat and disk cleanup
tags: [installer, rig, heat, disk]
status: final
---

# vtcon-05 session 16 handoff

## Slot rules (unchanged, plus heat)
- Our rigs: 9, 10, 11. Never touch rigs 1, 2, 3, 4, 7, 8. Never stop rig 1.
- Owner said the Mac runs hot (2026-10-28): this session runs ONE rig at a time, never two in parallel.
- Stop a rig only after the last command plus 5 s quiet.
- Never delete another session's or agent's files without its OK (owner, this session).

## Tasks
1. INSTALLER-CLICK-SPEED step 3: timed fresh-all run on rig 9. Open (see Next Steps).
2. nvim 5-run recheck / NVIM-HEAP-CORRUPT: queued behind task 1, one rig at a time.
3. RIG-CLONE-NEW-NUMBER: DONE 07f3e368.
4. Disk and heat cleanup: DONE (disk 1.7 GB to 14 GB free; rigs 2, 3, 7 stopped by their owners).

## Critical References
- tools/rig/installer_rig.py: `installer_name` (~line 341), `reboot_later` (~line 346), the `(100% done)` branch in `drive_pages` (~line 580).
- tests/test_installer_drive_loop.py: class NestedFinishPage (harness `run_pages` fakes pages, clicks and OCR). Makefile `ONLY=installer_drive`.
- thoughts/spot/term-findings/installer-click-speed-s16.md: run 1 and "Run 2 (a3c680f1)" sections.
- thoughts/spot/term-findings/nvim-recheck-s16.md, thoughts/spot/term-findings/curl-check-rig-s16.md.
- Ledger: thoughts/spot/todos.md, rows "INSTALLER-CLICK-SPEED", "s16 HEAT", "DISK s16" near the end.

## Recent Changes
- d26d3636 rig helpers work inside a worktree.
- 07f3e368 `fresh_sys_hdf()` creates the rig directory before the `cp -c` clone; test tools/test_rig_clone_new.py (`ONLY=rig_clone_new`).
- a3c680f1 the reboot guard applies only to the first Installer window name; a nested Install-AmiSSL's finish page is answered with Proceed. Tests NestedFinishPage (one fails with the fix reverted, the outer-reboot guard test passes both ways).

## Learnings
- Run 2 (a3c680f1) on rig 9: no ConnectionRefusedError; the nested AmiSSL finish page was clicked through at 60 s; the drive reached the outer "VTC:distkit/Install (100% done)" page at 76 s (128 s at 8605d39d, which reached the end).
- The outer 100% page is NOT the "Reboot now?" ask. Ids 0, 90, 91; OCR: "Reboot once: then LIBS: has AmiSSL. TCP/IP stack found... AmiNetXDuo was not installed. Proceed | Abort Install". `reboot_later` sees "reboot", no "later", no id 1, returns 0, and the drive returns False.
- Waits over 5 s in run 2: 17 to 34 s (Installer preflight before the copy, unchanged) and 60 to 75 s (outer Installer returning after the nested finish click; cause unverified).
- A LAYOUT fault (text in bottom border strip, y 204..205) was logged on that same outer page; unverified whether it is real or an artifact of the stop.
- nvim crash is not caused by a stale ixemul: ixemul is current. Run 1 killed FS-UAE; runs 2-5 not run.
- installer_rig.py has no --help: usage is `UPTERM_RIG=9 installer_rig.py ONLY <case>` (also FAILED, FORCE).

## Artifacts
- Run 2 log and pages: session scratchpad installer-s16-run2.log and fresh-all-s16-run2.pages.txt (scratchpad of session 18ac4fbe; may be gone).

## Next Steps (in order)
1. Decide how the outer "Reboot once ... Proceed | Abort Install" page is answered. First read the distkit Install script (upterm-ports kit, grep "Reboot once") to see what Proceed leads to. If Proceed only shows the real "Reboot now?" ask next, `reboot_later` must return None for a message page with Proceed and no ask gadgets (a page whose text asks to reboot later, not now), with a test in NestedFinishPage style; the "Reboot now?" page without Later must still never be clicked.
2. Check the LAYOUT fault on that page: real bottom-strip text or the stop.
3. Rerun step 3 on rig 9 (sonnet agent, one rig) for a full first-to-last timing against 128 s.
4. Then the nvim 5-run recheck (NVIM-HEAP-CORRUPT: find the first writer to _SysBase), on a quiet host, one rig.
5. INSTALLER-END-CRASH stays parked until the owner OKs the cost.

## Other Notes
- Do not touch: musicplayer-55 scratchpads 7b6e9b73, 2198f063, 05a22682; worktree vtcon-splits (63 unmerged commits); UpRough scratchpad eaea0c13 (owner's call).
- Uncommitted files at start that are not ours: tests/test_installer_drive_loop.py and tools/rig/installer_rig.py are now committed; the Amarchy handoff edit, `-`, scratchpad_info, scratchpad_w.log and the session 13/14 handoffs are other sessions' or earlier files, left alone.
