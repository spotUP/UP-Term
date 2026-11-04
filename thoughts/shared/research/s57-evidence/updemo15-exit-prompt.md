---
date: 2026-11-05
topic: UPDEMO15-EXIT-PROMPT-TOP
status: fixed (host), not yet eye-checked on a rig
---

# UPDemo 15: shell prompt lands on row 1 after the demo

## Symptom (rig 96, main 7cf4ac17, flat mode, VTC:vsh in XCON)

After `UPDemo 15` ends, row 1 shows `Empty-12MB-FFS: %` with the cursor after it. The rows
below hold the reflow scene's paragraph, ending "(the size is in the title bar; Q quits the demo)".

## Which scene the 15 path ends in

`UPDemo n` starts at index n-1 (14, kitty animation) and plays to the end of the list. That
passes through scene "reflow", the only scene with `main_screen = 1`. The reflow scene pushes the
old main-screen text into the scrollback, writes its paragraph from row 1, and leaves the cursor
on the row below the text.

## Cause and layer

Layer: UPDemo (`demo/updemo.c`, `play()`). The engine and the handler are correct.

After each scene, `play()` sent `ESC [ r` (DECSTBM reset). DECSTBM homes the cursor, as the
standard says. After the reflow scene the cursor was on the main screen, so the reset moved it
to (0,0). The next alt-screen scene's `?1049h` saved that (0,0), and the final `?1049l` restored
it, so the shell printed its prompt on row 1 over the reflow text. A quit inside the reflow scene
gives the same result (the teardown runs, and no `?1049l` follows to restore anything).

Violated invariant: the cursor of a main-screen scene is where the shell's output continues.
The demo's teardown must not move it.

Host probe before the fix (80x24, `updemo_run(..., 14, 6)`): cursor 0,0, last text row 8,
row 0 "Reflow on resize".

## Fix

`play()` sends the per-scene `ESC [ r` only after alt-screen scenes. A main-screen scene sets no
scroll region, so nothing needs to be reset there. The next scene's head still resets the region
after it switches to the alt screen.

## Test

`tests/test_updemo.c`: `the_prompt_after_the_demo_comes_below_the_main_screen_text`. Two passes:
the full run from 15 (index 14), and a run of the reflow scene alone. Each pass checks that the
main screen is active, that row 0 is "Reflow on resize", and that the cursor is in column 0 on a
row below the last text row. With the fix reverted, both passes fail (`y > last`). With the fix,
the updemo suite passes 8707 checks.

## Rig

No rig was started. The host harness reproduced the bug. The owner's eye check on a fresh build
of main is still open.
