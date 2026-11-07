# B2 ipatch gate hardening (hunt 3 sections 5-7)

Gate (38 bytes, built for run-time address SELF): 52B9 <SELF+34> | 2F3A 0012 | 660C | 53B9 <SELF+34> | 588F | 2F3A 0008 | 4E75 | hook@26 | saved@30 | busy@34.
Hooks drop their own busy++, call ipatch_leave(&<their ipatch>) last; gates are never freed (ipatch_forget); *_remove waits
(Delay(10), sentinel AMARCHY_<PATCH>_BUSY_WAIT every ~10 s) until the gates' busy words sum to 0, then frees g_pub.
wbr_rec.busy was only used by the hooks: replaced by a pad field (layout unchanged).

## Verdicts
Before (amarchy/ stashed, tests kept):
- A `make test ONLY=wbreset`: rc=2, tests/test_wbreset.c:138 excess elements in array initializer; :144 too many arguments to wbr_gate_build (compile failure; the old 22-byte gate has no 4-arg builder, so the behavioural stepper could not run against it).
- B `python3 -I tests/test_ipatch_guard.py`: FAILED (failures=3): test_gates_are_never_freed, test_hooks_do_not_count_themselves_and_leave_through_the_gate, test_remove_waits_then_frees_with_no_return_between.
After:
- A: [OK] wbreset: 95 checks, 0 failed
- B: Ran 4 tests ... OK
- Amiga build `make build/amiga/Amarchy`: rc=0 (vbcc, warnings-as-errors).

Not done: rig validation (Amarchy restart series), by design.
