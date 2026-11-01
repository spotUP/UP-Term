# outline hunt (s27) - NO PROVEN CAUSE, measurement blocked
- Rig 8 was NOT running at task start (ami.py: connection refused; rig.py status: down). Started it 4 times
  (`rig.py start --max --os 32`). Each time wasabid answers `up`, then the FS-UAE process for rig 8 exits within ~1-4 min
  without my commands (fs-uae.log ends mid-line at "mmu", no crash report, no rig 8 watchdog process). The last run died
  inside rig.py start itself (ConnectionResetError after "up"). Another session restarts rig 4 (vtcon-l12) at the same time.
- Debug build was ready and compiled: dbg[24] added to flatshared, counters (reached branch, Layer NULL, Window NULL,
  window found, rp==w->RPort, rp, Layer, ClipRegion + bounds, BitMap vs screen BitMap, layer bounds, window geometry,
  DamageList, flags, flat_outline_rects return value, pen_outline) printed as a hex "DBG" line by STATUS. Never ran on the rig.
- Code reading only (unproven): flat.c b1b6d940 draws via RectFill on d->imp_RPort at window-relative 0,0..w-1,h-1.
  Intuition draws border gadget images with the layer clipped to the gadget box (or a clip region installed around
  the gadget refresh), so candidate (2) is the likely one; candidate (3) next. Unverified.
- Cleanup: worktree olhunt removed, debug code not committed, RAM/VTCX debug copies removed, build/rig8/vtc/amarchy-flat untouched (never overwritten).

## Run 2 (rig 8, 3.2, debug build, measured)
Window 220x90 (flatwin), after `flatwin 1 SET 1 0 1 2 0 3`, DBG from STATUS:
- branch reached: yes (every c2_draw). Layer NULL: 0. Layer->Window NULL: 0. -> candidates 1 and 4 DISPROVED.
- rp == ow->RPort: NO (0). imp_RPort is a different RastPort on the same layer (layer ptr same each call). BitMap == screen BitMap: yes.
- Layer->ClipRegion: NULL. Layer bounds 40,30..259,119 = exactly the window 220x90. Layer cliprects: 1 total, 1 visible
  (not obscured), bounds = whole layer. -> candidate 2 DISPROVED (no clipping to the gadget box).
- flat_outline_rects returned 4 (all four rects issued), pen_outline = 3.
- ReadPixel(imp_RPort, 0, 5) immediately after the ring = 3 (ring pixel IS on the bitmap right after the draw).
- Re-entry reads at the start of the next c2_draw on the same layer (3 flatwin cycles): top edge (100,0) still pen 3 in 18 of 23,
  overwritten in 5; left edge (0,5) intact 12, overwritten 11 (the close gadget glyph box fill covers x=0 there, expected).
  So the ring survives gadget calls but is lost before the end of a refresh -> candidate 3 (something painted later) is the
  remaining one; the last overpainter is NOT yet identified (needs a read after the full refresh).
- Last attempt (STATUS scanning windows under LockIBase with ReadPixel) DEADLOCKED rig 8 (graphics call under IBase lock).
  Rig 8 is hung: ping times out, FS-UAE pid 14203 still running at ~59% CPU. Not reset by me.
- Debug patch: scratchpad/olhunt2-debug.patch. Worktree + branch removed. build/rig8/vtc/amarchy-flat set to the clean
  origin/main build (md5 fbd3df6276c4c4632942a85d5ccc5a76); not yet restarted on the rig (rig is hung).
- Next probe: read the pixel after the refresh ends WITHOUT LockIBase (use the window's RPort from a task context after Delay).
