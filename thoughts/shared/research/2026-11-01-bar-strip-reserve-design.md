---
date: 2026-11-01
topic: Amarchy bar strip reserved like an X11 strut (windows and icons stay out of the bottom strip)
status: design, not implemented
scope: host-only read; no rig, no A1200, no code edits
---

# BAR-STRUT: reserve the Amarchy bar strip at the root

Owner on the A1200: "stuff still draw behind our footer bar": Workbench icons and other programs' windows
open, zoom or size into the bottom strip. The bar is a plain window kept BEHIND normal windows (BAR-BEHIND
aa989997, BAR-CLICK a3ac2be0: plain window, raised only over foreign desktop windows by the poll,
`amarchy/bar.c:575-590`). Today the strip is reserved only for Amarchy's own placement code.

## Violated invariant and level

Invariant wanted: on the screen that carries the bar, no window Amarchy does not own gets an Intuition-chosen
or program-chosen box (open position, open size, zoom box) that reaches into the strip, and the desktop (WB root
window / DOpus backdrop) lays icons out only in the usable area.

Level of the cause: Intuition has no work-area concept (checked NDK 3.2 `Autodocs/intuition.doc`: no strut /
work-area / reserved-area tag; `WA_AutoAdjust` 5162 only keeps a window on the SCREEN; `WA_Zoom` 5226;
ZipWindow 7792-7826: the zoom gadget appears for every window with a sizing gadget, its initial alternate box
is unspecified, "more tuning needs to be done"). workbench.library V44-47 has no placement-area or move-icon
control either (`wb.doc` WBCTRLA_* list: drawer lists, hooks, hidden devices, stack, nothing on layout). So the
only root-level place is the function every window open goes through: intuition `OpenWindowTagList`
(LVO -606) and `OpenWindow` (LVO -204). Everything the app or Intuition later derives (zoom alt box, desktop
icon area) is fixed by the box set there.

## What exists and is reused (no second copy)

| Need | Existing code | Use |
|---|---|---|
| strut and usable area of a screen | `amarchy/bar_usable.c:7-28` (`bar_usable`), `amarchy/win_core.c:177-189` (`win_strut`, `win_usable`), header `amarchy/bar_usable.h:1-3` says it is "the one strut rule (R2.9)" | the clamp's area comes from `win_usable`; bar.c publishes the result |
| intuition jump-table patch, stack-safe, chained-patch safe | `amarchy/wbreset_patch.c:110-191` (`wbr_patch_install` / `wbr_patch_remove`): gate stub `wbr_gate_build` (`amarchy/wbreset_core.h:66-78`), `flat_swap_install/remove` restore-only-if-ours (`amarchy/flat_core.h:60-85`), `libpatch_done` checksum (`amarchy/libpatch.h`), public semaphore against a double install, busy count + leak-if-busy on remove | copy the model exactly; better: factor the gate+swap+semaphore install/remove of wbreset_patch.c into one `ipatch.c` both use (reuse rule, not a third copy) |
| install / remove points | `amarchy/amarchy.c:558` (`wbr_patch_install()`), `amarchy/amarchy.c:613` (`wbr_patch_remove()`) | add the strut patch next to them |
| desktop window test | `amarchy/bar_core.c:335-340` (`bar_is_desktop`: WFLG_BACKDROP or WBENCHWINDOW full width) | the clamp's "desktop" class |
| full-screen terminal test | `amarchy/win_core.h:72-74` (`win_is_fullscreen_terminal`) | pass-through class (FULLSCREEN opens its own screen, `handler/vtcon_handler.c:206`, so the screen check already skips it; keep the test as a guard) |
| Amarchy's own windows | `win_is_amarchy` `amarchy/win_core.c:192`; the bar's title is set after open (BAR-TWO-ROWS) so the hook cannot see it | exempt by geometry instead (below) |
| sentinels | `amarchy/sentinel_dos.h:5-7` (`am_sentinel`, `am_sentinel_str`) | counters for the rig test |
| probes | `tests/amiga/winprobe.c` (window boxes front to back), `tests/amiga/wincensus.c`, `tests/amiga/winbox.c` (program resize) | rig assertions |

Gap found: `bar_usable.h` claims AmTile and the notice window ask it; AmTile does (via `win_box_of`,
`amarchy/win_apply.c:21`), but `note.c` has no call (note_core takes a `note_box` from the caller). Not part of
this fix; check the caller of `note_slot` when touching it. `amarchy/win.c:148` zeroes `s.bottom` on purpose
(full-screen terminal test), fine.

No `strut` hook, SetFunction or window patch exists for windows today (grep of amarchy/ for SetFunction, LVO,
strut, usable: only flat.c graphics patches, wbreset_patch.c Close/OpenWorkBench, shot.c cgx call).

## Recommended design

### W1. One patch: OpenWindowTagList (-606) and OpenWindow (-204)

Gate stubs and swap exactly as `wbreset_patch.c`. Hook runs on the caller's task (any task, maybe a 4 KB Shell
stack): no DOS calls, < 300 bytes of locals, no IBase lock held across the original.

Hook steps:
1. Read the published strut (W3). None, or caller is not opening on that screen: tail-call the original.
   Target screen: `WA_CustomScreen`, `WA_PubScreen`, `nw->Type == CUSTOMSCREEN` + `nw->Screen`, else
   `WBENCHSCREEN` / `WA_PubScreenName` / default public screen. A name lookup uses `LockPubScreen` +
   `UnlockPubScreen`; anything unresolvable passes through (fail open).
2. Classify the request (pure, `win_core.c`, new `win_strut_class`):
   - PASS: borderless non-backdrop without drag bar or size gadget (popups, MUI/ReAction pop lists, tooltips:
     the X11 override-redirect analogue); a box that is exactly the strut (full width, bottom-aligned, height
     == strut: the bar itself, so no task check is needed); backdrop+borderless full-screen terminal.
   - DESKTOP: `bar_is_desktop` (WB root window, DOpus backdrop): keep top, set height so the bottom is the
     usable bottom.
   - BORDERED: drag bar or size gadget: move up so bottom <= usable bottom, top not above the title bar; then
     shrink to the usable height, never below MinHeight.
3. Never write the caller's NewWindow or tag list. Copy NewWindow (48 bytes, on stack) when given; tags:
   `CloneTagItems`, set `WA_Top`/`WA_Height` (and `WA_Zoom` copy, step 4) in the clone, append when absent via
   the NewWindow copy, call the original, `FreeTagItems` the clone. Clone-and-edit, not prepend-and-TAG_MORE,
   because which duplicate Intuition reads first is not documented.
4. Zoom box: when the window will get a zoom gadget (size gadget or `WA_Zoom`): a given `WA_Zoom` box is
   clamped by the same rule; when absent, inject `WA_Zoom` = usable box limited to MaxWidth/MaxHeight, only if
   probe Z0 shows Intuition's default alternate box reaches the strip (autodoc leaves it unspecified).
5. `WA_InnerHeight` requests: estimate borders (WBorTop + font height + 1, WBorBottom or size-gadget height)
   before; after the original returns, if the window still overlaps the strip (estimate off), `MoveWindow` it
   up by the overflow (a move only, safe for every app) before returning it.
6. Idempotent: if OpenWindow reaches OpenWindowTagList through the jump table the second pass sees an
   already-clamped box and changes nothing; no reentrancy flag.
7. Count every clamp in the public struct (`opens`, `clamped`, `zoom_set`) for the sentinels.

Not patched, on purpose: `MoveWindow` (-168), `SizeWindow` (-288), `ChangeWindowBox` (-486), `ZipWindow`
(-504). They are deferred deltas run by the input handler (autodoc 7815), reading the window box in the
caller would race; user drags and size-gadget drags do not go through them at all; and an X11 strut also
constrains only placement and maximise, never explicit moves. AmTile already places with `bar_usable`.

### W2. Existing desktop windows at install

The WB root window (and a DOpus desktop) is open before Amarchy starts. At install, under the published strut:
`ChangeWindowBox` each foreign desktop window on the bar screen to the usable height; record the old height.
At remove / `BAR OFF`: grow them back. Re-done when the bar reopens after a Workbench close/open (WBRESET
path; the patch also catches the reopened root window's OpenWindowTagList). With the desktop out of the strip
`bar_covered` is 0 and the raise poll (`bar.c:575-590`) becomes a fallback, kept for a desktop that regrows.

### W3. Publishing the strut

A public struct beside `wbr_pub` (or a field of the shared ipatch struct): `scr` (Screen *), `top` (first strip
row), `height`, written under Forbid by bar.c after `tw_open` succeeds (values from `bar_usable(g_scr, ...)`)
and cleared on bar close / `BAR OFF` / before CloseWorkBench (the wbreset close hook path). The hook reads
two words; it never walks the window list. Stale screen pointer is only ever compared, never dereferenced
without the bar having published it.

### I1. Icons

Root fix is W1+W2: workbench.library and DOpus lay out icons inside their desktop window. With that window
ending above the strip, CleanUp, new disk icons (NO_ICON_POSITION) and "Put Away" land in the usable area.
Unverified until probe I0: whether WB 3.1 (V40) and 3.2 (V47) relayout / redraw on the root window's NEWSIZE,
and whether DOpus 5 regrows its backdrop window to the screen on its own.

Icons already snapshotted inside the strip stay where they are (clipped by the shorter root window, same
visibility as today behind the bar). No API moves a WB icon (wb.doc WBCTRLA_* has none); the only way is
rewriting `.info` do_CurrentX/Y + `UpdateWorkbench`, which writes users' disks. Not by default: owner decision.

## Alternatives rejected

- Watcher that moves windows after the fact (bar poll / commodity): fights apps that restore positions and the
  user's own deliberate drags, visible jump, every app gets an extra resize; a commodity cannot see other
  windows' IDCMP at all.
- OS-native reserve: none exists on 3.1-3.2 (see above). Shrinking the Workbench screen and putting the bar on
  a separate screen below: two screens on one display does not work on RTG (owner's A1200 runs RTG and AGA).
- `WA_AutoAdjust`: screen bounds only.
- Patching Move/Size/ChangeWindowBox/ZipWindow: see W1, deferred deltas, racy, not how struts behave.
- Bar always in front (undo BAR-BEHIND) instead of W1: hides size gadgets of full-height windows (the reason
  for aa989997). After W1 no window opens or zooms into the strip, so the bar in front only covers windows the
  user dragged there: owner decision D2.
- Rewriting snapshot positions in `.info` files: writes user disks; decision D1.

## Risks

- Stacking with MCP, IPrefs (patches CloseWorkBench, `wincensus.c` header), Magic menu tools, other window
  patchers: the wbreset model handles it (restore only if ours; else gate goes pass-through and is leaked).
- Caller stack: hook keeps locals small; `CloneTagItems` allocates in the caller (AllocVec, fine). RULES.md 6
  stack rule applies to Amarchy itself, not the foreign caller; the probe run under `Stack 4096` checks it.
- MUI/ReAction: open through OpenWindowTagList, compute size from InnerHeight (step 5 covers it). MUI
  remembered window positions inside the strip get moved up once per open; MUI saves the moved position on
  snapshot only.
- Backdrop windows: only the desktop class is shortened; a foreign full-screen backdrop game/app on the WB
  screen gets shortened too: acceptable (it is a desktop by every test we have); list as a watch item.
- Screens other than the bar's: untouched (screen pointer check).
- Removal: gate pass-through first, desktop windows grown back, then swap restore; leak-if-busy as wbreset.
- 68020-060: gate stub is 68000 code, cache flush via `CacheClearU` in `libpatch_done` (already 060-safe).

## Files to touch

- `amarchy/win_core.c` / `.h`: `win_strut_class`, `win_strut_clamp` (pure).
- `amarchy/strut_patch.c` (new, Amiga) or, preferred, `amarchy/ipatch.c` factored out of
  `amarchy/wbreset_patch.c` + `strut_patch.c` using it.
- `amarchy/bar.c`: publish / clear strut (open path ~line 503, BAR OFF/close path), W2 desktop shrink/restore.
- `amarchy/amarchy.c:558/613`: install / remove; STATUS keys `strut_opens strut_clamped strut_zoom` (status_core).
- `amarchy/bar_usable.h`: comment (patch now asks too).
- `tests/test_win.c`: clamp cases; `tests/test_wbreset.c` stays green after the ipatch factoring.
- `tests/amiga/winprobe.c`: unchanged (the zoom alternate box has no public readout: `ZipWindow`, then read the box).
- New `tests/amiga/struttest.c` (opens a window with given flags/box, prints the box it got; optional ZIP).
- `tools/rig/amarchy_bar_rig.py`: new cases `strut_open`, `strut_zoom`, `strut_desk`, `strut_off`.
- Makefile: build rule for the probe; `thoughts/shared/research/2026-09-28_console-conformance-matrix.md` not affected.

## Reachability test plan

Host (pure, `tests/test_win.c`, fail on revert): bordered box below strip moves up; taller-than-usable shrinks
to usable but not below MinHeight; desktop box shortened; exact strut box passes; borderless popup passes;
zoom box clamped; idempotent second pass changes nothing.

One rig reachability test through the product entry point (Amarchy started from WBStartup as installed),
sentinels:
- `AMARCHY_STRUT_INSTALLED` (1 when the gate is in the -606 and -204 slots),
- `AMARCHY_STRUT_CLAMPED` (count of clamped opens), `AMARCHY_STRUT_ZOOM` (injected/clamped zoom boxes),
- `AMARCHY_STRUT_DESK` (desktop windows shortened at install).
Also exposed in `BAR STATUS`.

Rig steps (060, `UPTERM_RIG=<n> rig.py start --max --purpose "<session>: BAR-STRUT strip reserve"`, then
`--os 32` second run; stop + clear the rig after):
0. Probes before code (decide W1.4 and I1): Z0 open a sizable window with no WA_Zoom on stock 3.1 and 3.2,
   `ZipWindow`, read the box: does the default alternate reach the screen bottom? I0 `ChangeWindowBox` the WB
   root window 20 rows shorter, insert a disk/ new drawer icon, CleanUp: do icons stay above? Same with
   DOpus desktop if installed.
1. Bar on; `struttest` opens a 200x150 window at top = screen height - 100: assert box bottom <=
   strip top (winprobe), `AMARCHY_STRUT_CLAMPED` +1.
2. `struttest` full-height sizable window, `ZipWindow` twice: neither box enters the strip;
   `AMARCHY_STRUT_ZOOM` +1.
3. `winprobe`: WB root window bottom == strip top - 1; `AMARCHY_STRUT_DESK` == 1; drop a new icon
   (NO_ICON_POSITION) via `UpdateWorkbench`: its position above the strip.
4. Shell under `Stack 4096` opens via the patch: no crash (stack check).
5. `BAR OFF`: same opener gets the unclamped box, root window back to full height; `BAR ON` restores.
6. Quit Amarchy: -606/-204 slots hold the original targets again (lvoprobe), unless chained (report it).
Revert check: steps 1-3 fail with the patch install line commented out.

## Owner decisions

- D1: move icons already snapshotted inside the strip (rewrite `.info` positions) or leave them (recommended:
  leave; new placement and CleanUp are fixed by I1, no disk writes).
- D2: after W1, bring the bar back in front of normal windows (only user-dragged windows reach the strip then)
  or keep BAR-BEHIND (recommended: keep until the rig shows W1 holds, then ask).
