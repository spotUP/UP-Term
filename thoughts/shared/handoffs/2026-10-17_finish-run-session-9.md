---
date: 2026-10-17
topic: finish-open-plans run, session 9
tags: [handoff, finish-run, real-amiga, wasabi, gcc, git, kit-rebuild]
status: final
ledger: thoughts/shared/plans/2026-10-15-finish-open-plans.md
previous: thoughts/shared/handoffs/2026-10-17_finish-run-session-8.md
---

# Finish-open-plans run, session 9

Ledger is the source of truth; grep `^- \[ \]`. Read the ledger lines 24-30 (A1200) and the FP4.4 git
sub-bullet before acting.

## Done this session
- Real A1200 (Replay) back on the network: loose server-room cable (session 8's "Amiga side" was wrong).
- wasabid access confirmed: key in `~/.config/upterm/a1200-wasabi.conf` (mode 600, `key = ...`).
  Never print it; pass it via env:
  `AMI_HOST=192.168.0.66 AMI_PORT=1234 AMI_TOKEN="$(sed -n 's/^key = //p' ~/.config/upterm/a1200-wasabi.conf)" python3 tools/rig/ami.py ping`
  A1200: wasabid 0.11b2, 68060 + 68882, 172M RAM, no `UP-Term:` assign (no kit installed), NAS share
  `spotUP:` mounted (fast path to copy the kit).
- git network: push hang fixed (bd732f6), upterm-ports main merge 5f834d0, pushed (owner approved).
  The push also carried 15 older local-only commits (fast-forward, nothing overwritten).
- gcc clr-merge PR: branch `fix/m68k-clr-merge-mode` (5a4cfa02d on amiga6) ready in
  amiga-gcc/projects/gcc. NOT submitted: codeberg.org was down (HTTPS 503, SSH closed). Submit command
  in thoughts/shared/research/2026-10-17_gcc-clr-merge-pr.md.
- Disk: removed 4 stale other-session scratch dirs (~220M). The big ones are live (UpRough tauri dev,
  AmiExpress jest) or hold dirty worktrees (AmiExpress 61ba7259 31 dirty files, a90b37d9
  feature/w-settings-door 1 dirty file): left alone, owner's call. ~4.6 GiB free.

## In flight
- FULL KIT REBUILD agent (sonnet), started session 8/9. Progress: session-9 scratchpad
  `rebuild-progress.md` (cpython cc1 switch, libixcompat, ports libs done). Result file:
  thoughts/shared/research/2026-10-17_kit-rebuild-result.md. Its report arrives only in session
  6a06d735; if that session is gone, read the result file and check the progress file.

## Next, in order
1. Check the rebuild evidence (every binary rebuilt with the fixed cc1, `make dist` kit built).
2. Install the rebuilt kit on the A1200 (via `spotUP:` share or ami.py put), then real-Amiga tests:
   wasabi 0.11b2, compression byte-compare, git cases (git now includes push).
3. Submit the gcc clr-merge PR once codeberg answers.
4. SHELLICON root cause (needs the old kit's VERSIONS from the owner).
5. Queued: perl module case, rsync FK3/FK4, owner items (FP1.10, colorsim, xxd stpcpy, FP0.6, locate),
   FP1.9 when load < 5.

## Shared A1200
The UpRough session (up-rough-demo-system-32) also drives the A1200 (wasabi M1 test, a1200.py
durable-copy fix; it owns that work). Agreed 2026-10-17: each side warns the other before a reboot,
reset or long job on the A1200. Their M1 notes were their own. A running Up Rough demo holds the whole
machine until left-mouse quit: if wasabid stops answering, ask that session before anything else.

## Owner decisions open
- Symptoms of the "UP-Term: assign bug".
- The two dirty AmiExpress worktrees (1.3G).
