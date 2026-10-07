---
date: 2026-10-07
topic: vsh versus bash 5.x gap audit, to plan "bash for Amiga"
tags: [vsh, bash, shell, gap-audit, amiga, posix-sh]
status: draft
---

# vsh versus bash 5.x: gap audit

Read-only. No code changed, no commits, FS-UAE rig not touched.

## Method and how to read the evidence

- **Probe** = ran the real `shell/sh_parse.c`, `sh_expand.c`, `sh_exec.c` on the Mac host through a
  throwaway driver (scratchpad, not committed): a POSIX `sh_os` (open/pipe/fork-exec/`spawn` by
  fork, `sh_run_child` in the child, `listdir` by readdir). There is NO standalone host vsh in the
  Makefile (`make test` builds `build/vttest_host`, which runs `tests/test_sh_*.c` against a fake
  OS). Probe ids below (e.g. `P:subst`) are one-line scripts, run with a 5 s timeout.
- **Code** = read `file:line`, no probe.
- Driver caveats (marked unconfirmed where they matter): external commands inherit the host's
  environment, not vsh's variables, so "does `A=1 cmd` reach the child's environment" and
  `export` visibility are NOT confirmed on Amiga; "command not found" prints nothing in the driver
  (status 127 only), so builtin presence was decided from the builtin table
  (`shell/sh_exec.c:1261-1275`), not from probe output. The Amiga-side OS layer (`shell/vsh.c`) was
  read, not run.
- Status: **works** / **partial** / **missing** / **n/a** (not possible on AmigaOS as such, with why).
- Effort: S = under a day inside one file, M = a few days, touches parser + executor + tests,
  L = needs a design decision or an OS-layer change.
- Counts from the structure: 26 builtins in the table (`:` `true` `false` `echo` `printf` `cd` `pwd`
  `export` `unset` `set` `shift` `exit` `return` `break` `continue` `read` `alias` `unalias` `test` `[`
  `jobs` `wait` `fg` `bg` `stack` `source` `.` `eval` `exec` `trap` `local` `getopts` `umask` `which`
  `type`; 36 names counting aliases) plus `command` handled at exec level (`sh_exec.c:1921`). Tests
  files: `test_sh_exec.c` 293, `test_sh_expand.c` 63, `test_sh_parse.c` 6 CHECK/check hits (grep
  count, a spelling count, not an assertion count).

## Headline findings (what bites first)

1. `set -e`, `set -u`, `set -x`, `set -o pipefail`, `set -euo pipefail` do not error: `b_set`
   (`sh_exec.c:767-787`) has no option parsing, so `set -e` REPLACES the positional parameters with
   `-e`. Probes `P:set_e`, `P:setdash`. Silent wrong behaviour for nearly every script written for
   bash.
2. `vsh -ec 'cmd'`, `vsh -e script`, `vsh -x`, `vsh -s`, `vsh -l`, `vsh -i` are not parsed: only the
   exact `-c` as argv[1] (`vsh.c:1321`). `make` runs recipes with `$(SHELL) -c` (and `-ec` in POSIX
   mode); `-ec` would start an interactive shell. Code only, unconfirmed on rig.
3. `test` / `[` is a small subset: no `-a -o ( )`, no `-x -s -r -w -L -h -p -nt -ot -ef -o`
   (`sh_exec.c:987-1100`; probes `P:test_a`, `P:testx`, `P:tests`, `P:testnt`). autoconf-generated
   `configure` and most build scripts use these.
4. No file descriptors above 2: `exec 3>f`, `>&3`, `3>&1`, `{fd}>` fail (`P:fdopen`, `P:fd4`,
   `P:fdvar`). `configure` uses `exec 5>&1 6>&1` unconditionally.
5. `[[ ]]`, `(( ))`, `$((` beyond `+ - * / % ( )`, arrays, `${x/a/b}`, `${x:1:2}`, `${x^^}`,
   `${!x}`, `declare`, `readonly`, `printf -v/%f/%q`, `echo -e`, `<<<`, `<<-`, `$(<f)` are all
   missing. See table.

## Table

Columns: feature | status | evidence | effort.

### Grammar and quoting

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| Simple commands, `;` `&` newline lists | works | P:semi, P:bgcmd; parser `sh_parse.c` | - |
| `&&` `\|\|` `!` | works | P:andor, P:bang | - |
| Pipelines, any stage a function/group/loop | works | P:pipe_simple, P:pipe_func, P:pipe_group (all stages but the last in processes of their own, README) | - |
| `\|&` (stderr into pipe) | missing | P:pipeamp: syntax error | S |
| Comments, `#!` line | works | P:comment, P:hashbang | - |
| Single and double quotes, backslash | works | P:dquote, P:squote | - |
| Backslash-newline continuation | works | P:linecont, linecont2, linecont3 | - |
| `$'...'` ANSI-C quoting | missing | P:dollarq prints `$a\tb` | S |
| `$"..."` locale quoting | missing (low value) | P:dollarq2 | S |
| `time` / `time -p` keyword | missing (falls to a host-less external; on Amiga not found) | P:time_ ran /usr/bin/time; not a builtin (grep "time" in shell/*.c: none) | S |
| Function definition `name() { }` | works | P:func3, P:funcparen, P:multiline_fn | - |
| Function definition with `function` keyword | missing | P:fnkw, P:fnkw2: syntax error | S |
| Empty command `;` as an error | works (same as bash) | P:emptycmd | - |
| Parse of `case` inside `$( )` with unparenthesised `a)` pattern | missing | P:cmdsubcase2 syntax error; `(a)` form works (P:cmdsubcase3) | M |
| `coproc` | missing | not in parser | M |

### Compound commands

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `if / elif / else / fi` | works | P:ifelif | - |
| `for x in ...` and `for x` (positional) | works | P:forin_nl, P:forempty | - |
| `for` with `in` list on one line using `;`-less newline escapes | works | P:forin_nl; P:forlines was my malformed probe, not a bug | - |
| `for (( ; ; ))` | missing | P:forarith: "a name is missing" | M (needs arith) |
| `while` / `until` | works | P:while1, P:until1 | - |
| `case` with `\|` patterns | works | P:casepipe | - |
| `case` terminators `;;&` and `;&` | missing | P:case2, P:case3 | S |
| `select` | missing | not in parser | M |
| `[[ ... ]]` (== =~ && \|\| ( ) -z -n -f, no word-splitting) | missing | P:dbrack: 127; P:dbrack2, dbrack3: syntax error | L (new grammar, regex engine for =~) |
| `(( ... ))` arithmetic command | missing | P:dparen1: parsed as two nested subshells; P:dparen2 creates a file named `5` | M |
| `{ ...; }` group + redirections | works | P:redirgroup, P:group | - |
| `( ... )` subshell | works | P:subsh, P:exitsub (clone process; no fork on AmigaOS, README) | - |
| Redirection on compound commands (loops, if, group) | works | P:redirloop, P:redirif | - |
| `break N` / `continue N` | works (break 2 probed) | P:break2 | - |

### Expansions

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| Brace expansion `{a,b}`, `{1..3}`, `{a..c}` | missing | P:brace, P:brace2, P:brace3: left literal | M |
| Tilde `~`, `~/x` | works | P:tilde, P:tildeassign | - |
| `~user`, `~+`, `~-` | missing; `~user` n/a (no user database on AmigaOS) | P:tilde2 | S (`~+ ~-`) |
| `$x ${x}`, positional `${10}` | works | P:posparam10 | - |
| `${x:-d} ${x-d} ${x:=d} ${x:?m} ${x:+a}` | works | P:dflt, P:alt, P:colon | - |
| `${x:?m}` aborting a non-interactive shell | partial: message printed, script continues | P:errmsg, P:exitq | S |
| `${#x}` | works | P:len | - |
| `${x#p} ${x##p} ${x%p} ${x%%p}` | works | P:pfx, P:suffix, P:suffixglob | - |
| `${x/p/r} ${x//p/r} ${x/#p/r} ${x/%p/r}` | missing | P:subst, P:substanch: bad substitution | S |
| `${x:off:len}` | missing | P:substr | S |
| `${x^^} ${x^} ${x,,} ${x,}` | missing | P:case | S |
| `${!x}` indirection, `${!prefix*}` | missing | P:indirect | S |
| `${@:2}`, `${*:2}`, `${#@}`, `${@#p}` | missing / `${#@}` wrong (prints 0) | P:at_slice, P:dollarhash | S |
| `${x@Q}` `${x@U}` transforms | missing | not probed (no code for `@` op in sh_expand.c) | S |
| Nested quotes inside `${x:-"a b"}` in double quotes | missing | P:nestedquote: bad substitution | S |
| Indexed arrays `a=(1 2)`, `${a[i]}`, `${a[@]}`, `${#a[@]}`, `a+=(..)`, `unset a[i]` | missing (needs a variable-store change: `sh_var` is name/value string) | P:arr, P:assignarr: syntax error | L |
| Associative arrays `declare -A` | missing | P:assoc | L (after arrays) |
| `x+=str` append | missing (silently wrong: no append, no error) | P:assignplus printed `a` | S |
| Command substitution `$( )` incl. nesting, status | works | P:cmdsub_nest, P:cmdsub_status, P:cmdsub_quote | - |
| Backtick substitution, nested backticks | partial (nested escaped backticks fail) | P:cmdsub works; P:bt prints literal | S |
| `$(< file)` | missing | P:cmdsubfile | S |
| Arithmetic `$(( ))` + - * / % unary - ( ) variables | works | P:arithvar, `sh_expand.c:168-260` | - |
| Arithmetic: `<< >> & \| ^ ~ ! < > <= >= == != && \|\| ?: , ** = += -= ++ -- 0x 010 base#` | missing | P:arith, arith2, arithcmp, arithassign, arithpre, arithbase, arithtern, arithcomma | M |
| Arithmetic division by zero error | works | P:arithdiv0 | - |
| Process substitution `<( ) >( )` | missing; not possible as in bash (no /dev/fd), possible via a `PIPE:` or temp file name | P:procsub: "redirection without a target" | L (design question 3) |
| Word splitting by `$IFS` (default, `:`, empty, unset) | works | P:ifs, P:ifsws, P:ifsempty, P:unsetIFS | - |
| `"$@"` vs `"$*"` | works | P:quotedat | - |
| Globbing `* ? [a-z] [!x]`, quoted/escaped | works (case-insensitive: `ctx.nocase` set on Amiga `vsh.c:1276`, bash is case-sensitive: DECISION) | P:glob, P:globquote, P:negglob | - |
| Dotfiles not matched by `*` | works | P:dotglob | - |
| POSIX classes `[[:upper:]]` | missing | P:globclass | S |
| `shopt -s extglob` patterns `@( ) *( ) +( ) ?( ) !( )` | missing | P:extglob: syntax error | M |
| `globstar` `**` | missing (a `**/f` probe returned wrong, `**` as `*`) | P:globstar | M |
| `nullglob` `failglob` `dotglob` `nocaseglob` `GLOBIGNORE` | missing | no shopt builtin | S each (after shopt) |
| Unmatched glob stays literal | works (bash default) | P:globnomatch | - |

### Redirections

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `< > >> 2> 2>&1 >&2 &> ` | works | P:inredir, P:append, P:fd2b, P:ampout, P:fddup | - |
| `&>>` | missing | P:ampappend | S |
| `<<EOF` incl. expansion, quoted delimiter, in function/pipe | works | P:heredoc, P:herequote, P:herefn, P:heredocpipe, P:herecmd | - |
| `<<-EOF` (tab strip) | missing: the shell waits for more input | P:heredocdash | S |
| `<<< word` here-string | missing | P:herestr | S |
| `<>` read-write, `>\|` clobber override | missing | P:rwredir, P:clobber | S |
| `set -C` noclobber | missing (no option parsing) | P:noclobber | S (after set options) |
| `>&-`, `<&-` close | missing | P:close: bad file descriptor | S |
| fd 3..9, `exec 3>f`, `>&3`, `3<&0` | missing; partial possible: AmigaDOS processes have only Input/Output/Error, so a shell-held fd table works for builtins and functions, native Amiga commands and most ixemul programs cannot receive fd 3 | P:fdopen, P:fd4 | L (design question 2) |
| `{fd}>file` | missing (needs fd table) | P:fdvar | L |
| `exec >file` / `exec >&2` (redirect shell's own streams) | works | P:redirexec | - |
| `/dev/null`, `/dev/stdout` | works (mapped, host-fs probe) | P:devnull, P:devstd; Amiga mapping: code unconfirmed | - |
| `/dev/stderr`, `/dev/tty`, `/dev/fd/N`, `/dev/zero`, `/dev/urandom` | unconfirmed / n/a for `/dev/fd` and urandom | not probed on Amiga | S-M |

### Variables, functions, declarations

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| Assignment, `export NAME=v`, `unset a b` | works | P:exportassign, P:unsetmulti | - |
| Prefix assignment `A=1 cmd` (scoped, restored) | works for functions/builtins; env reaching external commands unconfirmed on Amiga | P:envprefix; `sh_exec.c:1941-1955` | - |
| `export -p -n -f`, `unset -f -v` | missing | `b_export` ignores flags (`sh_exec.c:749`); P:unsetf (`unset -f f` unsets variable `-f`, f still callable: rc shows no error) | S |
| `readonly` | missing (not a builtin; `Q=2` after `readonly Q=1` succeeds) | builtin table; P:readonlyb | S |
| `declare` / `typeset` (-i -r -x -u -l -a -A -n -p -f -g) | missing | builtin table; P:declare, P:declarep | M (flags that need typed vars are L) |
| `local` (plain names, `local x=v`, dynamic scope) | works | P:local, P:localdecl, P:localnoleak, P:funcdyn | - |
| `local -a -i -r -n`, `local` with no args | missing | `b_local` `sh_exec.c:1683` | S |
| Function `return N`, `$?`, N above 255 not truncated | partial | P:funcret, P:retval prints 300 | S |
| `declare -f` / function listing | missing | P:declare_f | S |
| `export -f` | missing (silently ignored) | P:exportf | M (no child bash on Amiga: low value) |
| Namerefs `declare -n` | missing | P:nameref | M |
| Integer attribute `declare -i` | missing | P:declare | M |

### Shell variables

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `$? $$ $! $# $@ $* $0 $1..` | works | P:paramspecial, P:dollarbang, P:dollarat | - |
| `$-` | missing (empty; flags const only holds `i`) | P:dollarmin; `sh_expand.c:370` | S |
| `$_` | unconfirmed (driver saw the host env value) | P:lastarg | S |
| `IFS` `PS1` `PS2` `HOME` `PATH` `PWD` `OLDPWD` `OPTIND` `OPTARG` `TERM` `HOSTNAME` `USER` | works / imported from ENV: | `vsh.c:1283-1287`, `sh_exec.c:708-719` | - |
| `PS4`, `PROMPT_COMMAND`, `PS3` | missing | grep: none | S |
| `RANDOM` `SRANDOM` `SECONDS` `EPOCHSECONDS` `EPOCHREALTIME` `LINENO` `UID` `EUID` `PPID` `BASHPID` | missing | P:random, P:lineno (empty) | S each (dynamic variables need a hook in `sh_get`) |
| `FUNCNAME` `BASH_SOURCE` `BASH_LINENO` `BASH_COMMAND` `BASH_VERSION` `BASH_VERSINFO` `BASH_REMATCH` `BASH_ARGV` `BASH` `SHELLOPTS` `BASHOPTS` `HISTFILE` `HISTSIZE` `COMP_*` `READLINE_*` `COLUMNS` `LINES` `DIRSTACK` `GROUPS` `MACHTYPE` `OSTYPE` `HOSTTYPE` `SHLVL` `TIMEFORMAT` | missing | P:funcname, P:bashv empty | S-M each (arrays: L) |
| `PIPESTATUS` | missing (needs arrays and pipeline-status capture) | P:PIPESTATUS | M |
| `CDPATH`, `MAIL*`, `ENV`, `BASH_ENV` | missing | grep: none | S |
| `OSTYPE`/`uname` style identification for `configure` | missing | none | S |

### Builtins (bash's full list)

Present: `:` `.` `[` `alias` `bg` `break` `cd` `continue` `eval` `exec` `exit` `export` (no flags)
`fg` `getopts` `jobs` (no flags unconfirmed) `local` (plain) `printf` (subset) `pwd` (no -P/-L:
P:pwdP identical) `read` (-r only) `return` `set` (positionals only) `shift` `source` `test` `trap`
(EXIT INT TERM only) `type` (`which` too) `umask` `unalias` `unset` (no flags) `wait` (job ids) `true`
`false` `echo` (-n only) `command` (no -v -V -p), plus vsh's own `stack`.

| Builtin | Status | Evidence | Effort |
|---|---|---|---|
| `echo -e -E`, escapes | missing (prints `-e a\tb`) | P:echo_e; `b_echo` `sh_exec.c:446` | S |
| `printf` `%s %d %c %x %o %b` escapes | works | `sh_exec.c:571-700` | - |
| `printf -v var`, `%f %e %g`, `%q`, `%(fmt)T`, `%*d` | missing | P:printf_v, P:printf_fmt, P:printf_q | M (float formatting without libm on 68000: check the C runtime) |
| `read -p -a -n -N -d -t -s -u -e` | missing (only `-r`) | P:readdelim, P:readn, P:readarr, `sh_exec.c:23` | S-M |
| `read` with IFS and a here-string/pipe | works | P:ifsread, P:readopts | - |
| `test`/`[`: `-a -o ( ) -x -s -r -w -L -h -p -S -b -c -g -u -k -O -G -N -nt -ot -ef -v -R -o` | missing (only `-z -n -e -f -d -t`, string = != , int ops) | P:test_a, P:test_paren, P:testx, P:tests, P:testL, P:testnt, P:testo | S (host-side) + OS layer `stat` callbacks (M): `sh_os.exists` has `want_dir` only |
| `set -e -u -x -f -n -v -C -a -b -h -m -o NAME +o`, `set -o` listing | missing (see Headline 1) | P:set_e, P:set_u, P:set_x, P:set_o, P:seto2 | M (-e semantics: the "condition context" rules are the work; -x needs PS4 and expanded-word tracing) |
| `set -o pipefail` | missing | P:set_pf prints 0 | S after set options (needs per-stage statuses: M) |
| `shopt` | missing | table | S for the mechanism, each option separate |
| `declare typeset readonly let` | missing | see above | M |
| `mapfile` / `readarray` | missing | P:mapfile | M (needs arrays) |
| `command -v -V -p` | missing (`command` only strips functions: `sh_exec.c:1921`) | P:command_v | S |
| `builtin NAME` | missing | P:builtin | S |
| `type -t -a -p`, `hash` | partial (`type` works; no flags) / `hash` missing | P:type_ | S |
| `enable` `help` `caller` `suspend` `logout` `bind` `complete` `compgen` `compopt` `fc` `history` `dirs` `pushd` `popd` `disown` `times` `ulimit` `kill` `let` `shopt` `exec -a/-c/-l` | missing | builtin table (each a probe returned 127) | S each, except `complete/compgen/bind` L (see readline), `kill` M (see jobs) |
| `trap` signals | partial | see traps | M |
| `umask` | works | `sh_exec.c:1823` | - |
| `alias` / `unalias`, alias expansion rules (trailing space, in scripts only with shopt) | works (basic) | P:alias1; `shopt expand_aliases` unconfirmed | - |
| `eval`, `source` (+ args, `return` in sourced file) | partial: `. ./s2.sh x` did not pass `x` as `$1` | P:sourceargs printed empty (unconfirmed: could be the driver; `b_source` `sh_exec.c:1399`) ; P:returnsrc works | S |
| `exec cmd`, `exec` redirections | works | P:execr, P:redirexec | - |
| `getopts` | works | `sh_exec.c:1725-1815` | - |
| `wait [id]`, `wait -n`, `wait $!` | works / `-n` missing | P:bgwait | S |
| `jobs` (`-l -p -r -s`), `fg %n`, `bg %n`, `%+ %- %name` jobspecs | partial: `%n` yes (`job_arg` `sh_exec.c:1145`), `%+ %- %name` and jobs flags no | code | S |
| `disown`, `kill %n`, `kill -SIG pid` | missing as builtins; signals to ixemul programs via `ixkill` (README), native Amiga commands only get break | table | M |
| `cd -`, `cd` (HOME), `pwd` | works | P:cd1, P:cdhome | - |
| `cd -P -L`, `CDPATH`, `pushd/popd/dirs` | missing | table | S |

### Shell options and shopt

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `errexit nounset xtrace pipefail noglob noclobber noexec verbose allexport` | missing | see set | M |
| `shopt` options (extglob nullglob globstar nocasematch expand_aliases lastpipe ...) | missing | table | S-M each |
| POSIX mode, `--posix`, `--norc`, `--login` | missing | `vsh.c:1321` | S |

### Job control

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `&`, `$!`, `wait`, "[1] Done" notice | works | P:dollarbang, `sh_notify` | - |
| Ctrl-C to the foreground job | works (Amiga, rig-verified per README; not re-verified here) | README VSH | - |
| Ctrl-Z, fg/bg | partial: ixemul programs only, native Amiga commands cannot be stopped (README) | n/a for native commands: AmigaOS has no SIGSTOP | - |
| `set -m`, process groups, `setsid`, `tcsetpgrp` | n/a (no process groups on AmigaOS) | - | - |
| SIGPIPE on closed pipe | n/a: AmigaOS has no SIGPIPE; vsh keeps a writer list (`vsh.c:95`) | `yes \| head` termination unconfirmed | - |

### Traps and signals

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `trap ... EXIT` (also in subshell, at `exit`) | works | P:trapexit, P:trapsubshell; `sh_exit_trap` | - |
| `trap ... INT`, `TERM` | works | P:trapint, `sh_exec.c:1597` | - |
| `trap ... ERR` | missing: "bad signal" | P:traperr | S (needs errexit-style condition context) |
| `trap ... DEBUG`, `RETURN` | missing | P:trapdebug, P:trapret | M |
| `trap ... HUP USR1 USR2 CHLD WINCH ...`, numbers, `SIG` prefix, `trap -l -p` | missing | P:trapusr, P:trapl; `trap_names` has 3 entries | M (delivery is only possible for ixemul signals; native programs have SIGBREAK only) |
| `kill -l`, `kill -0` | missing as builtin | table | S |

### Command execution environment

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `$PATH` search (Unix form, `/vol/dir` mapped to `vol:dir`) | works | `sh_path_next` `sh_exec.h`, README | - |
| `#!` interpreter lines for scripts run by name | works for `./sc.sh` through the driver's host exec only: UNCONFIRMED on Amiga (AmigaDOS has its own script bit; ixemul handles `#!` for its own exec) | P:shebang is the host kernel's `#!`, not vsh | M |
| Hashing (`hash`), `command_not_found_handle` | missing | table | S |
| `ulimit`, `nice`, `nohup`, `timeout` (external) | `ulimit` missing; externals: unconfirmed whether the 86 coreutils include timeout/nohup/env/xargs (README: "86 in all") | - | - |
| `exit` status above 255, `exit` in function, subshell status | works | P:exitinfn, P:exitsub | - |
| Exit status 127/126 messages ("command not found") | works on Amiga (`err_not_found` prints; driver silent) | code | - |
| `env`/`export` reaching ixemul and native commands | works via ENV: / process-local vars (`import_locals` `vsh.c:1224`) ; unconfirmed per-variable | code | - |

### Invocation and startup

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| `vsh -c cmd [name args]`, `vsh file args` | works | `vsh.c:1319-1337` | - |
| `vsh -e -x -u -i -l -s -O -o`, combined `-ec`, `--`, `--version`, stdin script when not a tty | missing: `-` options other than lone `-c` fall through | `vsh.c:1321-1330` | S-M |
| Startup: `ENV:vsh/vshrc`, `$HOME/.vshrc` | works (not `/etc/profile`, `~/.bash_profile`, `~/.bashrc`, `BASH_ENV`, `ENV`) | `vsh.c:1352-1371` | S for `BASH_ENV` and `ENV` |
| Login/non-login distinction, `logout`, `~/.bash_logout` | missing | - | S |
| Restricted shell `rbash` | missing (low value) | - | - |

### Prompt, editing, history

| Feature | Status | Evidence | Effort |
|---|---|---|---|
| PS1 escapes `\w \W \u \h \H \$ \e \n \[ \]` | works (partial: `\d \t \T \@ \A \j \l \s \v \V \! \# \a \r \nnn \D{}` unconfirmed: not listed in `sh_prompt` comment) | `sh_exec.h` sh_prompt doc, `sh_exec.c:2640-2700` | S |
| PS1 zsh escapes, colours, command substitution in prompt | works (extra over bash) | same | - |
| `PROMPT_COMMAND`, `PS0`, `PS3`, `PS4` | missing | grep | S |
| Line editing (Emacs keys, history, Ctrl-R, suggestions, undo) | works but lives in the CONSOLE HANDLER (`handler/lineedit.c`), not in vsh: any shell in a vtcon window gets it; a plain CON:/XCON: window does not | `handler/lineedit.h:1-22`, README 242-252 | - |
| vi mode (`set -o vi`), `bind`, `.inputrc`, readline variables, `bind -x`, `READLINE_LINE` | missing; would need an editing-mode switch in the handler (ACTION packet like the existing TCGETA/SWINSZ ones) | handler/lineedit.h | L |
| Tab completion (files, commands, KingCON style) | works in the handler; knows vsh names via `send_words` `vsh.c:1103`; `complete`/`compgen`/programmable completion missing | README 253; `handler/complete_core.h` | L (programmable) |
| `history` builtin, `!!` `!n` `!str` expansion, `HISTFILE` `HISTSIZE` `HISTCONTROL`, `fc` | missing in vsh (history is the handler's, `ENVARC:vtcon.history`, last 100 lines) | README 252 | M (a handler packet to read history; `!` expansion needs the editor) |
| Multi-line continuation prompt PS2 | works | `vsh.c:1375-1390` | - |
| `shopt checkwinsize`, `COLUMNS` `LINES` | missing as variables (the handler knows the size: SWINSZ) | - | S |

## Not possible on AmigaOS as such

- `fork`: subshells and non-final pipeline stages are cloned processes (works, README); a long
  `( )` loop costs a process each; variables set in a pipeline's last stage persist (like `lastpipe`)
  only because that stage runs in-process: check that matches bash's default (it does not: bash
  runs every stage in a subshell). Unconfirmed against bash semantics for `x=1 | cat; echo $x`.
- Real signals and process groups (no SIGSTOP/SIGTSTP, no `setpgid`, no `kill`): ixemul programs get
  signals through `ixkill`; native programs get only break bits. Job-control items depend on that.
- `/dev/fd/N`, `/proc`: process substitution as in bash needs a named pipe; `PIPE:` or a temp file
  in `T:` are the substitutes.
- Per-process fd tables: native commands get Input/Output/Error only.
- File permission bits, owner, setuid, `test -x` as a permission test: AmigaDOS has protection bits
  (e, r, w, d, s, p, a); mappable, not identical.
- `~user`, `getpwnam`, `$UID` meaningful values.

## Proposed phase order

Ordered by how many real scripts each unlocks (an AI coding agent's bash tool, `configure`,
GNU make recipes, install scripts), cheapest and most-silent-failure first.

1. **Phase 1, stop the silent wrongness and make `make` and agents usable (S-M, all executor)**:
   `set` options (-e -u -x -f -C -n and `-o` names, `$-`), `vsh -e -x -u -i -l -s` and `-ec`, `test`
   completeness (-a -o ( ) -x -s -r -w -L -h -nt -ot -ef), `echo -e/-E`, `readonly`, `declare` /
   `typeset` for the non-array flags (-r -x -i -u -l -p -f), `export -p -n`, `unset -f -v`,
   `command -v/-V`, `builtin`, `time`, `kill` builtin (`-l`, `-0`, `%n`, delegating to the
   existing job signal code), `read -p -n -d -t -s -u`, `printf -v %q`, `${x:?}` abort, `x+=`,
   `return N` mod 256, `. file args`.
2. **Phase 2, expansions that agents write constantly (S, `sh_expand.c`)**: `${x/p/r}` family,
   `${x:o:l}`, `${x^^}` family, `${!x}`, `${@:n}`, `${#@}`, `$'...'`, `$(<f)`, nested quotes in
   `${..}`, `[[:class:]]`, brace expansion, `~+ ~-`, `<<-`, `<<<`, `&>>`, `<>`, `>|`, `|&`,
   `>&-`, `function` keyword, `;;&` `;&`.
3. **Phase 3, arithmetic and `[[ ]]` / `(( ))` / `for ((;;))` (M-L)**: full C operator set with
   assignment and base notation in `sh_arith`; `(( ))` and `for ((;;))` in the parser;
   `[[ ]]` as its own grammar (no splitting/globbing, `== !=` patterns, `=~` needs a small
   POSIX ERE matcher, `BASH_REMATCH` needs arrays).
4. **Phase 4, file descriptors above 2 (L, design)**: fd table in `sh_shell` for builtins,
   functions, compound commands; `exec 3>`, `>&3`, `{fd}`. Needed by autoconf `configure`.
   Decide what native commands see (design question 2).
5. **Phase 5, arrays and associative arrays (L)**: `sh_var` becomes a typed value (string, indexed,
   assoc, integer/nameref attributes); `${a[@]}`, `${#a[@]}`, `a=( )`, `+=( )`, `declare -a -A -n`,
   `local -a`, `mapfile`, `read -a`, `PIPESTATUS`, `BASH_REMATCH`, `FUNCNAME`, `DIRSTACK`. This is
   also the place for the dynamic variables hook (`RANDOM`, `SECONDS`, `LINENO`, `PPID`, `UID`...).
6. **Phase 6, traps and shell options (M)**: ERR (reuses the -e condition-context), DEBUG, RETURN,
   other signal names (delivery by ixemul only), `shopt` with extglob/nullglob/globstar/nocaseglob/
   nocasematch/expand_aliases/lastpipe, `pipefail` (per-stage status), `PS4`.
7. **Phase 7, interactive extras (L, mostly handler)**: `history`/`fc`/`!` expansion, `pushd/popd/dirs`,
   `disown`, `jobs` flags, `complete/compgen` and programmable completion, vi mode and `bind` (handler
   packet), `PROMPT_COMMAND`, `ulimit`, `times`, `hash`, `help`, `enable`, `caller`, `coproc`, `select`,
   process substitution via `PIPE:`/temp file.

## Open design questions

1. **Name and target.** Keep `vsh` as the binary and add a bash-compat mode (`bash` as a second name
   or `vsh --bash`), or make bash behaviour the default? Where bash and POSIX/zsh-flavoured vsh
   differ (zsh prompt escapes, case-insensitive globbing, `/x` meaning), which wins? Case-insensitive
   glob is right for AmigaDOS filenames, wrong for scripts that rely on `[a-z]` being lowercase.
2. **fd > 2.** Options: (a) shell-internal fd table only (builtins, functions, compound commands,
   `exec 3>`), native commands never see it; (b) additionally map `>&3` for an external command to
   a temp-file/`PIPE:` handle given as its Output; (c) leave to ixemul programs (do they get
   `dup2`-able handles from vsh's runner? unconfirmed: needs a probe in `os_run`/`runner`,
   `vsh.c:346-398`). Recommended: (a) now, because `configure` only uses 3-6 for its own redirects.
3. **Process substitution.** `PIPE:` writers with a reader that never reads can hang (the writer list
   at `vsh.c:95` exists for exactly that); a temp file is safe but not streaming. Which?
4. **Variable store.** `sh_var` is name/value strings in a linked list (`sh_expand.h`); arrays,
   integers and namerefs need a typed store. Do it once, before phases 2-6 touch it.
5. **Parser: one grammar or two.** `[[ ]]` and `(( ))` inside the POSIX parser, or a separate
   mini-parser invoked from it? The existing arena parser is C89 and host-tested; a new construct
   per `sh_kind` is the existing pattern.
6. **Compatibility target version.** Bash 5.2 semantics (`${x@}`, `wait -n`, `EPOCHREALTIME`) or a
   named subset (the one `configure` and typical agent commands use)? Sets the end of phase 3-6.
7. **Memory and stack.** vsh takes 64 KB of stack (README) and the interpreter recurses; arrays and
   `[[ ]]` add recursion and heap. Is a 68000 with 2 MB chip-only a supported target, or 68020+ with
   fast RAM? Decides whether regex (`=~`) and float `printf` are affordable.
8. **`set -e` semantics.** Full bash semantics (the condition-context rules, `||`/`&&` lists, `!`,
   function-in-condition) or the simple POSIX rule? Agents depend on the subtle cases rarely, builds
   often do. Recommended: POSIX rules exactly.
9. **Where the editing features live.** vi mode, `bind`, programmable completion are in the console
   handler, not vsh; the bash-for-Amiga work crosses that boundary (new packets). In scope?
10. **Test plan.** Reachability protocol: one test per feature driving the top-level entry
    (`sh_run_text`) with a sentinel, plus unit tests in `test_sh_expand.c`/`test_sh_parse.c`. A
    host `vsh` driver like the scratchpad one would let scripts be compared against `bash` output
    directly (differential testing); it is not in the Makefile today.

## Unconfirmed

- Everything about Amiga-side behaviour (env passing, `#!`, `/dev/*` mapping, SIGPIPE, `$_`,
  `jobs` flags) was read, not run, and the rig was off limits.
- Probes ran with the driver's POSIX `sh_os`; builtin-only behaviour (parser, expander, executor
  control flow, redirections to files, here-docs, functions, traps) is the real code and is
  reliable; anything involving external processes or environment is not.
- `[[ ]]` and `select` and `coproc` rows are from parser absence plus probes; `grep` of
  `sh_parse.c` for the keywords `"[["`, `"select"`, `"coproc"` returned nothing.
- PS1 escapes beyond those named in the `sh_prompt` header comment were not read line by line.
- The driver is in the session scratchpad and is not preserved; re-create from the description in
  Method if needed.
