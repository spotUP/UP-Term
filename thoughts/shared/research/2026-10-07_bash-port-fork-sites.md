---
date: 2026-10-07
topic: "Real bash port to AmigaOS: fork() call sites in bash 5.2.37 versus ixemul's vfork-only process model"
tags: [bash, ixemul, fork, vfork, port, vsh, amiga, research]
status: draft
---

# Bash port to AmigaOS: where bash needs fork(), and what that costs

Question: how hard is a real bash port given ixemul has no fork() (`library/misc.c:162`
returns -1/ENOSYS) and only vfork (`library/vfork.c`)? Compared with finishing the vsh plan
(`thoughts/shared/plans/2026-10-07-vsh-bash.md`).

Source counted: bash-5.2.37.tar.gz from ftp.gnu.org/gnu/bash/, extracted in a fresh scratchpad
directory, never built or run. Tests, doc and examples excluded.

## 1. Method and counts

Two counting methods, and they agree.

- **Method A (grep spelling):** `grep -rnE '\b(make_child|fork|vfork|posix_spawn|spawn[lv]p?)\b *\(' --include='*.c'`
  over the tree, minus tests/doc/examples/support/lib/malloc. Result: 7 `make_child` calls in
  `execute_cmd.c` (5) and `subst.c` (2), plus the two `fork()` calls inside the two definitions of
  `make_child` (`jobs.c:2164`, `nojobs.c:530`). `examples/loadables/push.c:62` is a 10th hit, excluded
  (example, not built). No `vfork`, `posix_spawn`, `system()` or `popen()` anywhere in the shell proper.
- **Method B (structure):** every child process in bash is made by `make_child()` (`jobs.c:2121` when
  JOB_CONTROL is on, `nojobs.c:493` when off; exactly one is compiled). Callers of `make_child`
  were read from the functions that contain them: `execute_command_internal`, `execute_coproc`,
  `execute_null_command`, `execute_simple_command`, `execute_disk_command`, `process_substitute`,
  `command_substitute`. Pipelines and `&` are not call sites: they are the flags
  (`pipe_in`, `pipe_out`, `asynchronous`) that make the five execute_* sites fork. Same 7 callers.

So: **7 caller sites, 1 fork() per build configuration (2 in the tree, mutually exclusive).**

Not fork sites (checked in source, method B): here-documents (`redir.c:437-500`: pipe if the text
fits the pipe capacity, else temp file; no child, in 5.2.37), `trap` handlers and `PROMPT_COMMAND`
(`eval.c:299-328`: run in-process through `parse_and_execute`), the `exec` builtin, `$(< file)`.
Those groups have 0 sites.

## 2. Sites grouped by purpose

"Continues bash" means the child keeps running bash interpreter code (variables, functions,
builtins, parser) after the fork, so it needs a real copy of the shell state. "Execs" means the
child ends in `execve()`.

| Group | Sites (file:line) | Count | Child after fork | vfork-compatible? |
|---|---|---|---|---|
| Simple command, external | `execute_cmd.c:5655` (`execute_disk_command`) | 1 | Does redirections, signal reset, `shell_execve` (`execute_cmd.c:5960`). Execs. Runs about 100 lines of bash code first (`do_redirections` 6202-6222, `restore_original_signals`, error paths), mutating globals. | Mostly. The preamble writes shared globals; needs audit (M). Without an `&`/pipe bash already skips the fork (`nofork`, `execute_cmd.c:5649`). |
| Pipelines | no own site; stages enter via `execute_cmd.c:654` (compound), `4446` (builtin/function), `5655` (external), `4110` (null) | 0 own, 4 shared | per stage kind, see rows | External stages yes; builtin, function and compound stages no |
| Simple command that is a builtin or function in a pipeline or `&` | `execute_cmd.c:4446` (`execute_simple_command`, `dofork`) | 1 | Runs `execute_builtin_or_function` in the child, then `exit`. If the word is external it falls through to the disk path with `CMD_NO_FORK`. Continues bash. | No |
| Subshell `( )`, `{ }` in a pipe or `&`, compound command in a pipe | `execute_cmd.c:654` (`execute_in_subshell`, `execute_cmd.c:1477`) | 1 | Runs a whole command tree, can modify variables and exit; the parent must not see the changes. Continues bash. | No |
| Async `&` | no own site; flag `FORK_ASYNC` on 654, 4110, 4446, 5655, 2424, subst.c:6536 | 0 own | as the underlying command | as the underlying command |
| Null command with redirection or pipe (`: > f`, `> f &`, `| >f`) | `execute_cmd.c:4110` (`execute_null_command`) | 1 | Does redirections and exits. No execve, but runs bash code. Continues (trivial). | Yes in practice, but it exits instead of exec; vfork-style resume on exit exists (`vfork.c`: parent resumes on `_exit` or `execve`) |
| Coprocess | `execute_cmd.c:2424` (`execute_coproc`) | 1 | Runs a command tree via `execute_in_subshell` (2436). Continues bash. | No |
| Command substitution `$( )` and backticks | `subst.c:7009` (`command_substitute`) | 1 | `parse_and_execute` of the string, with stdout on a pipe to the parent. Continues bash. The parent reads while the child runs, so the parent cannot be blocked. | No |
| Process substitution `<( )` `>( )` | `subst.c:6536` (`process_substitute`) | 1 | `parse_and_execute`, with a FIFO or /dev/fd path handed to the parent. Continues bash. Needs /dev/fd or named pipes; ixemul has neither (`mkfifo` returns ENOSYS, `misc.c:172`). | No |
| Here-doc writers | none (5.2.37 uses pipe or temp file) | 0 | not applicable | not applicable |
| `trap`, `PROMPT_COMMAND` | none (run in-process) | 0 | not applicable | not applicable |
| The fork() itself | `jobs.c:2164`, `nojobs.c:530` (inside `make_child`) | 1 live (2 in tree) | not a site: the single choke point | patching here replaces all 7 |

Totals: 7 callers. Of those, **1 execs after a short preamble** (5655), **1 is a trivial
bash-code-then-exit** (4110), **5 continue running bash with real fork semantics** (654, 2424,
4446, 6536, 7009). The 4446 site is shared: when its word is external it ends in exec, so it is
"continues" only for builtins and functions.

One choke point is the good news: `make_child()` is the only function to change. The bad news is
that its contract is `pid == 0` means "I am the copy"; the child then returns up through the
caller's own stack frames and uses the parent's heap.

## 3. Other POSIX needs, against ixemul-vtcon (grep of `library/` and `include/`)

| Need | bash use | ixemul-vtcon state (where I looked) |
|---|---|---|
| `fork` | 7 sites above | Stub, returns -1/ENOSYS: `library/misc.c:162`. |
| `vfork`, exec | none (but is the only route) | Real: `library/vfork.c` (1047 lines). Child is a new AmigaDOS process sharing the parent's memory; parent blocks until child calls `execve` or `_exit` (comment near `vfork.c:605`, wait at `vfork.c:790`). |
| `waitpid`, `wait4` | `jobs.c:125`, `nojobs.c:65` | `wait4` in `vfork.c:913`; `waitpid` declared in `include/sys/wait.h` and `library/ixprotos.h`. Only vfork children are waitable. |
| `SIGCHLD` | `jobs.c` reaper | Sent on child death (`vfork.c:483`) and on stop (`kern_sig.c:759`). |
| `setpgid`, `setpgrp`, `getpgrp` | `jobs.c:2249, 2313, 5057` | Present but only write `p_pgrp` in the user area (`library/misc.c:50-70`); no kernel meaning beyond `wait4(0/-pgrp)` matching. |
| `tcsetpgrp`, `tcgetpgrp` | `jobs.c:4696, 4726` | Declared (`include/unistd.h:116-117`) and numbered in `syscall.def`; no definition found by grep in `library/*.c`. `TIOCSPGRP`/`TIOCGPGRP` handled in `library/__tioctl.c:597, 620` (not read in detail: UNCONFIRMED whether the vtcon handler honours them). |
| `pipe` | `execute_cmd.c:2526`, `subst.c:6505, 6991`, `redir.c:472`, `general.c:720` | `library/pipe.c`, present. |
| `dup2` | `execute_cmd.c:6202-6222`, `redir.c` (6 sites) | Present (`library/kern_descrip.c`). |
| `sigaction`, `sigprocmask`, `sigsuspend` | about 40 sites in `sig.c`, `jobs.c`, `trap.c`, `nojobs.c` | Present (`library/kern_sig.c`). Signal delivery between AmigaDOS processes is the existing ixemul design. |
| `tcgetattr`, `tcsetattr` | `jobs.c:2489, 4607`, readline | Present (`library/__tioctl.c`). |
| `mkfifo`, `/dev/fd/N` | process substitution | `mkfifo` stub ENOSYS (`misc.c:172`); `/dev/fd` not checked: UNCONFIRMED. |

Job control would be configured out (`--disable-job-control`, `nojobs.c`), which removes the
`tcsetpgrp`/`setpgid` dependency, at the price of no `fg`/`bg`/`%1`.

## 4. How existing AmigaOS bash ports handled fork

What I found, and what I could not confirm.

- **Aminet `dev/gg/bash-src.lha`, "GNU bourne compatible shell. V2.01"**, from the Amiga
  Development Environment (GeekGadgets). Read the readme (https://aminet.net/dev/gg/bash-src.readme)
  and downloaded the archive; extracted into the scratchpad and read as text only. CONFIRMED in the
  source: it is bash 2.01 with `jobs.c:1116` and `nojobs.c:368` still calling plain `fork()`. The only
  Amiga change I found is in `configure:2067`: `#ifdef __amigaos__ child = vfork();` inside the
  getpgrp test program. I found no patch of `make_child`, `command_substitute` or `process_substitute`.
  The archive carries no ADE patch files that I saw. UNCONFIRMED: whether that binary ever ran
  subshells and command substitution on a real ixemul (ixemul 48 `fork()` returns ENOSYS here, so
  my expectation is that `$(...)`, `( )` and pipelines of builtins failed; I did not test it and did
  not find a report either way).
- **Perl on Amiga (`README.amiga`)**: "the ixemul library has only vfork"; after Perl 5.7.2
  dropped internal vfork support, the port broke and "executing miniperl in backticks seems to
  generate nothing". Source: https://perldoc.perl.org/5.8.5/perlamiga and the perl README.amiga
  copies the search returned. This confirms the failure mode for a program that needs fork
  semantics (backticks), not for bash.
- **Aminet `util/shell/bsh`**: a separate shell with command substitution and pipes
  (https://aminet.net/package/util/shell/bsh); not bash; not investigated.
- **ADE `pdksh-bin`** is what the GeekGadgets toolchain itself used as its shell (named as part of
  the minimum ADE setup in the search results). pdksh also forks for `$( )` and subshells; how it
  coped on ixemul: UNCONFIRMED, not read.
- **bebbo/amiga-gcc** (https://github.com/mheyer32/amiga-gcc): the search returned nothing about a
  bash port. UNCONFIRMED that none exists.
- **AROS**: search found no bash port or ixemul/vfork patches (AROS uses its own `posixc.library`;
  `aros.aminet.net/package/dev/gg/bash-src` only mirrors the same V2.01 archive). UNCONFIRMED beyond that.
- **Searches done** (WebSearch, 6 queries, plus Aminet name search and two WebFetches). The
  context7 and tavily search servers were down. No source describing a working modern bash on
  AmigaOS with subshell support was found. Absence of evidence only; a search of Amiga forums
  (EAB, amigaworld, Aminet comments) was not possible with these tools.

## 5. Per group: make it vfork-compatible, or an in-process alternative

Effort: S under 1 day, M 2-5 days, L 1-3 weeks of one developer, working with the rig; all with
the build in docker (not yet proven for bash 5.2 on this toolchain: UNCONFIRMED, see section 7).

| Group | Approach | Effort | Why |
|---|---|---|---|
| External simple command (5655) | vfork in `make_child`, audit the child preamble (`do_redirections`, signal restore, `adjust_shell_level`) for global writes; parent stays blocked until `execve` | M | Standard vfork discipline; every global write before exec is shared with the parent. Bash has about 100 lines there plus `shell_execve`'s script-interpreter fallback (`#!`). Also needs `shell_execve` to find AmigaDOS paths and scripts. |
| Null command (4110) | child work is redirections only: do them in the parent on a saved fd, or vfork-and-`_exit` | S | Few lines, no state to copy. |
| Builtin or function in pipe/`&` (4446) | needs a subshell | L (shared with subshell row) | See next row. |
| Subshell, compound in pipe, `&` (654), coproc (2424) | In-process alternative: run the command tree on a new AmigaDOS process with a deep copy of the variable table, function table, option flags, traps, fd table, then `exit` that process. This is what vsh already does (`vsh.c:1024-1084` `subshell_proc`, `sh_shell_clone` at `sh_exec.c:169`). In bash the "shell state" is about 114 000 lines of C with several hundred globals (variable hash tables, `array.c`, `assoc.c`, alias, hash, dirstack, `jobs` list, trap table, parser state, readline state, `current_command_line`, `execute_cmd.c` statics). A correct copy of all of it is the real fork. Alternative: implement a fork in ixemul by copying the data, bss and heap into a new process at a different address: impossible without an MMU unless bash is linked position-independent (m68k gcc: no), because pointers into the heap would point at the parent's copy. | L, with high risk | There is no per-process address space on a plain Amiga. A heap copy cannot be relocated, so "fork by copying" yields a child whose pointers still refer to the parent's memory: the two must not run concurrently, which a pipeline needs. Serialised subshells (parent blocked, child shares memory, child's variable changes undone on exit by a save/restore of the variable table) cover `( )` and `$( )` that are synchronous, and fail for pipelines of two bash-code stages, coprocesses and `&` of non-exec commands. |
| Command substitution (7009) | Synchronous variant: run `parse_and_execute` in the parent with stdout redirected to a temp file or pipe buffer, then read it back; restore variable table (subshell semantics) by snapshot | M | The parent is the only runner, so no concurrency problem. What does leak: `cd`, `set`, variable assignments, `exit` inside the substitution; each needs snapshot/restore code in a function that was never designed for it. Plausible, and it is what vsh does without a process (`sh_exec.c:2454-2481`: temp file path). |
| Process substitution (6536) | Temp file first (the vsh plan's own decision, V59), or `PIPE:` | M | No FIFO or `/dev/fd`; replace the whole mechanism. |
| Pipelines of bash-code stages | Needs real concurrency between stages: cloned-state processes as in vsh | L | Cannot be done with shared-memory vfork children running bash code. |
| `make_child` / `wait` / job table | Reimplement over `vfork`/`wait4`, build `--disable-job-control` | M | `jobs.c` is 5156 lines; the `nojobs.c` path is the smaller one. |
| Here-doc, traps, PROMPT_COMMAND | none needed | none | in-process already |

The rest of a bash port, independent of fork: configure/cross-build for m68k-amigaos with gcc
(bash 5.2 `configure` runs test programs that cannot run when cross-compiling; needs
cache-file answers, about 60 checks), AmigaDOS path semantics (`/`, `:`, volume names, case
insensitivity, `PATH` with `;`-less entries, `#!` scripts do not exist natively), readline against
the vtcon console handler (the vsh plan already treats the console handler as a phase of its own),
and locale/`getpwnam`/`getgroups` stubs. I did not size this block; a rough figure is M-L (UNCONFIRMED,
estimate only).

## 6. Verdict

**Finish the vsh plan. Do not port bash.**

Reasons, with numbers:

1. **Bash's core assumption is fork.** 5 of 7 call sites need a real copy of the shell: subshell,
   coproc, builtin/function stage, `$( )`, `<( )`. Those are not corner cases: `$( )` and
   pipelines are the bulk of real scripts (agent use and `configure`). Of the sites, only 1 execs
   cleanly. Without a copying fork, a port runs only the scripts that avoid all five.
2. **A copying fork on a machine with no MMU is not available.** The in-process alternatives
   (state snapshot/restore, serialised subshells) break pipelines of two bash-code stages and `&`
   of non-exec commands, which are exactly what the five hard sites exist for. Making bash
   re-entrant over a cloned state means touching hundreds of globals across about 114 000 lines
   (source count: `*.c`, `builtins/*`, `lib/sh/*`), and merging every upstream bash fix after that.
3. **vsh already solved the same problem its own way.** It has clone-the-shell subshell processes
   (`sh_shell_clone`, `os_spawn`) and a temp-file `$( )`, designed for the Amiga from the start.
   Its parser, expander and executor total 6277 lines, small enough that cloning the state is
   one struct. The plan lists 190 V items (0 ticked: `grep -c '^- \[ \] V'` = 190, `[x]` = 0) to
   reach bash behaviour on top of that; large, but each item is a spelled-out feature, not an
   unbounded porting risk.
4. **Prior art points the same way (partly UNCONFIRMED).** The one Amiga bash I could inspect
   (ADE 2.01) left `fork()` untouched except in `configure`; Perl's Amiga port broke on the same
   fork/vfork gap. No source found shows a bash with working subshells on ixemul.

Rough sizes (estimates, not measurements): a bash port that handles external commands, `$( )`
(serialised) and `( )` (serialised), no real pipelines of builtins, no coproc, no process
substitution: M-L, about 3-5 weeks including cross-build and console work, and it still fails on
`cmd | while read ...` (the case the vsh plan itself calls out, `lastpipe`, V79). A bash port
with real fork semantics: not achievable on this platform without an MMU-less relocating
process-copy facility that does not exist (a research project in ixemul, L+ and unproven).

What would change the verdict: (a) an MMU-hosted target (Amiga with 68040/060 and a pager,
AmigaOS 4, MorphOS, AROS hosted) where a real fork could be built, but that is a different
project from ixemul-vtcon; (b) a requirement for byte-identical bash behaviour on scripts the
plan has not covered, which the `bashdiff` harness in the plan is meant to measure.

## 7. Open questions and unconfirmed items

- Does ADE bash 2.01 run `$( )` on a real ixemul 48 (fork ENOSYS)? Not tested (the FS-UAE rig
  was not used, by instruction). Where I looked: bash-src.lha source, Aminet readme.
- Does the vtcon handler honour `TIOCSPGRP`/`TIOCGPGRP` (`__tioctl.c:597, 620` read only by name)?
  Matters only with job control on.
- `/dev/fd/N` and named-pipe support under ixemul-vtcon (`mkfifo` stub confirmed; `/dev/fd` not checked).
- Whether bash 5.2 cross-builds with the project's gcc 2.95/16 toolchain (`build295`, `buildgcc16`) is
  untested; the port estimates assume it does.
- The size of non-fork porting work (AmigaDOS paths, readline on the console handler) is a guess.
- Amiga forum archives (EAB, amigaworld) and the GeekGadgets CVS were not searched; there may
  be a later ADE bash (2.05a or newer) that handled fork differently. The name "bash 2.05a" in the
  question did not appear in any result.

## 8. MMU-based fork feasibility

Looked at: web search (mmu.library, AmigaOS 4, FPGA cores; 3 queries), and the ixemul tree for
task hooks. No MMU library source or manual was read in full; the manual PDF
(mmudoc, discmaster.textfiles.com AACD 10) was only returned as a search hit.

- **mmu.library (Thomas Richter, MMULib):** a documented interface to the MC68K MMU; needs a
  68020+68851, 68030, 68040 or 68060 with a working MMU. It exists because earlier tools
  (Enforcer, CyberGuard, SetCPU, VMM, GigaMem) each programmed the MMU themselves and conflicted;
  AmigaOS 3.x has no MMU support of its own. Sources: https://se.aminet.net/util/libs/MMULib.readme
  and the search summary of the MMULib manual. It offers page properties (write-protect, cache
  mode, remap of ranges) and tools such as MuLink. UNCONFIRMED from the manual itself: multiple
  user-created MMU contexts, a page-fault hook that could implement copy-on-write, and a
  per-task context switch. Those three decide everything and must be read in `mmudoc` first.
- **68040.library / 68060.library:** they set up the MMU for caching and ROM-copy tables;
  they are not an API for per-process spaces. UNCONFIRMED in detail (not read).
- **AmigaOS 3.x per-task hook:** exec's `Task` structure has `tc_Switch` and `tc_Launch`
  (called at task switch-out and switch-in). This is exec's documented design from memory, not
  re-read in this session: UNCONFIRMED here. So a switch hook to swap an MMU root pointer
  could exist, but all of AmigaOS (libraries, message ports, AllocMem'd structures, the
  Chip RAM) assumes ONE shared flat address space. A forked process with its own copy of the
  data at the same virtual addresses would still send and receive messages whose pointers
  refer to shared system memory, so such pages must be mapped identically in every space.
  That is a partial-sharing design with no precedent I found.
- **Prior art:** AmigaOS 4 has virtual memory but only limited protection of kernel space and
  "nothing in user-space" (https://ftp.fau.de/mirrors/aminet/docs/anno/amigaos40features.pdf, and the
  Ars Technica review summary via Slashdot); no per-process address space or fork found.
  AROS: nothing found (UNCONFIRMED). Copy-on-write or remapped fork on any Amiga: none found.
  The search returned only generic notes that fork without COW means copying all parent memory
  (https://git.psf.lt/emo/busybox, vm slides) which, with a flat space, is also what the
  non-MMU option would do.
- **Hardware:** 68020 has no MMU (needs an external 68851). 68030 has a simple MMU (the
  68030's, not the 040/060 page-table MMU); 68040 and full 68060 have it; the 68LC040 and 68EC060
  do not. Replay: dedicated 68060 core; MMU only with a real 68060 that has one, and the FPGA
  68000/020 cores have none; whether any Replay or MiSTer core implements an MMU is
  UNCONFIRMED (MiSTer Minimig is a 68020-class TG68; a 68030/040 core was only discussed).
  Apollo 68080: closed source, MMU said to be a future item (UNCONFIRMED). The FS-UAE rig
  emulates an MMU only in 68030/040 configurations: UNCONFIRMED here.

**What an MMU fork would take:** (1) mmu.library contexts with identical mapping of all system
memory plus private copies of the process's data, bss and heap, (2) a copy-on-write fault
hook, (3) `tc_Switch`/`tc_Launch` hooks to switch the context, (4) ixemul changes: a real `fork()`
that allocates a new Process, re-maps its data/bss/heap and stack, and makes every library
base, `SysBase`-derived pointer and open file handle valid in both. Plus bash needs the child
heap at the same addresses, so malloc's arena must live in the remapped range.

**Effort: L, with a high chance of failure (research project).** Hard parts: exec's flat-space
assumption, the AmigaOS shared structures reached from the private heap, fault-handling in
supervisor mode inside exec, and bugs that would corrupt the whole system rather than one
process. It would also exclude every 68000/68020 machine and every FPGA core without a working
MMU, which is most of the target base of this project. In cases where only an MMU-less path
exists, the MMU fork is not usable, so any bash port would still need the non-MMU design.

**Verdict: neither first.** Not MMU first (L, high risk, small hardware base, required
library behaviour unconfirmed, and it still ends with a fork that works on some machines
only). Not a bash port on the non-MMU path either (section 6). The usable step is the vsh plan,
which runs on every machine. If a real fork is ever wanted, the first S-size task is reading the
mmudoc manual for the three UNCONFIRMED capabilities above (contexts, fault hook, per-task switch);
if any is missing, the idea is dead.

## Sources

- bash 5.2.37 source: https://ftp.gnu.org/gnu/bash/bash-5.2.37.tar.gz (counted)
- Aminet bash-src (V2.01, ADE): https://aminet.net/dev/gg/bash-src.lha and https://aminet.net/dev/gg/bash-src.readme (read)
- AROS mirror of the same package: https://aros.aminet.net/package/dev/gg/bash-src (listing only)
- perlamiga (ixemul has only vfork; backticks fail): https://perldoc.perl.org/5.8.5/perlamiga
- bsh on Aminet: https://aminet.net/package/util/shell/bsh (listing only)
- amiga-gcc: https://github.com/mheyer32/amiga-gcc (no bash information found)
- Local: `library/misc.c`, `library/vfork.c`, `library/__tioctl.c`, `include/unistd.h`,
  `thoughts/shared/plans/2026-10-07-vsh-bash.md`, `shell/vsh.c`, `shell/sh_exec.c` (vtcon)
