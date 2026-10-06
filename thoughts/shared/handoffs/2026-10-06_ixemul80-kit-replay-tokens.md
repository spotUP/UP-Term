---
date: 2026-10-06
topic: ixemul 80.1 landed, one-drawer kit with Claude/Python/nvim, Replay test, gcc 16 parked, token rules
tags: [ixemul, kit, claude, replay, gcc16, tokens]
status: final
---

# Handoff 2026-10-06: ixemul 80.1, one-drawer kit, Replay, gcc 16

## Task(s)

1. **ixemul 80.1 migration** (ixemul-vtcon). DONE and the default. 47 of 50
   plan items; open R7, M10.1, M10.2.
2. **gcc 16 build of ixemul** (M10). PARKED by the user ("whats next?").
3. **One-drawer kit**: `C:Claude` goes straight to the NAS, `C:ClaudeCode`,
   Python 3.14 and Neovim 0.12 in `SYS:UP-Term`. DONE (W49).
4. **Replay test** (real A1200, 192.168.0.58, amiagent). IN PROGRESS: the
   Replay dropped off the network during Install; waiting for the user to say
   what its screen shows.
5. **Token rules**: global `~/.claude/CLAUDE.md` §6b "Token budget" plus §10
   "Images never enter the main context" and "Session model". DONE.

## Critical references

- vtcon ledger: `thoughts/shared/plans/2026-09-28-vtcon.md` (W46 to W49).
- ixemul plan: `~/Code/ixemul-vtcon/thoughts/shared/plans/2026-10-05-ixemul-80-migration.md`.
- Desktop kit: `~/Desktop/UP-Term/` holds `UP-Term.lha` (27.8 MB, full kit),
  `backup-UP-Term-ixemul48.lha`, `TESTING.txt` and `for-the-replay/`.
- Replay amiagent token: read at runtime from
  `~/Desktop/UP-Term/for-the-replay/A1200-network-kit/Install-Agent`. Never
  store or print it. Use with `AMI_HOST=192.168.0.58` (plus `AMI_TOKEN`) and
  `tools/rig/ami.py`.
- gcc 16 compiler: `~/Code/amiga-gcc15/projects/gcc`, branch
  `feature/m68k-save-reg-and-float-return-d0` (e32636be50), installed in
  `~/opt/amiga16`. Patch copy in the old session scratchpad (gone with it).
- SDK backup: `~/opt/ixemul-sdk-backup-2026-10-05.tgz`.

## Recent changes

vtcon (main):
- 38b930e ixnet in kit and rig; 842467d waitset (one WAIT_CHAR per task);
  480d127 XCON exit fix; b092708 ClaudeCode script and icon; d074f3e
  one-drawer kit; 8294e6c plain `Claude` reads `ENVARC:Claude/remote` and runs
  uptelnet (install_rig 62/62); bf8f4ca `ami.py` env host/port/token.

ixemul-vtcon (feature/ixemul-80, HEAD e0f7026):
- 80.1 import and build fixes, AF_UNIX on `sock_stream` (423aff5), sun_path
  (2ae1c37), gcc 16 portability (40852da), `string/libc_impl.h` no loop
  distribution (e8bcc9d). 48.2 stays on `stable-48.2`.

neovim-amiga be3c4e5: compat netdb.h defers to 80.x's.

## Uncommitted (vtcon)

- `tools/rig/ami.py`: chunked `put()` (`PUT_PART = 8 << 20`, parts
  `remote.partN`, `Join ... AS remote`, then delete parts). Needed because the
  amiagent frame limit is 16 MiB and `UP-Term.lha` is 28 MB. Not yet tested to
  completion on the Replay. Commit after the Replay put succeeds.
- `tests/amiga/mallocbench.c`, `tools/rig/_h8rig_clip`: untracked, origin not
  checked this session. Look before committing or deleting.

## Learnings

- AmigaDOS `.KEY` template is limited to about 200 characters; longer gives
  "Illegal KEY directive" and the script does not run.
- `uptelnet` needs a console; run with `<NIL:` it hangs. Tests open a window.
- Protection bits: LFORMAT `%A`, not `%P`.
- FS-UAE 060 rig shows a Workbench-reset requester on every boot; `rig.py`
  clicks Retry by gadget label (not the 0x0 gadget).
- Ctrl+B is raw 0x35; 0x23 is F.
- gcc 16 on m68k AmigaOS needs: `-mfloat-return-d0`, `-fcommon`, `-std=gnu99`
  (C23 default), memory clobbers on volatile asm, d0/d1 clobbers on
  Supervisor() asm, and no loop distribution in string functions. Mixed
  gcc 2.95/16 archives take the object format of their first member.
- After a gcc `.opt` change, rebuild cleanly (`rm gcc/*.o libbackend.a
  libcommon*.a`), or argument passing breaks.
- Docker image `ixemul-gcc295` had to be rebuilt after Docker data was wiped
  (likely the full disk).

## Next steps (ordered)

1. Replay: get the user's report of the screen. Then Uninstall or redo
   Install, reconnect with `AMI_HOST=192.168.0.58`, put `UP-Term.lha` with the
   chunked put, and verify Install, `Claude`, `ClaudeCode`, Python and nvim.
2. Commit `tools/rig/ami.py` once step 1's put works.
3. W47: resize does not reach ixemul programs.
4. W46: setup as `/` commands and menu items (colours, exports). Needs the
   user's list of what they want.
5. Python warning "Could not find platform independent libraries" when started
   through vsh (prefix not found).
6. R7 freeze recipe (ixemul plan).
7. M10 gcc 16 library: remaining hang is gcc 16 `string` objects with the
   gcc 16 rest. Next probe: bisect gcc 16 string units inside an all-gcc 16
   build for duplicate symbols or data placement. Only with the user's go
   (cost checkpoint, §6b).

## Other notes

- Session model: Sonnet for steps 1 to 5; Opus only for step 7 or a crash with
  no known cause.
- Never run more than 2 emulators; max 3 subagents.
- Never enter passwords; the owner types them. No API key in the NAS
  container; the A1 endpoint stays LAN-only.
- Keep 48.2 as backup if 80.x fails.

## Update 2026-10-06 (later the same day)

- Replay = FPGA Arcade Replay board (Amiga core amiga_db060), now at 192.168.0.57 (DHCP moved it;
  reserve MAC 54:10:ec:cd:e3:60 in the Deco app). Access details: memory note replay-and-pi-lab.
- Checklist: thoughts/shared/plans/2026-10-06-replay-hardware-tests.md -- 2 of 14 pass (H1 Claude from
  the Amiga to the NAS; H13 Synergy). H2 Install freeze PINNED to install.dos line 393 (Copy
  LIBS:ixemul.library to .orig); next step is the owner running that Copy by hand, then to RAM:.
- Done today: W51 Uninstall fix (518e65b), chunked ami.py put (b9110b9), W52/W53 dropped (eriQue's
  auxcon), W54 tmux over auxcon, W55 native multiplexer idea, W56 browser thin client (all ledger).
- asynergyc on the Replay (SYS:UP-Term/asynergyc, ;BEGIN asynergyc in S:User-Startup) -> Deskflow on the
  Mac (192.168.0.70; the Mac uses a randomized Wi-Fi MAC, so its IP can move).
- Pi 4 "amigrabber" (ssh amigrabber, key ~/.ssh/amigrabber_ed25519): serial bridge running, camera
  service from Up_Rough_Demo_System. The grabber/stream belongs to the up-rough-demo-system session;
  don't touch the Pi's camera service or the Replay SD card from here.
- Open: which port the serial cable is on (watch ~/amiga/serial.log on the Pi during a Replay reboot);
  kit rebuild with the W51 fix before H14.
- Everything committed and pushed: vtcon, ixemul-vtcon (all branches), cpython-amiga, neovim-amiga,
  upterm-ports, tmux-amiga, screen-amiga, amiga-gcc16 (patch repo).
