---
date: 2026-11-02
session: vtcon-ee
topic: vsh $( ) around a pipeline with an external last stage gave "" (release blocker)
---

# $( ) of an external pipeline printed nothing

## Reproduction (rig 67, C:vsh md5 e9c7456420d33adf29d544c6625f48cf = main's build/amiga/vsh)

Script run as `Stack 200000` / `C:vsh RAM:r3c.sh >RAM:r3c.out` (file output, as wasabi):

```
x=$(sed --version | head -1); echo "[$x]"      -> []
echo "[$(echo abc | sed s/b/X/)]"               -> []
echo "[$(echo abc | cat)]"                      -> []
echo "[$(command -v sed)]"                      -> [UP-Term:bin/sed]
echo abc | sed s/b/X/                           -> aXc
{ echo grp | cat; } | cat                       -> (no line at all)
y=$(sed --version); echo "[${y%%<nl>*}]"        -> [sed (GNU sed) 4.10]
```

A single external in $( ) works; every PIPELINE whose last stage is external loses its output
when the shell's stdout is a pipe: inside $( ) (the capture pipe) and inside a stage of an outer
pipeline (`{ a | cat; } | cat`). Not ixemul, not the $( ) child as such.

## Root cause

exec_pipeline (shell/sh_exec.c, `exec_cmd(sh, st[i], &sio[i], 0, &job[i])`) starts every external
stage with wait 0, the same value as a `&` job. os_start (shell/vsh.c, the `!wait` branch) then gives
the last stage, whose output is the shell's own (not owned) stream, a handle of its own:
`j->out = bg_own_stream(j, 0, io->out)` (fe76c6a7, VSH-JOB-SIGNALS). On a PIPE: stream
DupLockFromFH fails (no lock), so bg_own_stream returns `Open("NIL:")`: the output is discarded.
With the shell's stdout a FILE the dup works, so the plain pipeline printed aXc.

Before fe76c6a7 a non-interactive out was "the shell's handle itself, as Unix shares the
descriptor"; the commit meant the outlive-the-shell handle for `&` jobs and hit pipeline stages too.

Secondary: core_subst1 spawned the $( ) child as SH_SPAWN_BG (wait 0, stage 0): input NIL: and an
own handle on stderr, though the shell waits for it. Now SH_SPAWN_STAGE (keeps the shell's streams,
bash: $( ) inherits stdin).

## Fix

os.run's wait takes the spawn values: SH_SPAWN_FG / SH_SPAWN_BG / SH_SPAWN_STAGE. exec_pipeline
passes STAGE; exec_cmd1 treats STAGE as not waited (unchanged logic) but hands it to os.run; os_start
gives a STAGE the shell's own out/err handles on a file or pipe (close 0), `&` jobs keep
bg_own_stream. core_subst1 spawns STAGE.

## Results (2026-11-02)

- Host: `make test ONLY=sh_exec` 578 checks, 0 failed. With the two sh_exec.c call sites reverted
  (exec_pipeline back to wait 0, core_subst1 back to subshell_mode(...,0,0,1)) the new checks fail:
  test_sh_exec.c:1163/1164 last_wait = 0 want 2, :1169 last_spawn_wait = 0 want 2.
  The fake f_spawn now runs a job synchronously only for SH_SPAWN_FG (a STAGE spawn returns a job id,
  as the real os_spawn does); with `if (wait)` it returned the status as a job id and
  `x=$(exit 3); echo $?` gave 0.
- Rig 67, fixed vsh: repro script prints `[sed (GNU sed) 4.10]`, `[aXc]`, `[abc]`, `aXc`, `grp`
  (before: `[]`, `[]`, `[]`, no `grp` line).
- Rig 67, tools/rig/vshpipe_rig.py: NOT clean, open.
  - pure main (main's script, main C:vsh): echo_wc OK, printf_wc / func_stage / three_stages hung
    (20 s), the redirect_* cases OK. So builtin-first pipelines into the staged coreutils hang on
    main already (flaky: which case hangs changes run to run).
  - fixed vsh (C:vsh refreshed to it by the rig tools): echo_wc hung, then the Amiga stopped
    answering wasabi (150 s). Two runs, same result. Not yet separated from the main flake;
    host had two other fs-uae rigs at 80-100% CPU at the time.
  - Note: vshpipe_rig.py / rig.py start from the worktree refresh VTC: vsh from the worktree build,
    so VSH= alone does not keep main's C:vsh.
- tools/rig/vsh_orphan_job_rig.py: not run (rig froze first).
