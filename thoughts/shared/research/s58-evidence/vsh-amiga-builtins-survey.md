# vsh and AmigaDOS Shell built-ins (AMIGA-IN-VSH survey)

Date 2026-11-05. Research only. Sources: shell/sh_exec.c builtins[] table (~4788-4803), find_builtin + amiga_internals (~6876-6896), b_stack (3282), b_why (5949), RULES.md.

Caveats (unverified):
- No AmigaDOS manual or Shell docs were found in the repo or ~/Code/Reference (grepped for endskip/failat). The command list is the ledger row plus my memory of 3.1/3.2.
- "Internal vs C: file" varies by release. Verify per command on a rig or the A1200 with `Resident` and `Which`. Rows marked "?" are those I am unsure of.
- "Today" is from the builtins[] table only. A name not in the table resolves through vsh.c resolve(): cwd, then PATH, then C:.
- Unix name clashes are with vsh's own builtins. Policy (RULES "Unix and Amiga forms"): both syntaxes coexist, and bash wins only on a real clash.

| Command | vsh today | Unix counterpart in vsh / clash | Recommendation | Size |
|---|---|---|---|---|
| Alias / Unalias | builtins `alias`, `unalias` (bash syntax). The Amiga form `alias name value` may already parse; unverified. | same names, bash form. Clash is only in the syntax of `alias` with no args or `alias name value`. Accepting both is compatible. | native: accept `alias name value` (no `=`) as well, and the AmigaDOS list output. | S |
| Ask | none; runs C:Ask if present (? internal in 3.1) | `read` / `read -p`. No name clash. | native: set status 5 on yes and 0 on no, read from the tty. Reuse b_read. | S |
| CD | builtin `cd`. Implicit CD on a bare directory name exists (line 975). | `cd`. Same name. | already done; only check `cd` with no args printing the current directory (AmigaDOS) against bash going home. This is a real clash, so bash wins. | S (check) |
| Echo | builtin `echo` (bash form) | same name. AmigaDOS `NOLINE` and `FIRST/N`/`LEN/N` keywords differ. | native: add `noline`, `first`, `len` keywords only when the first word is one of them. | S |
| If / Else / EndIf | none as Amiga commands. The parser has Unix `if/then/else/fi`. Else and EndIf are C: scripts-only words. | `if ... fi`, `[ ]`. No name clash with `fi`, but `else` is a keyword already. | native in script/`source` mode only, to run AmigaDOS .bat style scripts: needs a line-oriented pre-pass. Real work. Or leave to C: (the C:If binary only works under the AmigaDOS Shell script reader). | M |
| Lab / Skip / EndSkip | none (C: only, depends on the Shell script reader) | `goto` does not exist in sh. No counterpart. | skip unless AmigaDOS scripts must run under vsh. If Amiga scripts are a goal, do with If/Else in one "Amiga script mode" item. | M |
| FailAt | none (? internal) | no direct counterpart. Closest is `set -e`. | native: store the fail limit so a failing script line aborts at rc >= limit. Needs a script-mode hook. Otherwise accept and ignore, returning 0. | S |
| Fail | none (C:) | `exit N` | leave to C: | - |
| Fault | none; runs C:Fault if present (? internal in 3.1) | `b_why` uses os.fault to get the text, so the OS hook exists. No Unix counterpart (`strerror` is the nearest). | native: `fault N` prints "Fault N: text" via os.fault. Reuse b_why's hook. | S |
| Why | builtin `why`, case-insensitive. | none | done | - |
| EndCLI / EndShell | builtin, mapped to b_exit, case-insensitive. | `exit`, `logout` | done | - |
| Get / Getenv | none; runs C:GetEnv if present. `$VAR` expansion reads ENV: at each expansion (line 790). | `echo $VAR`, `printenv`? (unverified) | leave to C:, since the file works. Optionally native `getenv name` for speed. | S |
| Set / Setenv / Unset / Unsetenv | `set` and `unset` are bash builtins (set: options and positionals, unset: vars). Setenv/Unsetenv have no builtin: C: files. Unset clears the local variable and leaves ENV: alone (comment line 3986). | `export`, `unset`. CLASH on `set` and `unset`: AmigaDOS `set name value` vs bash `set -e` / `set -- a b`. | `setenv`, `unsetenv`, `getenv`: native, backed by the ENV: store that expansion already reads (S each). `set name value` / `unset name`: bash wins on a real clash; accept `set name value` only when the first word is not an option and not `--` and has 2+ args (bash would then set positionals, so it is an actual clash: skip). | S |
| Path | none; C:Path binary (? internal in 3.1). The PATH variable is Unix style and vsh resolve() looks at cwd, PATH, C:. | `PATH=` / `export PATH` | native `path [add dir ...] [show] [reset] [remove]` that edits PATH in vsh. A C: Path binary would change the process's CLI path list, not vsh's PATH, so it does nothing visible. Worth doing. | M |
| Prompt | none; C:Prompt (? internal) would change the CLI prompt, not vsh's. | `PS1` | native: translate `%N`, `%S`, `%R` into PS1. | S |
| Quit | none (C:Quit sets return code in scripts only) | `exit N` / `return N` | native alias of `exit`? Not the same (Quit only sets rc, no exit). Leave. | - |
| Resident | none; runs C:Resident if present; affects the process CLI list, not vsh. | `hash` (nearest) | skip | - |
| Run | not a builtin. vsh.c ~1399 special-cases `run <ixemul cmd>`. Otherwise C:Run. | `cmd &` (background). Name clash none. | native: `run cmd args` == `cmd args &` and print `[CLI n]`. Makes jobs visible in `jobs`. Probably good value. | M |
| NewCLI / NewShell | none; C:NewShell opens a new window. | none (`vtcon` window command is outside vsh) | leave to C: (check it works from vsh with a console: path). | - |
| Stack | builtin `stack [bytes]` (b_stack, 3282); "not here" when no os.stack hook. | `ulimit -s` (comment line 5912 says it maps to this) | done | - |
| Date / Dir / List / Copy / Delete / Type / Sort | C: files | `ls cp rm cat sort` (Unix twins exist) | leave to C:. dist/vshrc keeps these names unaliased (lines 108-110, 178). | - |

## Suggested picks for the owner
Cheap and visible: `setenv`, `unsetenv`, `getenv`, `fault`, `path`, `prompt`, `ask`, `run`, Amiga-form `alias`, `echo noline`. These are all S, apart from `path` and `run` (M).

Decision: AmigaDOS script flow control (`If/Else/EndIf/Skip/Lab/FailAt`) is one M-or-larger item, "Amiga script mode". It is only worth it if .bat style scripts must run under vsh.

Not worth doing: `set`/`unset` Amiga semantics (real clash, bash wins), `Resident`, `Quit`, `NewShell`.
