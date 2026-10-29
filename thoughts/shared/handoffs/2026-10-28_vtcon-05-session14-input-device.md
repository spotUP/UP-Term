---
date: 2026-10-28
session: vtcon-05 session 14
topic: RIG9-INSTALLER-CRASH narrowed to input.device; UHC demo prep handed to vtcon-32
---

# vtcon-05 session 14 handoff

## Slot rules (unchanged)
- Our rigs: 9, 10, 11. Never touch rigs 1, 2, 3, 4, 7, 8. Never stop rig 1.
- Stop a rig only after the last command plus 5 s quiet.
- upterm-ports main checkout is on another session's branch port/htop: use worktree upterm-ports/.claude/worktrees/kit-main.
- ixemul-vtcon main checkout: leave the dirty include/sys/fcntl.h, include/sched.h, compat/test/fstatat_probe and the worktrees ixemul-mkdir, np-cli, wb-cd alone.
- Never write images/nyhd2.hdf without owner approval. Keep VTC:Songs in every rig.

## Done this session
- ixemul-vtcon PR #7 (poll.c comment), upterm-ports PR #22 (test_amisslstubs_no_poll.sh), UP-Term PR #7 (git long-name volume prose): all merged.
- RIG9-INSTALLER-CRASH hunts 7-10. Findings: thoughts/spot/term-findings/rig9-installer-loop.md, sections "## hunt 8", "## hunt 9", "## hunt 10". Ledger rows in thoughts/spot/todos.md after the hunt 6 row.

## RIG9-INSTALLER-CRASH state (start here)
- Hunt 10 (debug fs-uae, `f 00F80DC8` before first `g`, no snoop): halt on the exec dead-end RESET after the Installer reached "(100% done)".
  - Faulting task: input.device (task 4000985A, ThisTask and low memory 0x104 agree).
  - Alert 80000004 (illegal instruction); hunt 8 saw 80000003 (address error). Both are "CPU jumped into garbage".
  - Fault PC 40D15F30, user-mode frame at ISP 40002280, vector 4. PC is inside a pointer table in Z3 RAM (40CDD734, 40CDD75C, 40CDD784), data not code.
- Hypothesis (unproven): an input handler whose code was unloaded or overwritten stays on input.device's handler chain. The next input event (mouse move, the Proceed click) calls it. That fits hunt 6 (reset on the TCPAIP page before any click) and the owner's "froze on clicking Proceed".
- The exec Alert patch at the Z3 address is wasabid's guru recorder (GuruStash, build/wasabi/src/patches.c ~1186), not the bug. GuruStash stays empty on this path.
- wasabi snoop during the Installer stalled rig 9 at the first Execute in both hunt 9 runs (cause unproven). Hunt 10 ran without snoop and reached the end.
- Tool facts: installer_rig.py args `FORCE ONLY default`; flags via env UPTERM_RIG=9 UPTERM_FSUAE=/Users/spot/Code/Emulators/fs-uae-debug/fs-uae UPTERM_RIG_FLAGS="--max --fresh --mmu --debugger"; installer_rig truncates debugger.log on restart, so send `f`/`g` right after "Next PC"; debugger.log is block-buffered (flush with ~30 `m 408E5000 100`); `m` takes hex addresses; SegmentTracker `Zs` printed nothing.

## Next hunt (hunt 11, one sonnet agent, ~35 tool calls)
Question: which program owns the input handler that input.device calls at the fault?
1. Static first (cheap): read the Installer script (dist/Install*, grep for the TCPAIP page and the steps between "(100% done)" and it) and list every program it runs there. Any of them that installs an input handler (IND_ADDHANDLER, commodities, input injection) and exits or is killed without IND_REMHANDLER is a suspect. Also grep vtcon sources (up-console.device, vtcon-handler, UPTerm, wasabid input injection) for IND_ADDHANDLER / IND_REMHANDLER pairing.
2. Live: same hunt 10 setup. At the halt, walk input.device's handler list (input.device base from ExecBase DeviceList; the handler list is private, so locate it from the handler-node pattern: Interrupt nodes with is_Code) and print each handler's is_Code, is_Data and ln_Name. The node whose is_Code is near 40D15F30 / 40CDD7xx names the owner.
3. Prove it: run the suspect alone on a clean rig, exit it, move the mouse: same reset = proven.

## Other open items (unchanged order)
- UHC demo: vtcon-32 owns it (push 770f5202, full make test, kit rebuild from main, walk the Launch row and a live theme change, fresh rig 7 for the owner).
- curl rig check cases; upkg two-directory install rig proof (8c76c96d); RIG-KIT-DERIVE; RIG-RECHECK-STALE; INSTALLER-CLICK-SPEED step 3; TLS-SYSROOT; AmiNetXDuo case B.
- Parked: IXEMUL-CRT0-CTORS, branch fix/crt0-ctors (b37b93e): needs a rig proof, then the PR, the toolchain install and the curl https retest.
