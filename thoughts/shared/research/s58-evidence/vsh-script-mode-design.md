---
date: 2026-11-05
row: VSH-AMIGA-SCRIPT-MODE (thoughts/spot/todos.md)
status: design, no code
---

# vsh AmigaDOS script mode: design

Goal: `.bat`-style AmigaDOS scripts (If/Else/EndIf, Skip/Lab/EndSkip, FailAt, Quit, `.key` templates,
Execute) run inside vsh, in the same process, so `cd`, variables, Path and vsh functions behave as in
an AmigaDOS Shell running Execute. sh scripts and the interactive prompt stay bash.

No AmigaDOS manual exists in the repo (the builtins survey found none either). Everything marked
UNVERIFIED is from memory of the 3.1/3.2 manuals and is listed as a rig check against the real
C:Execute in section 7.

## 1. Trigger: how vsh knows a file is an AmigaDOS script

Decision: per FILE, never per line. A file is AmigaDOS when it is entered through one of two doors:

1. **`execute FILE [args]`** (any case): a new builtin `execute` (builtins table + `amiga_internals`,
   as `path` in 7bfa8df9). A function or alias named `execute` wins (bash order, as `run` in 7bfa8df9).
   `C:Execute file` / `c:execute file` (path-qualified) is not the builtin word and still reaches the
   real C:Execute through `os.run` (today's SystemTags path in vsh.c os_start): the escape hatch.
2. **A script file run by name** that is not a load file and either has the S bit and no `#!`/`;!`
   line, or starts with `.key`/`.k` and has no `#!` on line 2. Today `sh_script_argv`
   (sh_exec.c:8183) returns 0 for exactly these (`!plain || keyed`) and vsh.c os_start hands them to
   SystemTags, i.e. a separate real Shell. New: in that branch, when `wait` is a foreground wait, os_start
   releases seg/home, touches no io, and returns a new sentinel `SH_RUN_AMIGA_SCRIPT` (-1001L, next to
   `SH_STOPPED` in sh_exec.h). `start_program` (sh_exec.c:8273) sees it, resolves the path again with
   `find_command_file`, and calls the same entry the builtin uses. Background (`&`, `run`) and pipeline
   stages keep today's real-Shell path (no in-process semantics are possible there anyway: a subshell
   would lose `cd` too). This keeps the cost of ordinary commands at zero: the decision is made only
   for a file that already failed LoadSeg.

Not a trigger: the interactive prompt, `source`/`.`, `sh file`, `vsh file`, `$0` scripts
(`sh_run_script`), `#!/bin/sh` files, plain no-S-bit files (POSIX ENOEXEC rule stays). Per-line keyword
recognition was rejected: `if`/`else` are sh reserved words, and an `If EXISTS x` line in an sh file
would have to be guessed, which breaks the "bash wins on a real clash" rule.

Clash table (RULES "Unix and Amiga forms": bash wins only on a real clash):

| Word | sh meaning | Amiga meaning | Outcome |
|---|---|---|---|
| `if`, `else` | reserved words | If/Else commands | sh files and the prompt: bash always. Inside an Amiga script: Amiga. No overlap: the language is chosen per file. |
| `endif`, `skip`, `lab`, `endskip`, `failat` | none | script commands | Amiga script only. At the prompt they stay command-not-found (or C: files if present), as `If` typed in an AmigaDOS Shell outside a script is an error (UNVERIFIED R1). |
| `quit` | none in vsh | end script with rc | Amiga script only. At the prompt: falls to C:Quit if present (unchanged). |
| `set`, `unset` | bash builtins | local Shell variables | Amiga script only: Amiga meaning (`Set name value` -> shell variable, `Unset name`). No clash: bash is not the language of that file. OWNER DECISION D1. |
| `execute` | none | run a script | Builtin everywhere (door 1). |
| `.key` etc. | `.key` is just an unknown word | directive | Directive only in Amiga mode. |

## 2. Where it lives

Decision: a **separate line-oriented interpreter** in a new file `shell/sh_amiscript.c` (+ `shell/sh_amiscript.h`),
over the existing simple-command executor. Not an extension of sh_parse.c: AmigaDOS lexing is
incompatible with sh lexing (`;` starts a comment, `#?` is a pattern not a comment, `*"` / `*N` / `*E`
escapes inside quotes, `<key>` substitution, no `|`/`&&`), and control flow is textual (If/Skip move
the read position) so it cannot be a parse tree.

Each executable line becomes one stack-built `SH_CMD` node, the same trick `amiga_run` uses
(sh_exec.c, `sh_node c = *n; c.words = ...`):
- words: each Amiga token single-quoted (`'` written as `'\''`) so sh_expand leaves it literal
  (Amiga commands do their own `#?` matching, so no globbing is correct);
- redirs: `>f`, `>>f`, `<f` become `sh_redir` SH_R_OUT / SH_R_APPEND / SH_R_IN with a quoted target;
- run through the public `sh_exec(sh, node, io)` (sh_exec.h:486), so builtins (native Echo/Path/SetEnv/
  Why/Fault/Ask), `run` (amiga_run intercept in exec_node1), functions, the hash and `os.run` all apply
  unchanged; `$?` and Why's secondary result update as for any command.
- Alias: vsh expands aliases on argv at exec time (sh_exec.c:7355 region) so literal nodes should keep
  AmigaDOS aliases; step S2 confirms it with a test.

Reused, not rewritten:
- file reading: the read loop of `b_source` (sh_exec.c:7260) is extracted into a static helper
  `read_file_text(sh, path, io, &len)` that both `source` and `execute` call;
- frames: `frame_push(sh, "execute", file)` / `frame_pop` as b_source does, so `caller`/errors name it;
- keywords: `echo_kw` (sh_exec.c:1546) for case-insensitive words (needs a non-static shim or the
  interpreter's own 4-line copy is a defect: expose it via sh_amiscript.h as `sh_kw_eq`);
- variables: `sh_get`/`sh_set` for locals, `sh->os.getenv` for ENV: (as GetEnv/$name do);
- exists: `sh->os.stat`;
- printing: `say`/`err2` style through `sh->os.write` on `io->err`.
Entry points: `long sh_amiga_execute(sh_shell *sh, const char *file, int argc, char **argv, const sh_io *io)`
called by `b_execute` (sh_exec.c) and by `start_program` on `SH_RUN_AMIGA_SCRIPT`.
Build: add `shell/sh_amiscript.c` to `SHELL_CORE` (Makefile:36) and `VSH_SRC` (Makefile:898) and the
header to the vsh_host deps (Makefile:358/363).

## 3. Semantics

Interpreter state per Execute call (struct on the C stack, nested Execute gets its own):
`lines[]` (substituted text split on `\n`), `pc`, `failat` (default 10), `bra '<'`, `ket '>'`,
`dot '.'`, `dollar '$'`, template + bound args. The last rc is `sh->ctx.status` (shared, so a
condition after a vsh builtin sees its status).

Loading (Execute does it up front into a T: file, so all substitution happens once, before line 1 runs):
1. Read the file. Leading directive lines (first char = `dot`): `.key`/`.k TEMPLATE`, `.bra c`,
   `.ket c`, `.dot c`, `.dollar`/`.dol c`, `.default`/`.def name value`; `.` followed by space or end of
   line is a comment. Directives may appear anywhere in the file (UNVERIFIED R2), they are applied
   before substitution and dropped from `lines`.
2. With a `.key` line: bind argv against the template (mini ReadArgs: items `NAME[=ALIAS]` with
   `/A /K /S /N /M /F`; `NAME value` and `NAME=value`; positional fill of non-/K items; /M takes the
   rest). Missing /A or a wrong keyword: print `Parameters unsuitable for key` style error and return
   rc 20 without running (exact text and rc UNVERIFIED R3). /S set substitutes the keyword name
   (UNVERIFIED R4). Then replace every `<name>`, `<name$default>` (with `.dollar` char), and `<$$>`
   (the CLI number; vsh: `$$`). An unknown name substitutes empty. Without a `.key` line no `<...>`
   substitution happens, so `<` / `>` stay redirections (UNVERIFIED R5).
3. Optional: a leading `?` arg prints the template (UNVERIFIED R6, low priority).

Per line (lexer):
- `;` outside quotes ends the line (comment). Blank lines skip.
- Tokens split on space/tab; `"..."` quoted with `*"` -> `"`, `**` -> `*`, `*N` -> newline,
  `*E` -> ESC; outside quotes `*` is literal.
- `$name` / `${name}` expand to the local var, else ENV: (`os.getenv`); `*$` is a literal `$`.
  Inside double quotes too (UNVERIFIED R7). Backticks: not in v1 (OWNER DECISION D2).
- `>f`, `>>f`, `<f` tokens are redirections (any position; UNVERIFIED R8 whether 3.x only accepts
  them right after the command name).
- First token decides: a script word (below) is handled by the interpreter; anything else goes to
  `sh_exec` as a literal SH_CMD. After an external/builtin command: if `status >= failat`, print
  `<cmd> failed returncode <n>` to stderr (text UNVERIFIED R9) and end the script with that rc.

Script words (case-insensitive):
- **If** `[NOT] [WARN|ERROR|FAIL] [a EQ|GT|GE b] [VAL] [EXISTS name]`: WARN true when last rc >= 5,
  ERROR >= 10, FAIL >= 20. EQ/GT/GE compare strings case-insensitively (UNVERIFIED R10), numerically
  with VAL (strtol, base 10). EXISTS uses `os.stat` (file or dir). Several conditions: AND (UNVERIFIED
  R11). NOT inverts the result. True: next line. False: scan forward from pc+1 counting depth: `If`
  +1, `EndIf` -1 at depth 0 ends the scan after it, `Else` at depth 0 ends the scan after it. End of file
  while scanning: the script ends (rc unchanged, UNVERIFIED R12).
- **Else** reached while running (the true branch ended): scan forward to the matching `EndIf`
  (same depth counting). **EndIf**: no-op. No If stack is kept: this is how the C:If/Else pair works
  (stateless textual skipping), which is also why Skip can jump in and out of If blocks freely.
  Scans look only at the first token of each line (after `;` stripping), so `echo If` is not counted.
- **Skip** `[label] [BACK]`: forward from pc+1 (BACK: from line 0) to the first `Lab label`
  (case-insensitive; no label: the first `Lab` of any name, UNVERIFIED R13) or `EndSkip`; execution
  resumes on the line after it. Not found: error `Label <x> not found` (UNVERIFIED text/rc R14, assume
  rc 10) and the script ends. Skip leaves rc unchanged; **EndSkip** reached by a skip sets rc 5
  (UNVERIFIED R15); reached in normal flow, no-op. **Lab**: no-op.
- **FailAt** `[n]`: set `failat`; no arg prints the current limit (text UNVERIFIED R16). Scope: this
  Execute call; nested Execute starts at 10, the caller's value is restored on return (UNVERIFIED R17).
  The interactive prompt never aborts.
- **Quit** `[rc]`: end this script with rc (default 0). Inside a nested Execute only the inner script ends
  (UNVERIFIED R18). A bad number: rc 0 and a warning? (UNVERIFIED R19; v1: treat as 0).
- **Execute** inside a script: recursive `sh_amiga_execute`, same process.
- **Set name value** / **Unset name** (D1): local shell variable via `sh_set` / unset; **Get name**:
  print it. Echo/Ask/SetEnv/GetEnv/Path/Run/Why/Fault already exist as native forms.
- `sh->intr` (Ctrl-C) ends the script with rc 20 and `***Break` (UNVERIFIED R20).

Return value of the script = the last command's rc, or Quit's, or the failing rc. `$?` gets it.

## 4. Test plan

Host: `tests/test_sh_exec.c`, one test function per construct, registered in `suite_sh_exec`; scripts
written into the fake fs with `printf '...\n' > RAM:x` (f_open's in-memory files); the fake `fail N`
command and `args` (prints `<arg>` per line) are reused. Run: `make test ONLY=sh_exec`.

| Test | Asserts |
|---|---|
| `execute_runs_lines_literally` | `;` comment dropped; `#?.info` reaches `args` as `<#?.info>`; `"a*"b"` -> `<a"b>`; `>RAM:o` redirection; `$x` expands, `*$x` does not; a vsh function is callable; `c:execute` is not the builtin (goes to os.run) |
| `execute_if_else_endif` | EQ true/false, NOT, VAL GT/GE, nested If inside a false branch skipped as a unit, Else skips to EndIf, `echo If` not counted |
| `execute_if_condition_flags` | after `fail 5`/`fail 10`/`fail 20` (with `FailAt 21`): WARN/ERROR/FAIL truth table; EXISTS on an existing and a missing RAM: file |
| `execute_skip_and_lab` | forward skip over lines; `Skip BACK` loop runs 3 times using a sh function counter (`inc() { n=$((n+1)); }` then `If $n EQ 3 VAL`); missing label error + script ends; EndSkip rc 5 |
| `execute_failat_aborts` | `fail 10` at default limit ends the script, message on stderr, `$?` = 10, next line not run; `FailAt 11` keeps going; nested Execute restores the limit |
| `execute_quit_sets_rc` | `Quit 7` -> `$?` 7, lines after not run; nested Quit ends only the inner script |
| `execute_key_templates` | `/A` missing -> error rc 20, nothing ran; keyword and positional binding; `<x$def>`; `.bra {` `.ket }`; `.dot`; `/S`; no `.key` -> `<` is redirection |
| `amiga_script_by_name_runs_in_shell` | fake os.run returns `SH_RUN_AMIGA_SCRIPT` for an S-bit fake file: the script ran in-process (SH_HIT(AMIGA_SCRIPT) counter, new entry in sh_hits.h) and a `cd` inside it persists; `cmd &` on the same file still calls os.run with SH_SPAWN_BG |
| `sh_files_keep_bash_if` | `source` of a file with `if true; then echo y; fi` prints y; `If EXISTS x` typed as sh is a parse wait/error, not Amiga (bash wins) |

Rig reachability (one test, the feature's single wiring proof): extend `tools/rig/vshexec_rig.py` with
case 7. Write `RAM:vx/loop.dos`: `.key N/A`, `FailAt 21`, a `Skip BACK` loop driven by `C:Eval >ENV:n`,
`If EXISTS RAM:vx`, `If WARN` after a command returning 5, `Quit 3`, plus `CD RAM:vx` as the sentinel.
Run `execute RAM:vx/loop.dos 3` in vsh: output equals the same script run under `C:Execute` in a plain
Shell (golden captured once on the rig, stored with the test), `$?` = 3, and `pwd` afterwards prints
`RAM:vx` (only an in-process run can move vsh's directory). Then run it by name after `protect +s`,
same checks. Grep `thoughts/shared/research/test-register.md` first and append a row after the run.

## 5. Work split (sonnet-sized, easiest first)

- **S1** Lexer + line runner skeleton: `shell/sh_amiscript.{c,h}`, Makefile entries, `read_file_text`
  extracted from b_source, `b_execute` builtin + `amiga_internals`, literal SH_CMD per line, comments,
  quotes/escapes, redirections, `$var`. Test `execute_runs_lines_literally`.
- **S2** Quit, FailAt (limit, abort message, per-call scope, nested Execute), Set/Unset/Get (after D1),
  alias check. Tests `execute_quit_sets_rc`, `execute_failat_aborts`.
- **S3** If/Else/EndIf with all conditions and textual depth scan. Tests `execute_if_else_endif`,
  `execute_if_condition_flags`.
- **S4** Skip/Lab/EndSkip incl. BACK and errors. Test `execute_skip_and_lab`.
- **S5** `.key`/`.k` mini ReadArgs, `.bra/.ket/.dot/.dollar/.default`, `<key$def>`, `<$$>`.
  Test `execute_key_templates`.
- **S6** Run by name: `SH_RUN_AMIGA_SCRIPT` sentinel in sh_exec.h, vsh.c os_start branch (foreground
  only), start_program dispatch, SH_HIT. Tests `amiga_script_by_name_runs_in_shell`,
  `sh_files_keep_bash_if`.
- **S7** Rig: R-checks below first (one shared rig run, results into this file), adjust semantics where
  the real C:Execute differs, then vshexec_rig.py case 7 + RULES.md `## Commands` row text.

## 6. Owner decisions

- **D1** In an Amiga script, `Set`/`Unset` mean AmigaDOS local variables (recommended: the file's
  language is Amiga, so there is no clash) vs. keep bash `set`/`unset` there too.
- **D2** Backtick command substitution (`` `cmd` ``) in v1 (recommended: no, later row; rare in .bat
  scripts) vs. in v1.
- **D3** S-bit script run by name: native in-process (recommended; same `cd`/variable behaviour as
  Execute) vs. leave it on the real Shell and make only `execute` native.

## 7. Rig checks against the real C:Execute (all UNVERIFIED items)

One shared rig run, AmigaOS 3.2 rig (`--os 32`) and 3.1, a scratch script per item, record output + rc:
R1 `If` typed at a Shell prompt outside a script; R2 directives after line 1; R3 missing /A error text
and rc; R4 what `<sw>` substitutes for a set /S; R5 `<`/`>` in a script without `.key`; R6 `execute
file ?`; R7 `$var` inside double quotes; R8 redirection after arguments; R9 FailAt abort message text;
R10 EQ case sensitivity; R11 several If conditions together; R12 If without EndIf at EOF; R13 Skip
without a label; R14 missing label message and rc; R15 rc after EndSkip; R16 FailAt without args output;
R17 FailAt scope across nested Execute; R18 Quit inside nested Execute; R19 Quit with a non-number;
R20 Ctrl-C during a script; plus: does If/Else/EndIf/Lab change rc; is a `cd` in an executed script
kept by the calling Shell (expected yes); does the 3.2 Shell treat `|` as a pipe in scripts (v1 keeps
it literal).

## Owner decisions (2026-11-05)

- D1: Set/Unset inside an AmigaDOS script = AmigaDOS local variables. Bash meaning everywhere else.
- D2: backtick command substitution IS in the first version (owner overrode the recommendation). Add it as step S5b after S5 (.key templates), with its own host test and an R-check against the real Shell.
- D3: an S-bit script run by name runs inside vsh (sentinel path, step S6). Background jobs and pipe stages stay on the real Shell.
