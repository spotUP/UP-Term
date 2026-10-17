---
date: 2026-10-17
topic: finish-open-plans run, session 8
tags: [handoff, finish-run, real-amiga, wasabi, gcc, git]
status: final
ledger: thoughts/shared/plans/2026-10-15-finish-open-plans.md
previous: thoughts/shared/handoffs/2026-10-17_finish-run-session-7.md
---

# Finish-open-plans run, session 8

Ledger is the source of truth; grep `^- \[ \]`. vtcon main is ahead of origin by 48, not pushed.

## Done this session

- wasabi 0.11b2 pinned (be4c055, efc7f040). Rig 3 `install_rig --only wasabi` 29 of 29; the case restarts the kit daemon on KIT_WASABI_PORT (1283 + rig number) because the rig daemon holds WASABI_PORT.
- zlib -O2 root cause: gcc m68k peephole2 "combine clr if possible" merged `clr.w x` + `clr.l x+2` into `clr.l x` (trees.c bi_windup). Fix gcc aee333a76 on fix/zlib-o2 in cpython-amiga/build/gcc/src; new cc1 installed (old kept as cc1.pre-clrmerge-2026-10-17). Regression test f2dc4dfc.
- upterm-ports: zlib back at -O2 (8fe2ed8); git port merged (3bea7b2).

## Real Amiga (owner's A1200 with Replay board): in progress

An old UP-Term kit install broke the owner's real Amiga.

- Shell: FIXED by the owner. They restored a pre-UP-Term S:User-Startup and removed the ROM module. The module is "UP-Term ROM 1.0" (RTF_AFTERDOS, pri -101) and only exists in a `make rom` custom Kickstart, never in a kit.
- Network: STILL DOWN. Symptoms:
  - `ShowNetStatus` gives local host 0.0.0.0, no default gateway, DNS 127.0.0.1.
  - `AddNetInterface replayeth` says "object exists", so the interface is added.
  - DEVS:NetInterfaces/replayeth has `configure=dhcp`; a DHCP request times out.
  - A static .250 did not answer; the Mac got no ARP entry.
  - The Replay link light is green.
  - The cable from the working Raspberry Pi was moved to the Replay: no change. Cable and switch port are ruled out.
- Router: TP-Link Deco BE22 mesh, main unit "Teknikrummet", 192.168.0.1, LAN 192.168.0.0/22. The web UI shows a client list only; it has no reservation or block-list pages (those exist only in the Deco app).
  - The Amiga is not in the 20-client list, so the router never sees it.
  - The owner says no clients are blocked in the Deco app.
  - The old Amiga addresses .57 and .58 are now an iPhone and "Johan" (DC-B5-4F-28-F5-9A).
- Conclusion: the fault is on the Amiga side (replayeth.device, its config, or the Replay hardware), not the router or the cable. The kit (dist/install.dos) has no network configuration, so this needs evidence before any blame.

## Next steps, in order

1. Ask the owner for the Amiga's own output; it can't be reached remotely yet:
   - `Type DEVS:NetInterfaces/replayeth`, the whole file. Look for `hardwareaddress=` or odd options.
   - The last 10 lines of `NetLogViewer` after `ConfigureNetInterface replayeth ADDRESS=DHCP`.
   - `Version DEVS:Networks/replayeth.device`, and the file date. Did a backup restore swap the driver?
   - `List DEVS:NetInterfaces`. A second interface file on the same device would conflict.
   - Does the Replay show its MAC in its own menu (OSD)? Compare it with NetLogViewer.
2. Once the Amiga is online, get its IP, wasabi port and key. Install the current kit, then run the real-Amiga tests: wasabi 0.11b2, compression byte-compare, git cases.
3. Root-cause why the old kit's XCON / Shell icon (the SHELLICON option, 265bcefb) broke the shell on real hardware. This needs that kit's VERSIONS file from the owner.
4. Owner approvals pending:
   - FULL KIT REBUILD with the new cc1 (any binary may carry the bad clr merge).
   - Upstream PR of the clr-merge fix to bebbo/gcc (outward-facing).
5. Queued on rig 2, one agent at a time:
   - the UP-Term: assign bug
   - git network via ix_vfork_resume (FK1/FK2)
   - a perl case that loads a module
   - rsync FK3/FK4, after git network passes
6. Owner items: FP1.10 verdict, colorsim, xxd stpcpy (ixemul-vtcon), FP0.6, locate (FuzzyFind).
7. FP1.9 waits for load < 5. Rig 3 stays up until the owner says stop.

## Gotchas

- Deco web login is one device at a time and expires; it asks for the owner's TP-Link password, which the agent never enters.
- The Deco web UI hash routes redirect to #networkMap. Read the client list via javascript_tool on the "Clients" view (MAC regex over the visible leaf text).
- Never change router settings without explicit owner approval.
