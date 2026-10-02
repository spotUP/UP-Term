---
date: 2026-10-02
topic: the Unix userland in an UP-Term window: real ls, and retiring the vshrc shims
tags: [vsh, vshrc, ixemul, geek-gadgets, coreutils, fileutils, dircolors, ls, styling]
status: draft
---

# The Unix userland in an UP-Term window

## Why `ls` looked wrong, and why the terminal was never the cause

A terminal renders the escapes a program emits. It cannot classify files,
so when `ls` painted only directories blue and everything else in the
foreground colour, the cause was upstream: vshrc defined `ls()` as a
wrapper over AmigaDOS `Dir` (short) and `List` (long), and those colour
directories and nothing else. The per-type colouring people expect from a
Unix `ls` — directories, executables, archives, images — has to be emitted
by the program that does the listing.

Two wrong turns worth recording, because both were measurements that
looked like evidence:

- **`--color` did nothing, and it was not a parser bug.** `ls` was a
  *function*, and a function shadows a command, so no amount of escape
  handling would have helped. The wrapper had to go, not be overridden.
- **`ls` "missing from the path" was never a path problem.** `PATH` is
  irrelevant to vsh. `resolve()` (shell/vsh.c) searches, in order:
  a Resident, a path as given, the current directory, the Shell's CLI
  search path, then `C:`. It never consults the Unix `PATH` variable, and
  `PATH=GG:bin` cannot work anyway because `:` is both the DOS device
  separator and the `PATH` separator. `C:` is the answer, and it is the
  pattern the kit already used for `vsh`, `ixkill`, `screen` and `tmux`.

## What is installed

`dist/gg/fileutils-3.16/` — GNU fileutils 3.16 from Geek Gadgets: `ls`,
`dircolors`, and the archive's `COPYING` (GPL v2). `Install` copies them to
`C:ls` and `C:dircolors`, each behind `If NOT EXISTS` so a machine that
already has its own is left alone. `vshrc` aliases `ls` to `--color=auto`.

`dir` and `vdir` are deliberately **not** shipped. `Dir` and `List` are
resident commands and `resolve()` finds Residents before `C:`, so those two
would sit in `C:` and never run — and they would also shadow nothing useful.

Note for anyone updating this: `vshrc` is written for `vsh`, which is a
POSIX shell (ixemul `sh`), **not** zsh. `$'...'`, `[[:space:]]` and `print`
are all unavailable — `print` is "not found" and `$'x'` stays literal. A
zsh-only construct makes the whole file fail to parse, which loses *every*
function in it, not just the offending one.

## Verified on the rig

| Check | Result |
|---|---|
| `which ls` | `ls is a command` (sentinel: the shim is gone) |
| `ls --version` | `ls (GNU fileutils) 3.16` |
| `ls --color=always SYS:` | 85 SGR sequences; directories `01;34`, executables `01;32` |
| `ls SYS: >file` | 0 SGR sequences |
| `ls ..` | lists the parent directory |

## `..` does NOT mean what the shims made it mean

This was measured after `df13767` shipped, and it reverses the plan this
document originally carried. Do not retire the remaining shims on the
strength of "ls .. worked".

| command | result |
|---|---|
| `cd VTC:; ls .` | VTC: contents |
| `cd VTC:; ls ..` | **VTC: contents again** |
| `cd VTC:shtest; ls ..` | VTC: contents |
| `cd VTC:; ls /` | `BOOTX`, `Ram Disk`, `Rushhours`, `System`, `VTCX` |

So under ixemul `..` drops one path component and **clamps at the assigned
root**: from `VTC:shtest` it goes to `VTC:`, but from `VTC:` it stays at
`VTC:`. The shims' `_amiga` translated `../x` into `/x`, where `/` is the
parent of the assign -- the fourth row, the volume list. Same typed path,
two different answers, and the shim's is the one a user of this system had
been trained by.

Two consequences:

1. **`ls ..` changed meaning when the real `ls` arrived.** Someone who
   typed `ls ..` to reach the volume parent now gets the assign root. `/`
   still reaches it. This is a real behaviour change, not a no-op, and it
   is the cost of not shipping a wrapper.
2. **The shims cannot simply be deleted.** They are the only thing that
   makes `..` mean `/`. Deleting `cp`, `mv`, `rm`, `mkdir` and `touch`
   would lose that, not just the name translation.

That leaves the honest options, and the choice is not mine:

- **Keep the shims.** `ls` is real and colourful; everything else keeps
  its old meaning. The cost is the inconsistency: `ls ..` and `cp ../x`
  now disagree.
- **Translate-then-exec.** Turn each shim into a wrapper that applies
  `_amiga` and then calls the real binary (`command cp ...`), so Unix
  semantics *and* `..` as `/` both hold. More shell to get right, and
  quoting a variable argument list in this POSIX shell is the risk.

Until that is decided, `cat` and the five shims stay as they are.

**MEASURED LATER THE SAME DAY (tests/amiga/dotdot on the rig): the premise
was wrong. ixemul does not clamp `..`.** From the root of System: or Ram Disk:
`..` is `/` (the volume list), as on Unix. Only `VTCX:` -- FS-UAE's
host-directory filesystem behind `VTC:` -- answers its own root as its
parent; on a real Amiga's FFS/PFS volumes `..` already works. So the
`..`-as-`/` argument for the shims is a rig artefact, and they can retire
for fileutils `cp mv rm mkdir touch` (rig checks of `..` must use a real
volume, not VTC:). The probe did find a real ixemul bug, fixed in
ixemul-vtcon: after `cd /`, Amiga paths (`SYS:C`) failed.

The earlier decision, kept for the record: **(lead, 2026-10-02; the owner delegates design decisions): neither
option -- fix `..` where it is wrong, in our patched ixemul.** On Unix the
parent of a mount point is the directory above it; under ixemul an assign
`VTC:` is `/VTC`, so `..` from its root must be `/` (the volume list), and
ixemul's clamp at the assign root is the deviation. Fixed there, every Unix
program agrees (`ls ..`, `cp ../x`, `cd ..` in tcsh, tmux's paths), and the
shims can then go the way `ls` did, replaced by fileutils `cp mv rm mkdir
touch` (3.15, the Aminet pair whose source we ship). Translate-then-exec
would have fixed five commands and left every other Unix program clamped.
Order: the ixemul `..` patch (with a rig probe: `cd VTC:; ls ..` lists the
volumes), then retire the shims, then the 48.2 -> 48.3 rebase for
coreutils (which carries the same patch forward).

## Next: settle the shims (not "retire" them)

`vshrc` still wraps `cp`, `mv`, `rm`, `mkdir`, `touch` and `cat` over
AmigaDOS commands. fileutils 3.15 covers the first five, so they *could*
go the way `ls` did -- but the section above shows they must not, because
they are what makes `..` mean `/`. The work is to decide between keeping
them and converting them to translate-then-exec.

## Why not a newer coreutils yet

`dev/gg/coreutils-bin-src` (5.2.1-9, 2010, ~80 tools) needs **ixemul 48.3 or
newer**; the rig runs the project's patched **48.2**. So it is one revision
away, and that is the whole of the gap.

Two routes, and the difference matters:

- **Rebase the patched tree onto 48.3.** Small delta onto a tree we already
  patch. Unblocks coreutils 5.2.1 immediately.
- **Rebase onto ixemul 80.x.** Already researched in
  `2026-09-30_ixemul-80.1-vs-our-48.2-patches.md`: 3 patches drop, 6 carry
  over, 6 need hand rework, SCM_RIGHTS needs a rewrite on 80's refcounted
  streams, both handlers need a waiter list, the build script changes, and
  the target fork is five days old with an unresolved freeze in its own
  BUGS file. Worth doing eventually; not a small job.

Start with 48.3. The 80.x rebase is a separate piece of work with a
separate risk budget.

Note that the 80.1 research doc is stale in one respect: it cites
`~/Code/ixemul-vtcon` at `5f2d856`, and the tree has since moved to
`ddd6e22` (per-process malloc cache, real `FIONREAD` on a vtcon console or
PTY:, gcc-3 `%zu` handling).

## The e-bit caveat

On AmigaOS a file's default protection is `----rwed`, and the `e` bit maps
to Unix execute. So a real `ls` paints a great many files green, because on
this machine most of them *are* marked executable. That is accurate
reporting, not a bug, and it is not fixed by a newer coreutils — `chmod` is.
`LS_COLORS` (via `dircolors`) can retune the palette but not the
classification.

## Open, unrelated to this plan

The close-path crash is **not** fixed. See the `a862085` commit message: the
trigger was localized to the `DoIO(IND_REMHANDLER)` in `close_window`, but
the same resize-then-close sequence now passes both with and without the
`winch_handler` guard, and `tools/rig/autoprobe_rig.py` records that this
path struck about one run in three. It needs a reproduction before it can be
fixed, and the SERIAL trace's last line is the place to start.