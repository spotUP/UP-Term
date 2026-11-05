# VSH-AMIGA-SCRIPT-MODE S7a: R-checks, real Shell vs vsh (2026-11-05)

Rig 97, `--os 32` (vsh built from main d22757ff, `make build/amiga/vsh` rc 0), then `--os 31` for the real half.
Harness: scratchpad `rcheck.py` (scripts in RAM:rc/, each case through `c.run`; vsh half as `VTC:vsh -c "<same command>"`).
Output text is stdout+stderr as returned by wasabi, `\n` shown literally; rc is the command's return code.
Caveats: (1) the rc shown for vsh errors is the `VTC:vsh -c` wrapper's rc, not the script's. (2) This rig has no C:Fail, so
"Fail n" is `Execute RAM:rc/qn` (a script holding `Quit n`). (3) Unix-style `|` etc. as typed. (4) S-bit files got `Protect +s`.
(5) The first 3.2 pass hit a Guru in vsh (`DEADEND ALERT #81000005 exec: corrupt memory list detected in FreeMem`, task VTC:vsh)
between vsh R6 and R7; the rerun of the identical sequence did not repeat it. Unexplained, one occurrence: needs a hunt of its own.

Counts: 33 rows, MATCH 15, DIFFERS 18, NOT RUN 1 (R20 Ctrl-C needs a human at the window) and 3.1 for the vsh half.

| item | script / command | real 3.2 (rc) | real 3.1 (rc) | vsh (rc) | verdict |
|---|---|---|---|---|---|
| R1 | `If EXISTS RAM:rc` typed | `` (0) | same (0) | `` (0) | MATCH |
| R2 | `.KEY x` on line 2, arg `val` | `a\n<x>\n` (0): directive ignored | `a\nval\n` (0): honoured | `a\nval\n` (0) | DIFFERS (3.2 only) |
| R3 | `.KEY w/A`, no arg | `EXECUTE: Parameters unsuitable for key "w/A"\nrequired argument missing\nExecute failed returncode 10\n` (10) | same | `execute: required argument missing\nVTC:vsh failed returncode 20\n` (20) | DIFFERS |
| R4a | `.KEY s/S`, arg `s` | `[s]` (0) | same | `[s]` (0) | MATCH |
| R4b | same, no arg | `[]` (0) | same | `[]` (0) | MATCH |
| R5 | `Echo "x<y>z"` no .key | `x<y>z` (0) | same | `x<y>z` (0) | MATCH |
| R6 | `execute file ?` stdin NIL: | `` (0) | `w/A: EXECUTE: Parameters unsuitable for key "w/A"\n...Execute failed returncode 10\n` (10) | `execute: Bad arguments\nVTC:vsh failed returncode 20\n` (20) | DIFFERS |
| R7 | `Set v=hi`, `Echo "q:$v"`, `Echo u:$v` | `q:hi\nu:hi\n` (0) | same | `q:\nu:\n` (0): Set not visible as $v | DIFFERS |
| R8 | `Echo hi >file there` then Type | `hi there` (0) | same | `hi there` (0) | MATCH |
| R9 | `FailAt 5`, nested rc 10, then Echo | `Quit failed returncode 10\n` (10) | same | `Execute failed returncode 10\nVTC:vsh failed returncode 10\n` (10) | DIFFERS (message names the failing command) |
| R10 | `If ABC EQ abc` | `eq` (0): case-insensitive | same | `eq` (0) | MATCH |
| R11 | NOT EXISTS / EXISTS / GT / WARN / `EXISTS x VAL 1` | `c1\nc2\nc3\nnowarn\nc5\n` (0) | same | `c1\nc2\nc3\nnowarn\n` (0): no c5 | DIFFERS (last form: real accepts, vsh skips) |
| R12 | `If` without EndIf at EOF | `in` (0) | same | `in` (0) | MATCH |
| R13 | `Skip` no label | `object not found\nSkip failed returncode 10\n` (10) | same | `Label  not found\nVTC:vsh failed returncode 10\n` (10) | DIFFERS (text) |
| R14 | `Skip nolab` | `object not found\nSkip failed returncode 10\n` (10) | same | `Label nolab not found\n...returncode 10` (10) | DIFFERS (text) |
| R15 | `Execute q1`, `Skip end`, `Lab end`, `EndSkip`, `Echo "rc=$RC"` | `rc=0` (0) | `` (1): script ended, rc 1 | `rc 1` (0): `=` lost, RC 1 | DIFFERS |
| R16 | `FailAt` no args | `Fail limit: 10\nx\n` (0) | same | `10\nx\n` (0) | DIFFERS (no "Fail limit: " prefix) |
| R17 | FailAt 3 outer, nested FailAt 30 + rc 10, then rc 5, Echo back | `inner\nback\n` (0): nested limit leaks to outer | `` (10): aborts | `inner\nExecute failed returncode 5\n` (5): outer limit restored | DIFFERS (3.2 leaks, 3.1 and vsh abort differently) |
| R18 | `Quit 3` in nested Execute | `after rc=3` (0) | `` (3) | `after rc=3` (0) | MATCH (3.2) |
| R19 | `Quit abc` | `bad number\nQuit failed returncode 10\n` (10) | same | `` (0), "after" not printed | DIFFERS |
| R20 | Ctrl-C during a script | NOT RUN | NOT RUN | NOT RUN | NOT RUN |
| X1 | rc after If/Else/EndIf/Lab (`Execute q5` first) | `in\nrc=0\nrc2=0\n` (0) | `` (5) | `in\nrc 0\nrc2 0\n` (0) | DIFFERS (only the `=` lost in vsh Echo "rc=$RC"; rc values equal) |
| X1b | same, no trailing Echo | `in` (0) | `` (5) | `in` (0) | MATCH (3.2) |
| X2 | `CD` inside executed script kept by caller | `RAM Disk:rc` (0): kept | `Ram Disk:rc` (0): kept | `Empty-12MB-FFS:` (0): not kept | DIFFERS |
| X3 | `Echo a \| Echo b` in script | `b` (0): pipe works | `a \| Echo b` (0): literal | `b` (0) | MATCH (3.2) |
| R21 | backticks inside double quotes | `q:hi\nu:hi\n` (0) | same | `q:hi\nu:hi\n` (0) | MATCH |
| R22 | unclosed backtick | `` `Echo hi\nnext `` (0): literal, continues | same | same (0) | MATCH |
| R23 | multi-line / empty output | `[l1 l2 l3]\n[]` (0): newlines to spaces | same | `[]\n[]` (0): multi-line gives empty | DIFFERS |
| R24 | `.key` file, no S bit, by name | `RAM:rc/r24: file is not executable\nRAM:rc/r24 failed returncode 10\n` (10) | same | `k24 foo` (0): ran it | DIFFERS |
| R25 | NUL in first 255 bytes, S-bit file by name | `before` (0) | `before\nobject not found\nCD failed returncode 20\n` (20) | `before` (0) | MATCH (3.2) |
| R26a | S-bit `.KEY a,b` by name, args `one two` | `one\|two` (0) | same | `one\|two` (0) | MATCH |
| R26b | S-bit, no .key, args `one two` | `plain` (0) | same | `plain` (0) | MATCH |
| R27a | S-bit script piped `\| Type *` | `Type failed returncode 10\n` (10) | `s27 ran` (0) | `` (0): output lost | DIFFERS |
| R27b | `Run <S-bit> >file` | `[CLI 3]` printed; file `s27 ran` (0) | `[CLI 3]`; file empty after 3 s | no banner; file `s27 ran` (0) | DIFFERS (banner only) |

Notes for the design: 3.2 vs 3.1 differ on R2, R6, R15, R17, R18, X1, X3, R25, R27; the 3.2 behaviour is the one vsh should copy.
Not verified: vsh on 3.1 (NOT RUN), R20.
