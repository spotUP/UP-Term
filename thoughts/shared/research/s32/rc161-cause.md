# rc161 hunt, session 33: cause NOT pinned (eliminations + clues)

Reproduced on rig 5: `vsh -c "UP-Term:bin/true; echo st=$?"` -> st=161; same binary from AmigaShell rc 0.
`vsh -c "Run UP-Term:bin/mkdir RAM:r2"` (Shell/Run path, SystemTags in runner) WORKS: only the runner's direct RunCommand path fails.

## Code reading
- ix_startup.c:176-178 (2525442): WIFSIGNALED(exit_val) -> 128+WTERMSIG. Only turns p_xstat low 7 bits into rc.
- Contradiction in rc161.md: old lib (pre-2525442) "also 161" yet old code returns WEXITSTATUS = (x>>8)&0xff. Then 161 would be exit(161)/p_xstat=0xA1xx, not a signal.
  Re-measure old lib on this rig before trusting "128+33" (the handoff's 33 may be a misreading).
- libsrc/crt0.c ENTRY: res = extend_stack_ix_startup(...) is the rc; crt0 stack extension (__init_stk_limit) runs before.

## Probe vsh (scratch worktree, VSHPROBE env bits read in runner/start_job); each bit run with UP-Term:bin/true, all still 161
- runner tc_SigRecvd = 0x100 (SIGB_DOS from the job message), SigAlloc 0xffff, SigWait 0 -> no break bits pending.
- SetSignal(0,0x100) before RunCommand: 161.
- Not cli_StandardInput/Output override, cli_Module set, SetProgramName, pr_CES, SelectInput/Output: 161.
- Not the out-of-band IXA1 argv (plain "args\n" line): 161.
- Not NP_CopyVars TRUE, not NP_StackSize 65536: 161.
- Not RunCommand stack arg (262144 or 4096 instead of command_stack): 161.
- Runner task: tc_TrapData=0, tc_TrapCode=ROM default 0x00f9fcb6, tc_UserData=0, tc_ExceptData=0, tc_ExceptCode=0x00f83558 (unchecked against a Shell-run process).

## Remaining suspects (not tested)
1. Process built by CreateNewProc(NP_Entry runner, NP_Cli TRUE): something ixemul reads in startup (pr_CLI fields, cli_CommandName, cli_Prompt/SetMode, pr_HomeDir, pr_ConsoleTask/pr_FileSystemTask, cli_CommandDir/path) is empty. Compare a Run-created process (works) field by field.
2. Why Run (SystemTags -> Shell) works but RunCommand does not: RunCommand in an NP_Entry process has no Shell around: tc_ExceptCode non-zero (0xf83558) may make the library's signal/exception code (sig_launch via tc_ExceptCode, machdep.c) treat it differently.
3. Next probe: DEBUG_VERSION ixemul (KPRINTF) in scratch worktree + `rig.py wasabi debug`, or MuForce (rig start --max --mmu) to catch a CPU trap; also dump the same task fields from a Run-started process for diff.

## Proposed regression test (once cause known)
Rig test through vsh's top-level: `vsh -c "UP-Term:bin/true; echo st=$?"` expects st=0 and `mkdir RAM:x` creating RAM:x; sentinel: file created.

## Debug trace (s33 agent 2)

CAUSE (high confidence): not a signal. The program calls exit(161) from ixnet's init_inet_daemon (ixemul-vtcon ixnet/socket.c ~1065-1100).
The "33" / 128+sig reading was wrong: p_xstat = 0xA100 = W_EXITCODE(161,0), so WIFSIGNALED is false and ix_startup returns WEXITSTATUS = 161.
sig_exit / sendsig were never entered (no "sig_exit" line in the trace).

Method: ixemul DEBUG_VERSION (-DDEBUG_VERSION, plus KPRINTF added in ix_startup after the longjmp and in sig_exit; scratch worktree, docker/build.sh variant, library only), installed as VTC:ixp6/ixemul.library on rig 5 (IXEMUL= at rig start), `rig.py wasabi debug`. Trace: build/rig5/wasabi/debug.log (rig 5, may be overwritten).

AmigaShell run (rc 0), tail of __ix_cli_parse:
  _cli_parse.c:461: argv[0] = [UP-Term:bin/true]
  __plock: file_name = ENV:   (the next DOS work after the argv build)
  _cli_parse.c:472: argc = 1
  _cli_parse.c:473: leaving __ix_cli_parse()
vsh -c run (st=161): same lines up to
  _cli_parse.c:461: argv[0] = [UP-Term:bin/true]
  ix_startup.c:128: ix_startup: longjmp back, p_xstat=A100 u_sig=0
No argc line, no "leaving __ix_cli_parse": the exit happens between line 461 and 472, i.e. in
  _cli_parse.c:466  netcall(NET_init_inet_daemon, argc, argv)   (taken because u.u_ixnetbase is open)
-> ixnet init_inet_daemon, AmiTCP branch: dm = (struct DaemonMessage *) me->pr_ExitData; NULL -> return -1 (normal);
non-NULL -> TCP_ObtainSocket(dm->dm_Id, ...) fails on garbage -> exit(DERR_OBTAIN). DERR_OBTAIN = 161 by the rc and p_xstat (the macro's header was not found on disk, value inferred; not read).

Runner field: pr_ExitData. vsh creates the runner with NP_ExitData = (ULONG)j (shell/vsh.c:1272, also 2062 and the SystemTags at 721) and uses it as the job key (vsh.c:145-159 sh_child_is, 802 sh_runner_is). An AmigaShell process has pr_ExitData NULL. ixemul reads pr_ExitData as an inetd DaemonMessage for every CLI program. `Run X` works because Run's new process does not inherit it.
Also: the old lib "rc 0" fits: exit(161) was then ... (not re-measured; the old lib returned the same 161 per rc161-root.md, which fits exit(161), not a signal).

Proposed fix (not written):
1. ixemul-vtcon ixnet/socket.c init_inet_daemon AmiTCP branch (~1078): accept pr_ExitData only if it is a real DaemonMessage: dm->dm_Message.mn_Node.ln_Type == NT_MESSAGE (mn_Length >= sizeof(struct DaemonMessage), mn_ReplyPort set); a vsh job struct fails that. Root fix at the reader, since pr_ExitData belongs to whoever creates the process. Same check in init_d (AS225 branch) if it reads pr_ExitData.
2. Optionally also vsh: key the runner->job link on something other than pr_ExitData (a table keyed by task), so no foreign struct sits where inetd's goes.
Regression test (rig, top-level): `vsh -c "UP-Term:bin/true; echo st=$?"` prints st=0 and `vsh -c "mkdir RAM:x"` creates RAM:x; sentinel: a KPRINTF/counter in init_inet_daemon that the reject path was taken (call-count 1), must fail with the check reverted (st=161). Host-side test of the validity predicate as a pure function.
