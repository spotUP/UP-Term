---
date: 2026-10-07
topic: "Amiga shells as bash-compat candidates: abcsh (pdksh) and CShell (Matt Dillon csh), against the vsh plan"
tags: [cshell, csh, abcsh, pdksh, shell, vsh, fork, vfork, ixemul, amiga, research]
status: draft
---

# Amiga shells: abcsh and CShell (renamed from 2026-10-07_abcsh.md on 2026-10-07; CShell and the combined verdict are sections 7 and 8)

Source: `git clone --depth 1 https://github.com/adtools/abcsh` into a fresh scratchpad directory,
HEAD 145cae0 (2017-07-02). Read as text only; nothing built, no scripts run. Aminet
`util/shell/abc-shell` v53.3 was not downloaded; the repo is v53.4 (`sh.h:7`
`#define ABC_VERSION "53.4"`, `distro/README.amigaos` changelog "broadblues - 53.4"). The repo is a
flat tree (no `src/`); paths below are relative to the repo root.

## 1. Base, license, size

- **Base: pdksh 5.2.14** (CONFIRMED). `old/README:3` "Last updated Jul '99 for pdksh-5.2.14";
  `old/ChangeLog:3` "made pdksh-5.2.14 distribution". Later patches: OpenBSD ksh bugfixes
  (`distro/README.amigaos`, "53.1 Applied bugfixes from the OpenBSD ksh"), clib2 `qsort()` instead
  of pdksh's (53.2).
- **License: public domain, with one exception** (`LEGAL:3-17`): "The vast majority of the code
  that makes pdksh is in the public domain." Exception `sigact.c`/`sigact.h` (Simon J. Gerraty):
  free, no warranty, keep the notice, comment your changes. No GPL anywhere in `LEGAL`. There is
  no COPYRIGHT file (`ls` shows only `LEGAL`). Per the owner's no-LICENSE rule, nothing to add on
  our side beyond keeping the `sigact` notice if that file were copied. The AmigaOS glue
  (`amigaos.c`) carries no licence header that I saw (UNCONFIRMED: I read lines 1-30 and the
  fork/exec parts, not every line).
- **Size: 18,860 lines in `*.c`; 20,611 lines in `*.c` plus `*.h`** (`cat *.c | wc -l`,
  `cat *.c *.h | wc -l`). Of that, the Amiga layer is `amigaos.c` 855 lines; the touched pdksh
  files are `exec.c` 1778, `jobs.c` 1226, `main.c` 737. Test scripts in `tests/*.t` (pdksh's own).
  For comparison, vsh's `shell/` is 6,277 lines for parse+expand+exec (research
  `2026-10-07_bash-port-fork-sites.md` section 6) plus `vsh.c` 1,608.

## 2. How it does fork, subshells, pipelines, `$( )`, `&`

**There is no fork.** `amigaos.c:183-188`: `fork()` prints "fork() not implemented", sets
`ENOMEM`, returns -1. `wait()` likewise (`amigaos.c:190-195`); `alarm()`, `pause()` are stubs
(`amigaos.c:135-141`, `~650`). Instead pdksh's `exchild()` (`jobs.c:191`, which calls `fork()` at
`jobs.c:248`) is **replaced** by an Amiga `exchild()` at `amigaos.c:685` (the Makefile builds
`jobs.c` out of the way: `SRCS` in `Makefile` lists `jobs.c`? No: it lists `amigaos.c ... exec.c ...
jobs.c`; UNCONFIRMED which of the two `exchild` definitions is compiled out; `jobs.c:191` is
presumably under an `#ifndef AMIGA`, I did not check).

Mechanism (`amigaos.c:685-800`):

1. Packs `{struct op *t, flags, parent Task}` into a stack struct `userdata` (`:642-647`, `:709-712`).
2. `CreateNewProcTags(NP_Entry execute_child, NP_Child true, NP_Input/Output/Error = the shell's
   current fds via __get_default_file, NP_Close* false, NP_Cli true, NP_StackSize = parent's)`
   (`:734-751`). A new AmigaOS **Process in the same address space** as the shell.
3. `execute_child` (`:652-682`): reads the packed op from `tc_UserData`, calls
   `copyenv(&globenv)` (`:665`), sets `e->type = E_SUBSHELL`, `ksh_sigsetjmp`, **calls `execute(t,...)`**
   (the pdksh tree walker, in the child process, on the parent's heap and globals), then
   `restoreenv()` (`:675`), then signals the parent.
4. **The parent blocks** until the child finishes: OS3 branch `Wait(SIGBREAKF_CTRL_F)` (`:757`),
   OS4 branch `Wait(SIGF_CHILD)` (`:760`). `exchild()` returns `lastresult`, a global.

`copyenv`/`restoreenv` (`exec.c:1614-1727`): saves the pointers to the parent's globals (`struct
globals`, `sh.h:510-523`: perm area, env, alias/home tables, path, cwd, fd table, ctypes, sigtraps),
allocates a fresh perm Area and **copies the tables**, then frees the copy on return. It is a
snapshot/restore of the shell's state, not a second interpreter instance: parent and child share
the same C globals, so it is only safe because the parent is asleep.

Consequences (derived from the code above; NOT run):

- **Async `&` is not asynchronous.** `exchild()` always waits; `XBGND` appears only in a debug
  `printf` in `amigaos.c:707`. A background command runs to completion first. (UNCONFIRMED: I did
  not read the `jobs.c` side for a path that bypasses `exchild()` for `&`.)
- **Pipelines run stage by stage, never concurrently.** `exec.c:158-163` calls
  `exchild(t->left, flags|XPIPEO|XCCLOSE, pv[0])`, which blocks until the left stage ends, and
  only then does the right stage start. A producer that writes more than the pipe holds blocks
  forever with the parent waiting for it: `pipe()` is `/PIPE/%x%08x/4096/0` on OS3 and
  `/PIPE/.../32768/0` on OS4 (`amigaos.c:143-176`, 4 KB / 32 KB buffer). The `USE_TEMPFILES`
  alternative (`amigaos.c:147-152`, `exec.c:145`) is **not** defined in `Makefile`, so the shipped
  binary uses PIPE:. Expected result: `cmd | cmd` works for small data and can deadlock for large
  data. Inferred from code only.
- **`$( )`** (`eval.c:860-885`): no child at all for `$(< file)`; otherwise `openpipe`, dup the
  pipe write end onto fd 1, `copyenv`, `execute(t, XXCOM|XPIPEO)` **in the same process** under a
  `sigsetjmp`, `restoreenv`, restore fd 1. Same buffer limit: the output is written fully before
  the shell reads any of it (a `$( )` that prints more than the pipe buffer deadlocks; inferred).
- **External commands** (`execve`, `amigaos.c:393-620`): builds one command line string, quotes
  arguments, handles `#!` interpreters by hand, `createvars(envp)` pushes the exported variables to
  AmigaOS local vars, and runs the program **inside the shell's own process** with
  `RunCommand(seglist, stacksize, args, len)` (`:~580`, clib2 path, `__USE_RUNCOMMAND__` defined
  unless `NEWLIB`, `:20-22`), or `SystemTags(full, SYS_UserShell true, NP_StackSize, SYS_Input/Output/Error)`
  (`:559`) in the other build. So `exec` returns (`errno = ENOMEM` after, `:~625`) and the caller
  treats the result as the child's.
- `sigact.c`, `trap.c`, `tty.c` are pdksh's POSIX signal layer; no real signal delivery to a
  "child" exists, `kill` of a job has nothing to kill (UNCONFIRMED, not traced).

**C library and API level.** Default is **clib2** (`Makefile`: `CRT = clib2`, `CC =
ppc-amigaos-gcc`, `-mcrt=$(CRT)`, `LIBS = -lnet -lm -lunix`); newlib is the alternative
(`-lunix` only). ixemul is not mentioned anywhere (`grep -ci ixemul` not run on every file;
Makefile and `amigaos.c` have none). Compiler is the AmigaOS 4 cross GCC (`ppc-amigaos-gcc`),
PowerPC. OS4-only pieces: `NP_NotifyOnDeathSigTask`, `NP_UserData`, `SIGF_CHILD` (`:747-760`,
`#ifdef __amigaos4__`), `GetSegListInfoTags` with `GSLI_Native/68KPS/68KHUNK`, `struct
PseudoSegList` (`:~570`), 19 `amigaos4` conditionals in total (`grep -c amigaos4`). There are
`#ifndef __amigaos4__` branches that use OS3-style calls (`tc_UserData` assignment, `Wait(CTRL_F)`),
so the author kept the OS3 shape alive in places, but the Aminet package says ppc-amigaos >= 4.0
and the Makefile builds PPC only. UNCONFIRMED: whether the `!__amigaos4__` branches have ever been
built (the `#warning this code has been included!` at `:755` suggests a rough path).

## 3. Features pdksh lacks versus bash

From `old/README`, `old/NOTES:1-12` and `old/ksh.Man` (grep), pdksh 5.2.14 has: **indexed arrays**
(`set -A`, `old/NOTES:101`), **`$(( ))`** (`ksh.Man:548`), **`[[ ]]`** (`ksh.Man:406-447`), variable
attributes, co-processes, extended globbing, `$( )`, aliases, vi/emacs editing.

It lacks (from `old/NOTES:1-12` plus general knowledge of bash; items from NOTES are CONFIRMED,
the bash comparison is mine and was not run against an oracle):

- **associative arrays** (bash 4+): absent (pdksh arrays are indexed only; not in NOTES either way).
- **process substitution `<( )` / `>( )`**: absent (not in `ksh.Man`; UNCONFIRMED by exhaustive grep,
  only `<(` and `process subst` searched).
- **brace expansion**: present (`ksh.Man:569` mentions "brace expansion"), but not bash's `{1..5}`
  ranges (UNCONFIRMED).
- **POSIX character classes in globs** `[[:alpha:]]` (`old/NOTES:7`).
- **`=~` regex in `[[ ]]`, `${v//x/y}`, `${v:off:len}`, `${!v}`, `${v^^}`, `declare`/`local -n`,
  `mapfile`, `printf -v`, `shopt`, `$'..'` quoting, `<<<`, `&>`, `{fd}>`, `PIPESTATUS`,
  `BASH_REMATCH`, `(( ))` as a command, `for ((;;))`, `**`**: not looked for; known absent in pdksh
  5.2.14 by my knowledge, UNCONFIRMED here.
- `lastpipe` the other way: pdksh runs the last pipeline stage in the child ("the last command of a
  pipeline is not run in the parent shell", `old/NOTES:11`), like bash with `lastpipe` off.
- no `trap DEBUG`, no per-function `ERR`/`EXIT` traps (`old/NOTES:5-6`).

So abcsh is a ksh88/POSIX shell. It is further from bash than vsh plan Phase 1-6 targets, not
closer: nearly every item in plan Phases 2-4 (assoc arrays, `${ }` operators, `=~`, process
substitution, 64-bit arithmetic) would still have to be written.

## 4. Reuse in vsh, or a port to OS3 68k with ixemul-vtcon

**Reuse of the mechanism in vsh: no gain.** vsh already has a stronger version of the same idea:
`sh_shell_clone()` (`shell/sh_exec.c:169`) copies variables, args, status, umask, functions into a
**separate `sh_shell` struct**, and `os_spawn()` (`shell/vsh.c:1060`) hands it to a new Process
with a message port, with a `wait` flag (`:1062`, background branch opens `NIL:` for input,
`:1070-1075`). That is a real second interpreter instance running **concurrently**, so `&` and
pipelines overlap and a producer larger than the pipe buffer does not deadlock. abcsh shares one set
of C globals and serialises. The one abcsh trick worth knowing, `RunCommand()` of a loaded seglist
in the shell's own process, is a way to avoid a process per external command; vsh's own spawn path
is presumably equivalent (UNCONFIRMED: not read for this question).

**Port abcsh to OS3 68k with ixemul-vtcon** would change:

- Toolchain: PowerPC `ppc-amigaos-gcc` to our m68k cross compilers (`build295`, `buildgcc16`);
  `-lunix`, `-lnet` (OS4 libraries) replaced by ixemul. The clib2-only hooks
  (`__open_locale`, `__expand_wildcard_args`, `__execve_exit`, `__execve_environ_init`,
  `amigaos.c:20-135`) become ixemul equivalents or are deleted.
- Process model: `execve()` as a hand-built RunCommand/SystemTags launcher can go, because ixemul
  has `vfork`+`execve` (`library/vfork.c`, 1047 lines). That is, abcsh's `execve()` in `amigaos.c`
  (about 230 lines) would be dropped for ixemul's, but then `exchild()` has to be reconsidered:
  its in-process serial model only needs `CreateNewProcTags`, which is OS3-valid. OS4-only
  `NP_NotifyOnDeathSigTask` and `SIGF_CHILD` would use the existing OS3 branch
  (`Wait(SIGBREAKF_CTRL_F)`), which is the least-tested path in the repo.
- `pipe()` on `PIPE:` needs the pipe-handler present on 3.x; ixemul has its own `pipe()` already
  (UNCONFIRMED, I did not grep `library/` for it).
- Remaining defect after the port: serialised pipelines and `&` (section 2). To fix those you would
  need exactly the thing vsh has (a cloned interpreter per stage), which means reworking pdksh's
  globals (`struct globals` already lists the ones it knows about, `sh.h:510-523`; the real list is
  larger, `main.c`, `var.c`, `trap.c`, `jobs.c` all keep file-static state, UNCONFIRMED count).

**Effort (my estimate, not measured):**

- Port abcsh as-is to 68k + ixemul-vtcon, same semantics as today (serial pipes and `&`): **M**,
  about 1-2 weeks (toolchain, strip OS4 calls, test on the rig). Result: a ksh88 shell with the
  known abcsh limits, and no bash features.
- Port with real concurrency (clone-per-stage): **L**, 4-8 weeks, touches the interpreter's
  global state; and it ends with a shell vsh already is.
- Use the mechanism only as a model for vsh: **S**, under a day to read further; nothing to take
  that vsh does not already do better.

## 5. Against the plan and the fork-sites research

Read: plan headings and "Decisions" (`thoughts/shared/plans/2026-10-07-vsh-bash.md:1-75`);
research sections 4 and 6 of `2026-10-07_bash-port-fork-sites.md`. The research verdict, section 6:
"Finish the vsh plan. Do not port bash." Its reasons are (1) bash needs fork at 5 of 7 sites,
(2) no copying fork without an MMU, (3) vsh already clones the shell, (4) prior art.

What abcsh adds to that picture:

- It is **new prior art** for reason 4: a working Amiga ksh/POSIX shell with `$( )`, `( )`,
  pipelines and external commands and **no fork at all**, by running the interpreter in a second
  Process on shared globals while the parent sleeps. Research section 5 listed "state
  snapshot/restore, serialised subshells" as the in-process alternative and said it breaks
  "pipelines of two bash-code stages and `&`". abcsh confirms that diagnosis: it is exactly that
  design and exactly those breakages (serial pipes, synchronous `&`, buffer deadlock risk).
- It supports reason 3: vsh's clone-per-subshell design is better than the only shipping
  alternative.
- It does **not** supply bash features. pdksh 5.2.14 is the 1999 ksh88 line (assoc arrays, process
  substitution, `=~`, `${ }` operators beyond POSIX are missing, section 3).

**Verdict: unchanged. Ignore abcsh as a base; keep the plan.**

Options, with numbers (estimates):

| Option | Cost | Result | Fails to cover |
|---|---|---|---|
| A. Use abcsh's mechanism as a model for vsh | S, under 1 day | nothing new: vsh `sh_shell_clone` + `os_spawn` already supersede it | n/a |
| B. Port abcsh as the `sh` | M, 1-2 weeks | a POSIX/ksh88 `sh` with serial pipelines and `&`, 20K lines of pdksh to carry | the whole bash gap (0 of the plan's 190 V items), concurrency, and a second shell next to vsh |
| C. Ignore | 0 | plan stands | nothing |

**Recommended: C, with one cheap use of A:** keep the plan; when Phase 6 (traps, signals) and the
`lastpipe` item (V79) are written, treat abcsh's `tests/*.t` (pdksh's regression suite: `alias`,
`arith`, `heredoc`, `ifs`, `read`, `syntax`, `th-sh` harness; public domain per `LEGAL`) as an extra
**oracle-free POSIX sanity corpus** for the `bashdiff` harness (Phase 0). That is the one
asset in abcsh that costs less than it gives: roughly 18 test files, no code merged. Not verified
that the `.t` scripts run under a non-ksh runner (they need pdksh's `th` driver, `tests/th`); budget
S for adapting the ones worth keeping.

## 6. Unconfirmed

- Which `exchild` (`jobs.c:191` or `amigaos.c:685`) the build selects, and how `jobs.c` is guarded.
- Whether `&` ever bypasses the blocking `exchild`.
- Deadlock behaviour on a large `pipe` or `$( )`: inferred from `PIPE:` buffer size and the
  parent-waits design, never run (no rig, per instruction).
- Whether `abc-shell` v53.3 on Aminet matches the repo's v53.4 source for these files.
- Whether the `!__amigaos4__` code paths were ever built or run.
- Absence of `process substitution`, `{a..b}`, `[[ =~ ]]` etc. in pdksh: grep of a few spellings
  only, plus prior knowledge.
- ixemul/`pipe()` in `library/`: not grepped.

## 7. CShell (amigazen/CShell, Matt Dillon's csh)

Source: `git clone --depth 1 https://github.com/amigazen/CShell`, HEAD c966ecb (2025-08-30). Read as
text, nothing built.

- **Base:** Matt Dillon's csh, maintained by Kirchwitz and Mueller, now by amigazen
  (`LICENSE.md:27-29`). Version "5.60" in `CHANGELOG.md:37`; `src/smakefile` header says "Shell
  6.00M". A **csh-like** command shell, **not** sh/bash syntax: `foreach`, `if`/`else`, `label`/`goto`,
  `alias`, `source`, `history`, `rpn` are builtins (`src/execom.c:91-178`). I found no `while` entry
  in the builtin table with the grep I ran (UNCONFIRMED; grep of quoted names only). `TODO.md:5` lists
  "Implement modern csh/tcsh and bash style script execution" as not yet done.
- **License: BSD 2-Clause** (`LICENSE.md`), "Copyright (c) 2025 amigazen project"; the README says
  the redistribution terms of the original authors are in `COPYING`/`LICENSE.md` (no COPYING file
  in the tree listing; UNCONFIRMED what the 1990s terms were). Compatible with reuse.
- **Size:** 15,669 lines in `src/*.c`, 633 in `*.h`. Largest: `comm3.c` 2856, `comm1.c` 3160,
  `execom.c` 2033, `comm2.c` 1980, `sub.c` 1522, `rawcon.c` 1165, `run.c` 1069. Most of it is
  builtin commands (ls, rm, mkdir, chown, info, strings, ...), not interpreter.
- **C library / OS3 support: native AmigaOS 2.0+, SAS/C 6.58, no clib2/newlib/ixemul.**
  `src/smakefile`: `sc ... PARMS=REG ... UTILLIB`, `slink ... LIB:sc.lib`. README:118 "AmigaOS 2.0
  (or higher)"; CHANGELOG "chown/chgrp ... run under AmigaOS 2.0+" (`:200`). Uses only
  `proto/dos, exec, utility, intuition, graphics, gadtools, asl, diskfont, battclock`
  (`src/shell.h:57-65`). It is **68k and OS3-native today.** It does not build with our gcc
  cross-toolchains as-is (SAS/C pragmas, `PARMS=REG`, `UNSCHAR`, `MCCONS`); UNCONFIRMED how deep.
- **Process model (no fork, no clone):** it is a Unix-like **front end over the AmigaDOS CLI
  process model**, a single process interpreting lines.
  - External commands: `RunCommand(seglist, stack, args, len)` in the shell's own process
    (`src/run.c:814`), or `SystemTags(..., SYS_Input, SYS_Output)` for scripts
    (`run.c:773`); a comment at `run.c:703` notes RunCommand cannot run Execute-style files.
  - Background `&`: `SystemTags`-style tag list with **`SYS_Asynch, 1`** and NIL: handles
    (`src/comm3.c:899-935`, `do_truerun`, `backflag`). Real asynchronous AmigaDOS process, fire
    and forget; the shell cannot wait for it or collect its status (UNCONFIRMED, not traced).
  - **Pipes are temp files, strictly sequential**: `execom.c:1133-1141` assigns the left stage's
    output to a temp name `"%spipe%c%d_%lx"` (`execom.c:1927-1931`, directory from `_pipe`,
    `globals.c:44`), runs it to completion, then feeds that file as the right stage's stdin and
    deletes it (`execom.c:1441-1450`). Only 2 temp names alternate (`which = 1 - which`). A long
    producer never deadlocks; a `yes | head` never terminates (inferred).
  - **Command substitution** `` `cmd` `` and `^cmd^`: the line is lexed with markers
    (`HOT_APOSTR`, `execom.c:423-434, 595-598`) and executed through the same line-execution
    function recursively (`execom.c:919-927`); no process is created (UNCONFIRMED: I read the marker
    handling, not the capture code).
  - No subshell `( )` concept was found (UNCONFIRMED; the shell has no copyable state struct, it
    uses static globals, `src/globals.c`).
- **Features vs bash:** csh syntax (`set x=..`, `foreach`, `if (...) then`), `alias`, `history`,
  command-line editing and filename/command completion (`rawcon.c`, 1165 lines, AUX: and console
  modes), Amiga-specific builtins and "object oriented file classes", RPN expressions
  (`rpn`). No bash/POSIX `sh` syntax at all: `$( )`, `$(( ))`, `[[ ]]`, arrays-by-`${a[i]}`, functions
  `f() { }`, here-documents, `2>&1` are not in csh in the bash form (csh has `>&`). Scripts written for
  bash do not run. Zero of the plan's POSIX/bash-syntax items are met.
- **Reusable parts for vsh:** (a) the **interactive layer** (`rawcon.c`: raw console line editor,
  completion, history) is relevant to plan Phase 7, but vtcon's own console handler is meant to own
  that (plan "Console-handler features last"); (b) the **SYS_Asynch + NIL:** background spawn pattern,
  which vsh's `os_spawn` already covers with its own process and message port (`shell/vsh.c:1060-1075`);
  (c) the builtin command bodies (ls, protect, info, strings...) under BSD-2: useful only if vsh
  wants Amiga-specific builtins; they depend on SAS/C and `shell.h` globals. The temp-file pipe
  is exactly the plan's stated decision for process substitution ("temp file first", plan "Decisions")
  and is weaker than vsh's concurrent stages.
- **Effort:** adopt CShell as the shell: **S** to run it (it already runs on OS3 68k), but it is a
  csh and delivers 0 bash compatibility. Port it to bash syntax: **L**, it is a rewrite of
  `execom.c`/`sub.c` (3.5K lines) with static-global state. Harvest the rawcon editor into the
  console work: **M**, 1-2 weeks (SAS/C idioms to gcc, binding to the handler); separate question
  from bash compatibility.

## 8. Combined verdict: bash-level compatibility on AmigaOS 3.x (68k)

Compared on the measured/estimated numbers above; "bash features" counted against the plan's 190 V
items (`grep -c '^- \[ \] V'` = 190, from the fork-sites research, section 6; not recounted
today).

| Candidate | Runs on OS3/68k today | Syntax | Concurrent pipes, `&`, subshell | Bash V items met | Cost to reach bash level |
|---|---|---|---|---|---|
| vsh plan (`shell/`, 6,277 + 1,608 lines) | yes (vsh, 78,240 bytes) | POSIX sh subset, growing | yes: clone per stage (`sh_shell_clone`, `os_spawn`), temp-file `$( )` | plan target 190 | the plan's phases, the only one with a path; large but each item spelled out |
| abcsh (pdksh 5.2.14, 18,860 lines) | no: PPC, OS4, clib2 (port M) | ksh88/POSIX | no: serial pipes, synchronous `&`, shared globals | 0 of 190 new; has `[[ ]]`, `$(( ))`, indexed arrays (not bash-exact) | port M (1-2 weeks) then still the full bash gap; concurrency L (4-8 weeks) |
| CShell (15,669 lines) | yes, SAS/C native | csh | `&` yes (SYS_Asynch); pipes temp-file sequential; no subshell found | 0 | rewrite of the parser, L; not a path to bash |

**Winner: the vsh plan, unchanged.** Neither shell closes any part of the bash gap that the plan
does not already schedule: abcsh gives a POSIX/ksh88 interpreter without concurrency, CShell gives a
different language. "Port abcsh as sh" (M, 1-2 weeks) buys a second, weaker shell and about 20K
lines to carry; "use CShell" buys 0 bash items.

**Mix, in order of value (all optional, none blocks the plan):**

1. **abcsh `tests/*.t`** (about 18 files, pdksh's regression suite, public domain per `LEGAL`) as a
   POSIX sanity corpus beside the Phase 0 `bashdiff` harness: S, about 1-2 days to adapt; each file
   targets the same constructs the plan adds (`arith`, `heredoc`, `ifs`, `read`, `alias`).
   Need to check they can run without pdksh's `th` driver.
2. **CShell `rawcon.c` line editor** as a reference (not a copy) when Phase 7 reaches the console
   handler: M, only if the handler's editor is built in vsh rather than in vtcon; today the plan puts
   it in the handler, so this is a reading task: S.
3. **Nothing from either shell's process mechanism.** vsh's clone-per-stage already supersedes
   abcsh's serial in-process subshell and CShell's temp-file pipe and fire-and-forget `&`.

Numbers: vsh-plan path = existing 7,885 lines + 190 V items; abcsh adds 18,860 lines, CShell 15,669,
for 0 of 190. Estimated cost of detour: 1-2 weeks (abcsh port) or L (anything with CShell) before the
first bash feature, versus starting Phase 0 now.

### Unconfirmed (CShell)

- `while`, subshell, and `&&`/`||` support in CShell (only the builtin table was grepped, by quoted
  name).
- The original Dillon-era terms (`COPYING` not in the tree).
- Whether `$( )`/backquote capture uses a temp file or a pipe (marker handling read, capture code not).
- How much of CShell is SAS/C-specific for a gcc cross build.
- Minimum OS: README says 2.0+; not tested on 3.x or any rig (no rig, per instruction).
