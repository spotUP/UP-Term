---
date: 2026-10-02
topic: ls ships, winch guard, and the close-path crash still open
tags: [ls, vshrc, ixemul, fileutils, dircolors, winch, close_window, crash, styling]
status: draft
---

# Handoff: `ls` is the real one; the close crash is still open

## Where things stand

`feature/prefs` is pushed to `c0e09c9`. Five commits today, all on that
branch: `df13767`, `a862085`, `977ba57`, `90876fb`, `c0e09c9` (on top of
`f43dd98`, the config-in-a-worker fix).

`make test` is green (13 suites, 1940 checks). The 68000 and 68020 builds
are clean. The parallel PTY/getty work in the tree is untouched and
unstaged — 22 files; do not stage them by accident, and note that
`Makefile` carries hunks from both this work and that work, which is why
only one hunk was committed from it.

## What was asked, and what is now true

The complaint was that `ls` in a window did not look like a Unix `ls`:
one entry red, the rest white. That was never the terminal. A terminal
renders escapes and cannot classify files. vshrc defined `ls()` as a
wrapper over AmigaDOS `Dir` and `List`, and those colour directories and
nothing else, so there was nothing for the terminal to show.

The kit now carries GNU fileutils 3.16 (`dist/gg/fileutils-3.16/`: `ls`,
`dircolors`, `COPYING`) and installs it as `C:ls` / `C:dircolors`.
`C:` is the answer because `resolve()` in `shell/vsh.c` searches
Resident → path as given → cwd → the CLI search path → `C:`, and never the
Unix `PATH` variable, which could not hold a DOS name anyway (`:` is both
separators). The wrapper is gone rather than overridden: a function
shadows a command, so leaving it would have hidden the binary and made
bare `ls` fail outright.

Verified on the rig:

| Check | Result |
|---|---|
| `which ls` | `ls is a command` |
| `ls --version` | `ls (GNU fileutils) 3.16` |
| `ls --color=always SYS:` | 85 SGR sequences, directories `01;34`, executables `01;32` |
| `ls SYS: >file` | 0 SGR — pipes and redirects stay clean |
| `ls ..` from `VTC:shtest` | lists `VTC:` (correct) |
| `ls ..` from `VTC:` | lists `VTC:` again — **clamped, see below** |

`dircolors` installs alongside so the palette is the user's to set;
`LS_COLORS` drives it and the ANSI numbers come from the window's profile.

## The thing that will bite the next session: `..`

The plan originally said "the `..` gate passed, retire the other shims".
**That was wrong**, and the correction is committed as `c0e09c9`.

Under ixemul, `..` drops one path component and **clamps at the assigned
root**:

```
cd VTC:; ls .      -> VTC: contents
cd VTC:; ls ..     -> VTC: contents (unchanged)
cd VTC:shtest; ls .. -> VTC: contents (correct)
cd VTC:; ls /      -> BOOTX, Ram Disk, Rushhours, System, VTCX
```

The shims' `_amiga` translated `../x` to `/x`, where `/` is the parent of
the assign. So the same typed path used to reach the volume list and now
reaches the assign root, and the user had been trained by the shim's
answer.

Two consequences, neither of which the plan anticipated:

1. **`ls ..` changed meaning** when the real `ls` shipped. That is a real
   behaviour change, not a no-op. `/` still reaches the old target.
2. **The remaining shims must not simply be deleted.** They are the only
   thing making `..` mean `/`.

The open choice, which is the owner's: keep the shims (`ls` real and
colourful, everything else unchanged, at the cost of `ls ..` and
`cp ../x` disagreeing), or convert each shim into translate-then-exec
(`_amiga` then `command cp ...`), which keeps `..` as `/` *and* gives Unix
semantics — at the cost of quoting a variable argument list in a POSIX
shell, which is the risky part.

`cat` stays regardless: it is textutils, not fileutils.

## The close-path crash: localized, guarded, NOT fixed

What is known, and it is solid:

- The trace stops at the top of `close_window`, and skipping the single
  `DoIO(IND_REMHANDLER)` (handler/vtcon_handler.c) once let both windows
  close cleanly with the agent alive.
- That call is structurally the same as ixemul's own
  `__ix_remove_sigwinch` (`library/ix_sigwinch.c:112-123`), so the
  mechanism is not the problem.
- The divergence is that *our* handler injects an event naming
  `c->w.win` and runs off input.device's 10 Hz timer, so it can fire while
  `close_window` dismantles that window.

`a862085` ships a correctness guard for that: `winch_closing` is set as
`close_window`'s first statement and `winch_handler` returns the chain
untouched after it; `open_window` clears it, because `close_window` is
called from inside `open_window`'s failure path and without the reset a
retried open would get a permanently deaf SIGWINCH handler.

**It is not a cure.** The same resize-then-close passes on a build without
the guard. `tools/rig/autoprobe_rig.py`'s docstring records that this path
struck about one run in three. The crash needs a reproduction before
anyone can fix it, and the SERIAL trace's last line is where to look.

Two traps in that work, both of which cost hours today:

- `post_sizewindow` — which allocates `input_io` and calls
  `IND_ADDHANDLER` — is reached **only** from `h_resized`
  (handler/vtcon_handler.c:350-354). A window that was never resized has
  `input_io == 0` and skips the `DoIO` entirely, so closing such a window
  proves nothing about this bug. Resize first: the zoom gadget
  (`gclick <title> sys:zoom`) is a reliable single-click resize, where
  synthetic drags on the size gadget did not take.
- `uitree` and `gclick` only see the **front screen**. With Directory
  Opus's MagicScreen in front, an XCON window is invisible to the harness
  even though it exists. `screen-to-back` is not on this system; check the
  screen name in the uitree `S` line before concluding a window is gone.

## Environment facts worth keeping

- ixemul on the rig is the project's **patched 48.2**
  (`~/Code/ixemul-vtcon`, HEAD `ddd6e22`, newer than the `5f2d856` the
  research doc cites). `post_sizewindow`'s input.device handler is ours.
- Windows take **1-2 minutes** to appear after `newshell`. Polling for
  30-60s and concluding "never opened" produced several wrong conclusions
  today, including one that the rig had wedged.
- The rig installs via `VTC:runkit`, which does `CD VTC:distkit` then
  `Execute Install` — so the script that runs is
  `build/rig/vtc/distkit/Install`, not `dist/Install`, and relative
  `Copy` names resolve **inside `distkit/`**. Staging into
  `build/rig/vtc/` alone does nothing.
- `vsh` reads `ENV:vsh/vshrc` (shell/vsh.c:1186) and then `$HOME/.vshrc`.
  `Install` writes both `ENVARC:vsh/vshrc` and `ENV:vsh/vshrc`; update
  both or the old copy keeps winning. The `$HOME/.vshrc` read is why the
  kit needs the `HOME:` guard: nothing assigns `HOME:`, and an
  interactive `vsh` stops at startup with "Please insert volume HOME".
- `vsh` is a **POSIX shell** (ixemul `sh`), not zsh: no `$'...'`, no
  `[[:space:]]`, no `print`. One zsh-only construct makes the whole file
  fail to parse and every function in it disappears.
- Every `vsh -c` prints `vsh: /dev/null: cannot create`. Cosmetic noise
  from the shell, present in every rig test.

## Next steps, in the order I would take them

1. **Decide the shim question** (keep, or translate-then-exec). It is the
   only thing blocking a consistent Unix environment, and it is a product
   decision, not an engineering one.
2. **Get a reproduction of the close crash** — resize a window with the
   zoom gadget, close it, repeat several times with a reboot between, as
   `autoprobe_rig.py` advises. Then the guard can be either confirmed or
   replaced with the real fix.
3. **Phase D: the 48.3 ixemul rebase.** coreutils 5.2.1-9
   (`dev/gg/coreutils-bin-src`, ~80 tools, and textutils, which would
   retire `cat` too) needs ixemul **48.3**; the rig has 48.2. It is one
   revision away, which makes this the cheapest large win available.
   Start here rather than the 80.x rebase, which
   `research/2026-09-30_ixemul-80.1-vs-our-48.2-patches.md` prices at
   6 hand reworks, an SCM_RIGHTS rewrite, a waiter list in both handlers,
   and an unresolved freeze in the target fork. That doc is otherwise
   sound but cites a stale HEAD.
4. Housekeeping: ~25 probe files on `VTC:` (`c_* d_* e_* g_* q_* r_* v_*
   w_* x_* y_* z_* dot_* sh_log`, plus the `shtest` tree) — `build/rig/vtc`
   is host-mounted, so they can be deleted from the host rather than over
   TCP. Also the `/dev/null` noise above.

## Not addressed, still true

Amiga files default to `----rwed` and the `e` bit maps to Unix execute, so
the real `ls` paints a great many files green. That is accurate reporting,
not a bug, and no coreutils version fixes it; `chmod` or an `LS_COLORS`
tuning is the answer.