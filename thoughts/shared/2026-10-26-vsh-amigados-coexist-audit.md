---
date: 2026-10-26
topic: Which AmigaDOS Shell conventions vsh lacks, and which can coexist with bash syntax
tags: [research, vsh, amigados, bash, compatibility]
status: draft
---

# vsh vs AmigaDOS Shell: coexistence audit

Static, read-only (grep + ranges of shell/sh_parse.c, sh_exec.c, vsh.c; no rig run). Rule (owner 2026-10-20 and 2026-10-26): Unix and Amiga forms coexist as far as possible; bash wins on a real clash. "untested" = no code path found or no rig run, so the row is a reading, not a measurement. Auto-cd landed on main 2026-10-26: shell/sh_exec.c try_autocd (commit d7b76e11) and the resolve change in shell/vsh.c (commit 21202804).

Verdicts: W = works, C = can coexist (no bash clash, not done), X = clash (bash wins), Q = queued or landed.

| # | Convention | AmigaDOS meaning | vsh today | bash meaning of the same characters | Verdict |
|---|---|---|---|---|---|
| 1 | Implicit CD on bare directory | `ram:` or `sys:c` alone cds | try_autocd (sh_exec.c, d7b76e11) plus resolve skipping drawers (vsh.c, 21202804) | `autocd` shopt, off by default (sh_exec.c:4139 stub) | Q (landed, main 21202804) |
| 2 | `KEY=value` keyword args to native commands | `aget TO=T:x URL="..."` | landed f19eb2ab on main | `VAR=x cmd` env prefix; only before the command name, so no clash after it | Q (landed) |
| 3 | `KEY value` keyword args | space-separated keyword | plain argv pass-through, native command parses its template | ordinary words | W |
| 4 | `?` template help | `list ?` prints template, waits for input | passed as a glob word; bash glob wins, Amiga form lives in the program (decided) | `?` single-char glob | X |
| 5 | `*` escape, `*N` `*E` `*"` in strings | `echo "a*Nb"` prints newline | no escape handling in the parser; only vsh.c:965 skips `*x` while scanning a quoted command name; b_echo has none | `*` glob outside quotes, literal inside; `"*N"` prints `*N` | X (quoted `*N` is literal in bash; a bash script printing it would break) |
| 6 | `;` starts a comment | `echo hi ; note` | `;` is a command separator (sh_parse.c:353,864); `#` is the comment (sh_parse.c:336) | `;` separator | X |
| 7 | `.key` `.bra` `.ket` `.def` `.dot` directives | script template, `<arg>` substitution | `.key` is only detected to decide a file is an AmigaDOS script (sh_exec.c:7736-7776), run through Execute (vsh.c:901); no `.bra/.ket/.def` grep hit in shell/ | `.` is source, `.key` is a filename: no clash | W for detection and Execute path; `<arg>` substitution still untested (not in this run's scope) |
| 8 | `Alias name value` (space form) | defines alias, `Alias` alone lists | only bash `alias name=value` (b_alias sh_exec.c:2600); `Alias x y` form untested | `alias x y` is legal bash: defines x and y as names looked up (prints not found) | Measured on rig 7, 2026-10-26: `Alias zz echo-zz` and `Alias qq "echo qq"` exit rc 0, define nothing (`alias zz` prints nothing, `qq` -> `vsh: qq: not found` rc 127); bare `Alias` rc 0, prints nothing. C confirmed |
| 9 | `Set` `Get` `SetEnv` `GetEnv` | local vs ENV: variables | `set` is the bash builtin (sh_exec.c:4700); SetEnv/GetEnv are C: programs (resolve via PATH, untested); ENV: imported into vars at start (vsh.c:2054 import_var, 2062 import_locals) | `set` lists vars / sets positionals | Measured on rig 7, 2026-10-26: `SetEnv covt hello` then `GetEnv covt` -> `hello` rc 0 (both INTERNAL AmigaShell commands, `Which` says INTERNAL, so not C: programs and no PATH lookup; vsh hands them to the system). `Set loc abc; Get loc` -> `object not found` rc 5 and `$loc` empty (the child's local var dies with it). `Set name value` C confirmed; SetEnv/GetEnv W |
| 10 | `$var` expansion of local vs ENV | `$name` local first, then global | import_var/import_locals copy both into the shell at start, sync_env (vsh.c:2183) for window vars; later `SetEnv` from a child is not seen (untested) | same `$name` | Measured on rig 7, 2026-10-26: script file run (`vsh RAM:t.sh`): an ENV: var set BEFORE vsh starts is in the environment (`env | grep pre1` -> `pre1=fromshell`, `printenv pre1` -> fromshell) but `echo "$pre1"` and `$Kickstart` expand EMPTY; a plain `pre1=local` then works. A `SetEnv` inside the run is seen by `GetEnv` and `List ENV:` but never by `$name`. Not as audited (W at start): ENV: vars reach the environment, not `$name`. In `vsh -c` via ami.py exec the outer AmigaShell expands `$name` first, so that path proves nothing. Verdict C |
| 11 | `<` `>` `>>` to Amiga paths, `>NIL:` `>PRT:` `>CON:` | redirection | operand passes through; `/dev/null` and `/dev/tty` map to NIL: and `*` (sh_exec.c:16-25); `>NIL:` passes as given (vsh.c:231, 1583 check for "NIL:") | same operators | Measured on rig 7, 2026-10-26: `>NIL:` rc 0 silent (W); `>PRT:` in a `-c` run: `echo x >PRT:` hung until the 30 s limit (slot freed), in a script `echo y > PRT:` -> `vsh: PRT:: cannot create` rc 1 (no printer on the rig, but the form fails, not silently); `>CON:0/0/200/50/t/CLOSE` rc 0. NIL: W, CON: W (rc only, window not looked at), PRT: fails on a rig without a printer |
| 12 | `Run` and `&` | Run starts background | `&` is a job (vsh.c:1786ff); `run cmd` recognised (vsh.c:995) | `&` | W |
| 13 | `Execute script` | run script in a new Shell | handled in-process (vsh.c:901-904) | no such command | W |
| 14 | `Resident` | list or add resident commands | vsh honours resident ixemul commands (vsh.c:735-740); the command itself is C:Resident | none | W (external) |
| 15 | `Stack` | show or set stack | b_stack (sh_exec.c:3200) | none | W |
| 16 | `Why` | explains the last failure | no builtin; C: has none; untested | none | Measured on rig 7, 2026-10-26: `Why` runs (INTERNAL AmigaShell command) but answers about the child's own last command: `The last command did not set a return code` rc 5, also after `list nosuchfile` (rc 20) and `ls nosuchfile` in vsh. Exists, wrong scope: it cannot explain vsh's last failure. C (builtin over vsh's $?) |
| 17 | `Fault n` | text for a DOS error | no builtin found; C:Fault if present (untested) | none | Measured on rig 7, 2026-10-26: `Fault 205` -> `Fault 205: object not found` rc 5 (INTERNAL AmigaShell command, reachable from vsh). W; verdict moves from C to W |
| 18 | `Prompt` with `%N %S %R` | set prompt string | prompt is bash PS1 (SH_PS1_DEFAULT vsh.c:1986, prompt escapes sh_exec.c:1780ff); C:Prompt only sets cli_Prompt, untested effect | `%` has no meaning in PS1 (bash uses backslash escapes) | Measured on rig 7, 2026-10-26: `Prompt "%N> "` and `Prompt "%N.%S> "` in a script exit rc 0 and `$PS1` is unchanged (empty in non-interactive vsh, so no PS1 effect; interactive cli_Prompt effect not looked at). C stands |
| 19 | `Path` | show or extend command path | Shell path read through cli_CommandDir (vsh.c:795-800) plus $PATH; `Path` command itself untested | none | Measured on rig 7, 2026-10-26: `Path` -> `Current_directory` / `C:` rc 0 (the AmigaShell path list, not `$PATH`); `Path ram: ADD; Path` prints the same two lines and `$PATH` stays `/UP-Term/bin:...:/c`: the command runs but vsh's lookup does not see it. W for lookup, C for `path` editing both |
| 20 | `Assign` | list/make assigns | C:Assign external; danger noted in memory (no target removes the assign) | none | W (external) |
| 21 | `CD` with no argument prints dir | `cd` shows current dir | goes to $HOME, else `SYS:` (sh_exec.c:2023-2026) | `cd` goes home | X (bash wins) |
| 22 | `/` as parent, `cd /`, `cd ::` | `/` parent, `::` ... | `/x` is tried as the parent's x first, then Unix `/vol/x` (vsh.c:1425-1445 lock_name, sh_path.c) | `/` is root | X for bare `cd /` is wrong on the AmigaDOS side: measured `cd sys:s; cd /; pwd` -> `System:` (parent, AmigaDOS meaning wins); at a volume root `cd /` -> `vsh: cd: /` rc 1 (no Unix root). `cd ::` -> `vsh: cd: ::` rc 1 from System: and from sys:s (not supported). Measured 2026-10-26; `cd ::` is C |
| 23 | `..` | not an AmigaDOS name | `./x`, `../x` mapped to Amiga names (vsh.c:316) | same | W |
| 24 | Case-insensitive command names | `LIST` = `list` | resolve uses case-insensitive same_name (vsh.c:1504-1541) for directory matches; builtins are case-sensitive strcmp (sh_exec.c builtins table) | commands are case-sensitive | W for programs (AmigaDOS FS is case-insensitive); builtins X |
| 25 | Device-relative `df0:` and `:` root | `df0:`, `:c/list` = current volume root | passed as AmigaDOS names untouched (plan 2026-09-29-vsh.md:37) | `:` is the no-op builtin, only as a command word; `:c/list` is one word, no clash | Measured on rig 7, 2026-10-26: `df0:` as a path: `dir df0:` -> `Device not configured` rc 1 (rig has no df0 disk; the name reaches DOS untouched, W). `:c/vsh` as path: `list :c/vsh` lists System:c/vsh, and as a command word `:c/vsh -c "echo viaroot"` -> `viaroot` rc 0 (W). But `ls :c/vsh` and `ls :c` -> `No such file or directory` (a Unix tool does not take `:`). W for native commands and command words; Unix tools C |
| 26 | `Dir List Type Delete Rename MakeDir Copy` vs Unix names | native C: commands | both run when on PATH or C:; `type` is the bash builtin b_type (sh_exec.c:4706), so Amiga `Type file` is shadowed; `list` vs `ls` resolve independently (no alias between them) | `type` = describe command | X for `type` (bash wins; `c:type` or `/c/type` reaches Amiga); W for the rest |
| 27 | Ctrl-C break vs Ctrl-D | Ctrl-C breaks, Ctrl-\ is EOF | Ctrl-C passed to jobs (vsh.c:81ff); `ignoreeof` is an inert shopt | Ctrl-D EOF at empty prompt | W (Ctrl-C), X (Ctrl-D keeps bash meaning) |
| 28 | `Echo "*N"` | newline | see row 5 | literal | X |
| 29 | `#?` pattern | any string | `#?` as a word: `#` starts a comment after whitespace in bash and in sh_parse.c:336, so `list #?` loses the pattern. Quoted `"#?"` passes to the program | comment start (clash) | X |
| 30 | `%` `~` `(a|b)` patterns | AmigaDOS patterns | globs: `~` is tilde (home), `(a|b)` is a subshell syntax error unquoted, `%` plain char; extglob available in sh_expand.c:881-898 | tilde and subshell | X unquoted; quoted forms reach the native command and W |
| 31 | `T:` and `ENV:` conventions | temp dir and env | T: used for temp (vsh.c:366-371, sh_exec.c:1388, 5502); ENV: vars imported (vsh.c:2054), ENV:vsh/vshrc sourced (vsh.c:2211) | `/tmp`, `$TMPDIR` | W |
| 32 | `EndCLI` `EndShell` | end the Shell | AmigaDOS Shell internals with no file in C:, so vsh found neither builtin nor file and handed the word to a child Shell, which ended itself: nothing visible happened (owner: "endcli doesn't exit up-term, only exit does"). Now builtins that run b_exit (sh_exec.c builtins table), matched in any case through find_builtin | `exit` | W, measured on rig 8, 2026-10-26 (rig lines in thoughts/spot/term-findings/endcli-2026-10-26.md) |

## Counts (32 rows, one verdict each; mixed rows counted by the first verdict)

- Works (W): 3, 7, 11, 12, 13, 14, 15, 17, 19, 20, 23, 24, 25, 31, 32 = 15 (rows 7, 11, 14, 19, 24, 25 carry an untested or failing sub-part; 17 moved C to W and 10 moved W to C on 2026-10-26)
- Queued or landed (Q): 1, 2 = 2
- Can coexist, not done (C): 8, 9, 10, 16, 18 = 5
- Clash, bash wins (X): 4, 5, 6, 21, 22, 26, 27, 28, 29, 30 = 10
- Total 32. Rows are counted once; the "mixed" notes above name a second verdict inside a row.

## Ranked "can coexist" candidates, by daily-typing value

1. Alias space form (`Alias ll "list LFORMAT=..."`), row 8: a habit typed daily, bash `alias x y` has no valid meaning that would break.
2. Prompt `%N %S %R` (row 18): set once in startup scripts, but Amiga users paste it from the manual; map into PS1.
3. `Set name value` and `SetEnv`/`GetEnv` live in `$var` (rows 9, 10): scripts and the Shell's own docs use them daily; the two-argument non-`=` form is an error in bash.
4. `Why` (row 16): small builtin, typed after every failed command.
5. `Fault n` (row 17): small, rarely typed; cheap to add beside `Why`.

Not on the list because they clash: `?` help, `;` comment, `cd` bare, `*N` escape, `#?` (bash wins). Ways around them without breaking bash: quote the arguments (`list "#?"`) or use `-h`-style help in the program.

## Gaps in this audit

Rows 8, 9, 10, 11, 16, 17, 18, 19, 22, 25 were measured on rig 7 on 2026-10-26 (this section's earlier "not measured" is superseded). Still reading only: row 7 `<arg>` substitution, and `Prompt` effect in an interactive vsh. The `vsh -c` path through ami.py exec is not a clean test of `$name` (the outer AmigaShell expands first); use script files. Row 22's earlier reading (X for `cd /`) was wrong on the AmigaDOS side.
