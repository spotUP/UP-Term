---
date: 2026-10-15
topic: NAS deploy (NL21) done, wasabi pinned at v0.9, NL22 next on the real Amiga
tags: [nas, wasabi, claude, handoff]
status: final
---

# Handoff 2026-10-15: NAS on image 9, wasabi v0.9, NL22 waits for the real Amiga

## Tasks

- [x] NL21 NAS deploy (plan `thoughts/shared/plans/2026-10-14-nas-claude-local.md`, ticked with evidence).
- [x] wasabi pin bumped 0.9b2 (ee23c09) -> v0.9 (f378c51), ami.py POINTER, gap list updated.
- [ ] NL22 real Amiga (owner only; steps below).
- [ ] wasabi gaps 1, 4, 6, 7, 10: upstream only; message for creep drafted in this session (owner pastes it).

## Critical references

- Ledger: `thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`, last ~15 lines (section `## Queue run 3 2026-10-14`).
- Plan: `thoughts/shared/plans/2026-10-14-nas-claude-local.md` Phase E (NL21 ticked, NL22 open).
- Gaps: `thoughts/shared/research/2026-10-14_wasabi-0.8-eval.md` section 6, block "Status at wasabi v0.9".
- NAS files: `tools/nas/` (compose.yaml image `claude-amiga:9`, `wasabi.rev` f378c51), `tools/uptelnetd.py`.
- Seeder rule: `tools/nas/seed-claude-files` (keeps any file with no seed record that differs from the image's).

## Recent changes (all pushed to origin/main)

- 883a450e merge feature/wasabi-0.9: ami.py POINTER from the WINDOWS "S" line (MouseX, MouseY), test in `tools/test_rig_ami.py`.
- 6e907874 (merged) pin corrected to f378c51: `v0.9` is a lightweight tag on f378c51; the first agent pinned d5a57f4 (3 commits older, without 3639e2e: SUM, requester gadgets, MCP slots/break).
- NAS image tag 8 -> 9 in `tools/nas/compose.yaml` and `tools/test_uptelnetd.py`.
- Gates: ONLY=rig_ami, installer, uptelnetd, rig_wasabimcp rc 0; make dist rc 0; dist-check rc 0. Rig 2: `wasabid 0.9`, pointer read back, `wasabi sum` answers.

## State of the NAS (Megadrive, 192.168.0.198; outside: https://spotup.fr1.quickconnect.to/)

- Project claude-amiga at /volume1/docker/claude-amiga, image claude-amiga:9, running.
- compose: NAS_IP "192.168.0.198", ALLOW "192.168.0.0/24 10.8.0.0/24".
- Log after the 08:21:43 restart: uptelnetd listening on 192.168.0.198 port 2323 allowing both ranges; relaying wasabi clients on 127.0.0.1:1234; no setpw request.
- `pairs list`: "no Amiga is paired" (expected before NL22).
- Backups: old project files in /docker/claude-amiga-backup-2026-10-15; old seeds in home as `work/.mcp.json.pre-wasabi-2026-10-15` and `.claude/skills/amiga-upterm/SKILL.md.pre-wasabi-2026-10-15`. home/ otherwise untouched.

## Learnings

- No ssh on the NAS (port 22 refused). Work goes through DSM in the Claude-controlled Chrome tab group. The LAN tab needs the self-signed certificate accepted in that group. Outside the LAN, use the QuickConnect URL.
- The permission classifier refuses some DSM actions (new tab to QuickConnect, edits inside home/, reading home/.config/uptelnetd/clients). Ask the owner for permission in chat; never work around a refusal.
- The NAS had run image 4, from before the seed records existed. Its .mcp.json and SKILL.md were therefore "kept" and still pointed at amimcp. Renaming them let the seeder install the image's copies. Any other install that jumps from an image without seed records hits the same thing.
- uptelnetd binds only NAS_IP. A VPN client reaches 192.168.0.198 only if the VPN Server gives LAN access, which is off today. Adding 10.8.0.1 to NAS_IP would make start fail while the tun is down (serve() OSError is fatal). Not tried from outside.
- dist-check prints FAIL lines and still exits rc 0: they are planted faults in `tests/test_rig_fixtures.py` (asserted at 494-496), not failures.
- Worktree builds need VTCON_NDK, VTCON_NETINCLUDE, AMISSL_SDK and UPTERM_ROOT pointed at the main checkout's vendor/.
- Agents misreport. Check their claims from the source: the pin hash and the image tag were both wrong in one report.

## Next steps (in order, at home)

1. NL22 on the real Amiga:
   1. Run `Claude REMOTE` (or start Claude from the icon and pick the NAS).
   2. The first start asks for the password, then says "paired with this NAS".
   3. Quit and start again: no password.
   4. Ask Claude Code "what volumes does my Amiga have?": it answers from that Amiga.
   5. Press Ctrl+]. `Status` shows no session agent. The amiga tools then say no Amiga is connected.
   6. In an UP-Term window, Right Amiga+L opens a Claude tab.
2. The real Amiga needs wasabid 0.9 from the kit (Claude.lha carries it). If it runs OS 3.0/3.1 or MUI, consider pinning ba83a43 (fixes after v0.9: GadTools `ui` on 3.0/3.1, 16 KB stack, MUI 3.8 windows).
3. After NL22 passes: run `pairs list` on the NAS (Container Manager > claude-amiga > Open terminal > `pairs list`); the Amiga must be listed.
4. wasabi gaps: wait for creep's answer; update section 6 of the research doc.

## Other notes

- Rig 1 (owner's, fs-uae pid 46062) ran all day at a full core; rigs 2 and 3 are stopped.
- The 18:57 reminder was cancelled (NL21 done).
- The DSM terminal dialog in the QuickConnect tab still has a stale session ("No isatty terminal found"); close it.
