---
date: 2026-10-12
topic: Return held at the vsh prompt - blank rows, then prompts piled on one line
tags: [handler, lineedit, typeahead, vsh, rig]
status: implemented
---

# Return held at the vsh prompt

## Symptom (owner, FS-UAE rig, window "vsh enter")

Holding Return at the vsh prompt: the upper rows show one prompt a row with an
empty row between each; the bottom rows show about 19 prompts written one after
another on a line ("BOOTX: % BOOTX: % ...", wrapping). A single slow Return is
fine. The owner later saw a clean run: the bug depends on timing.

## Who writes the newline

vsh reads its command line in cooked mode (`FGets(Input())`, shell/vsh.c:2300),
so the line editor is the handler's (handler/lineedit.c). `le_key` on Return
writes `\r\n` at once (lineedit.c:1214) and the handler queues the line for the
reader. vsh then writes its prompt (OSC 133;A, the prompt, OSC 133;B: four
Writes) and sends its next Read. Before the fix the handler fed every key to the
editor the moment it was typed, whether or not a reader waited.

## Measurement (rig 3, DEBUG handler with temporary traces)

Traces per event: the Return key (with `nreads`, the Reads waiting), the
editor's echo, each Write (its last 4 bytes), each Read and its reply.
Returns injected with amiagent INPUT op 8 SCRIPT (key down, 1 tick, key up,
`gap` ticks):

| burst | Returns typed while no Read waited |
|-------|-------------------------------------|
| 20 Returns, gap 2 ticks | 0 of 20 |
| 20 Returns, gap 0 ticks | 18 of 20 |

The decisive sequence (gap 0):

    REPLY n/b0 00000001 0000000a          <- line k to vsh
    KEY ret nreads/inlen 00000000 ...      <- next Return, nobody reading
    ECHO n/b0b1 00000002 00000d0a          <- its CR LF on screen now
    WRITE n/last4 00000013 6d202520        <- only then vsh's prompt "m % "

The CR LF of a typed-ahead Return goes out before the prompt of the line it
answers: a row with no prompt, then the prompt; the next Read finds the line
already queued, so vsh writes the next prompt straight after it on the same
row. Echo count equals prompt count; only the order is wrong. A key-repeat
period near or above 60 ms keeps vsh ahead (the owner's clean run); faster
repeat, or a slower moment in vsh, does not.

The scripted held key (down, 100 ticks, up) gave no Amiga key repeat on the
rig, so the bursts above stand in for it.

## Root cause

The handler's cooked line editor runs on keys at typing time, not when a reader
asks. A Unix line editor (readline, zle) reads its typeahead from the tty
queue after it has drawn the prompt, so each line's newline follows its prompt.

## Fix (the layer that owns the echo: the handler's cooked line editor)

- handler/lineedit.c `le_ahead_*`: a queue of keys that wait for a reader
  (one record a key: length, modifiers, key code, bytes; 16 KB; full -> the key
  is dropped with a beep, as a full tty input queue does).
- handler/vtcon_handler.c `typed` / `ahead_hold`: a key for the editor waits,
  unechoed, while no Read or WAIT_CHAR waits, or while keys wait before it. The
  break keys (Ctrl-C, E, F, Ctrl-\, Ctrl-Z) still act at once; Ctrl-C also
  empties the queue (a tty flushes its input on an interrupt). Ctrl-D waits its
  turn, since what it does depends on the line before it. Raw-mode keys go at
  once unless keys still wait. Pasted text takes the same path.
- `ahead_feed`: when a reader asks (ACTION_READ, ACTION_WAIT_CHAR including
  polls, ACTION_VTCON_NREAD) the waiting keys go to the editor until a line is
  finished for that reader; the rest wait for the next prompt.
- `tty_enter`: keys still waiting go to the line discipline (echoed by it).

Behaviour change: text typed while a command runs (no reader) now appears after
the next prompt instead of at once, as under bash/zsh.

## Verification

- Host: `make test ONLY=lineedit`: `typed_ahead_enters_each_start_a_new_line`
  (5 Returns, 3 typed while the shell is busy: one prompt a row), plus
  `a_line_typed_ahead_is_drawn_after_the_next_prompt` and
  `typeahead_queue_drops_a_key_only_when_full`. With the hold disabled the first
  fails with row 1 = "% % % %" and empty rows, the owner's picture.
- Rig 3, DEBUG build with the fix: 104 Returns (3 slow, 1 held, 100 in gap-0
  bursts), every one in the order prompt Writes -> READ -> KEY -> echo -> reply.
- Rig 3, release build: intr_rig line/raw/vsh OK, shstdin_rig PASS,
  histfile_rig PASS.

## Not this bug

The stopped agent's worktree held an unverified jump-scroll refactor
(render/jumpscroll.h, `vt_jump_key`) for another symptom ("starts from the
top when it reaches the bottom"). It is not part of this fix; it is kept in
`git stash` on this worktree ("prev agent jumpscroll refactor (unverified)").

`make test-rig` fails on main before this change: tools/rig/reach.py:13 uses
`paths.RIG` before `import paths` (line 15).
