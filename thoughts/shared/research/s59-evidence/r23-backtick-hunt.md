# R23 (S7c-2): Echo [`Type file`] in an Execute script printed [] under vsh

Rig 98, OS 3.2, C:Type 47.3, 2026-11-06.

## Symptom
- Shell `Execute RAM:rc/r23` gives `[l1 l2 l3]`. `VTC:vsh -c "Execute RAM:rc/r23"` gives `[]`.
- The bug is not specific to Execute. In a vsh script, `echo "[$(Type RAM:rc/data3; echo rc=$?)]"` gives `[rc=10]`.
- These all worked: plain `Type f` at top level, `Type f | ...` inside $( ), and `Echo hi | Type f` inside $( ).
- Snoop: Type's task opens CONSOLE:, RunCommand returns 10, and Type never opens data3. Type failed at its template, not when reading or writing.
- The evidence that "typed works" was invalid. The AmigaDOS Shell expands the backtick before vsh starts.

## Root cause
The cause is in shell/vsh.c, subshell_proc.
- The $( ) subshell is a CreateNewProc process. Its Input() and Output() are the NIL: handles that CreateNewProc gives.
- A foreground command spawned there gets `j->in = io->in`, which is the real input.
- Its `j->term_in` is `Input()`, which is that NIL: handle. This happens in the FG branch: `j->term_in = Input();`.
- The runner puts term_in into cli_StandardInput, so Input() != cli_StandardInput.
- C:Type 47.x then treats its input as redirected. It takes the template without FROM and fails silently with rc 10.
- A pipeline stage (term_in = j->in) never had this mismatch. Neither did the top-level shell, where Input() is the real input.

## Fix
subshell_proc selects j->io.in and j->io.out as the subshell's own Input() and Output() for sh_run_child. A forked Unix subshell has its parent's descriptors in the same way. The NIL: handles are put back before the process ends, because the process end closes them.

## Rig results after the fix (rig 98)
- `VTC:vsh -c "Execute RAM:rc/r23"` gives (0, '[l1 l2 l3]\n'). vshexec_rig.py 7 gives `ok 1 R23: vsh shows '[l1 l2 l3]'`.
- R2 `Execute RAM:rc/r2 val` gives (0, 'a\n<x>\n'). Case 7 R2 is MATCH.
- $( ) probes: 2>file, >file and `Echo hi | Type f` all give rc 0.
- `Type f <f` inside $( ) still gives rc 10. That input really is redirected, so this follows Type's own rule.
- Case 7 still differs on these items, which this fix does not touch:
  - R6 (S7c-3, another agent)
  - R27a (the CLI number only)
  - R27b, R17 and R15
- rig.py reboot: the rig came up. The DOpus desktop window is present, IPrefs and wasabid run, and there was no freeze.

## Host test
No host test is possible. The mismatch is between AmigaDOS Input() and the cli_StandardInput that C:Type 47.x reads. The os layer in vsh.c does not build on the host. The host sh_exec suite still passes: 691 checks, 0 failed.
