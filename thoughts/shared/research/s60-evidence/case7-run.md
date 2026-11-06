# S60 case-7 verify run (rig 98, OS 3.2, 060 --max, origin/main 70567faf, worktree ../vtcon-s60-case7, make amiga rc 0)
Shell = C:Execute in the 3.2 Shell; vsh = `VTC:vsh -c "Execute .."`. Full harness log: case7 section below; probes (RAM:p scripts) after.

1. R6 FAIL: `Execute r6 ? <NIL:` shell (0,'') vs vsh (20,'w/A: execute: Bad arguments'). (With a reply fed vsh gives 'w/A: got foo' rc 0; shell side of that harness row is 'foo: Unknown command' rc 10, harness artefact.)
   R17 PASS: both (0,'inner\nback\n'). R15 PASS: both (0,'rc=0\n').
2. R27b PASS: Run RAM:rc/r27 (S bit) both '[CLI 8]'. (R27a `Run C:Wait 1`: shell [CLI 3] vs vsh [CLI 7], DIFFERS, number depends on open CLIs; not in the list.)
3. X2 PASS (probe): `CD RAM:` + bare `CD` both 'RAM Disk:'; nested CD RAM:p then bare CD both 'RAM Disk:p'. Harness row X2 (cddrv) DIFFERS only because both sides got "Can't open RAM:rc/cddrv" (shell rc 10, vsh rc 20): harness script missing at that point, not a vsh result.
4. S7c-6 PASS: `type RAM:p/x.txt` shell and vsh both (0,'hello file\n'); `type RAM:p/q5` -> 'Quit 5' (C:Type); `type cd` / `type -a cd` -> 'cd is a shell builtin' (bash). `type ls` -> 'vsh: ls: not found' rc 1 (no ls on rig; bash path). Not tested: bare file name in the current directory.
5. FailAt PASS: `FailAt 30; Execute q10; Echo a` both (0,'a'); no FailAt both (10,'Quit failed returncode 10'); FailAt 3 + nested Execute q5 both (5,'Quit failed returncode 5').
6. rc after Lab/EndIf/Else FAIL: after `Execute q5` then `Lab e` / `If 1 EQ 1 EndIf` / `If 1 EQ 2 Else EndIf`, `Echo "rc=$RC"`: shell rc=0, vsh rc=5 (all three). EndSkip: `Skip e / Lab e / EndSkip` both rc=0 (Skip already resets; EndSkip not isolated).
7. quoted '?' FAIL for double quotes: `Execute k1 "?"` (k1 = .KEY w/A) shell (0,'got ?') vs vsh (20,'w/A: execute: Bad arguments'). Single quotes `'?'` MATCH (got '?' both). Also `Echo ?` / `Echo a?` / `Echo "?"` differ in template line shown (shell prints '?' for Echo "?", vsh prints Echo's template).
8. S7c-1 PASS: R5 `Echo "x<y>z"` both 'x<y>z', R2 '<x>' both. PUBSCR-NAME-CASE SKIP: no rig tool opens a named public screen and locks it with other case (no C:PubScreen either); needs a new probe program. FRONTSCR-PUB-LOCK SKIP: rig is stock 3.2 + VTC:vsh, handler not installed; rig booted, windows/requesters list stayed empty for all items (no DefIcons requester).

## Harness case-7 lines that differ (build/rig98/vshexec-case7.txt)
R6 (above), R27a (above), X2 (harness), R26/R26b (harness does not set the S bit: shell 'file is not executable' rc 10, vsh R26b ran 'plain' rc 0). All other 37 rows MATCH (R23 R2 R17 R24 R3 R9 R13 R14 R16 R19 R11 R1 R4a R4b R5 R7 R8 R10 R12 R15 R18 X1 X1b X3 R21 R22 S7b-2).
Harness checks 4/4 ok.
