# DOpus PR draft: display_open clears GUIF_CLOSE_PENDING

Repo: BlitterStudio/dopus5. Local clone (not pushed):
/private/tmp/claude-501/-Users-spot-Code-UP-Term/7dd70f99-ee11-4f9e-abff-f490d3a2ec21/scratchpad/dopus5-pr
Branch: display-open-clears-close-pending (one commit on upstream master 81fc554d).
Files: source/Program/display.c (1 line + comment), ChangeLog (entry under "[dopus5 next]").

CONTRIBUTING: ChangeLog entries are the norm (added). Asks for an issue first only for bigger work (this is a small fix).
Asks for a clean OS3 build with no new warnings. NOT done locally: no m68k-amigaos-gcc, no docker daemon/image here.
CI builds the full matrix on PR.

## Title
Clear GUIF_CLOSE_PENDING when the display is opened

## Problem
display_open() clears GUIF_CLOSED but not GUIF_CLOSE_PENDING. event_loop.c (DN_CLOSE_WORKBENCH) ignores
a close request while GUIF_CLOSE_PENDING is set. If the display is reopened by something other than the
close path while a close is pending, the flag is never cleared and Opus never closes for a later
CloseWorkBench(). Anything waiting for the Workbench screen to close (e.g. the IPrefs reset requester)
then hangs.

## Repro
1. Opus on the Workbench screen.
2. A CloseWorkBench() request arrives; DN_CLOSE_WORKBENCH sets GUIF_CLOSE_PENDING and starts dopus_closewb.
3. Before the pending close completes, a reset/refresh runs, e.g. ARexx `dopus refresh all`
   -> main_reset -> display_open. GUIF_CLOSED is cleared, GUIF_CLOSE_PENDING stays set.
4. Next CloseWorkBench(): DN_CLOSE_WORKBENCH is ignored (event_loop.c, `!(GUI->flags & GUIF_CLOSE_PENDING)`).
   Opus stays open; the requester waiting on the screen close deadlocks.

## Fix
Clear GUIF_CLOSE_PENDING together with GUIF_CLOSED in display_open(). Rationale: an open display
supersedes a pending close.

Why this is safe (all uses of the flag, event_loop.c): set at 621 (DN_CLOSE_WORKBENCH); cleared at 501
(MAIN_CLOSEWB declined), 642 (DN_OPEN_WORKBENCH) and 931 (open timer). In the normal path
MAIN_CLOSEWB sets GUIF_CLOSED at 490 (after close_display), and 642/931 call display_open and then clear
both flags, so clearing earlier in display_open changes nothing for them. DN_OPEN_WORKBENCH only acts if
CLOSE_PENDING is set and CLOSED is set; after an early display_open both are already clear and the
display is already open, which is the desired end state. Only residual: reset reopens between 621 and
the dopus_closewb reply; then MAIN_CLOSEWB (ok) closes the display anyway, as before.

## Tested
Source review only. Not compiled locally (no toolchain/docker here), not run on hardware. Diagnosed from source (see s85 evidence); not
reproduced or verified on a rig. CI builds on PR.
