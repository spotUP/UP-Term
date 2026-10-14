---
date: 2026-10-14
topic: wasabi 0.8 replaces amiagent and amimcp (phase 2)
tags: [wasabi, amiagent, amimcp, rig, mcp, kit]
status: draft
---

# wasabi 0.9 replaces amiagent and amimcp

Owner 2026-10-14: "no more wasabi testing it goes in". Evaluation:
`thoughts/shared/research/2026-10-14_wasabi-0.8-eval.md` (branch feature/wasabi-0.8, 355dfc6:
Makefile pin 68ef646, shim). Scope (owner): UP-Term (vtcon) + amiga-pi, Up Rough Demo System,
other Amiga repos.

Decisions:
- `tools/rig/ami.py` stays the one Python API the 72 vtcon importers (and neovim-amiga's 2)
  use; its implementation becomes a client of wasabid (reuse wasabi's own client module,
  `wasabi` / `wasabi_api.py` from build/wasabi, not a re-implementation of its protocol).
- Where wasabi lacks a call (raw key codes, region grab, gadget by id, tick-timed drag), the
  adapter maps to the nearest wasabi command when one exists, and otherwise fails loudly
  naming the gap. No amiagent fallback: one daemon on a rig. Gaps go to creep (owner relays).
- One daemon on rigs and in the kit: wasabid 0.8; amiagent leaves the rig boot, the kit
  option and the docs. An upgrade's Install removes the kit's old amiagent (ours only).
- MCP: `wasabi_mcp.py` on the Mac (`.mcp.json`) and in the NAS image; the amimcp target,
  wrapper, pin file and seeding move to wasabi's.

## Checklist

- [x] **WM1** Pin wasabi 0.9b2 (ee23c09, 2026-10-14 21:29, owner: 'there is a new version
  released'; 'builds without the CyberGraphX developer headers'): Makefile target from
  feature/wasabi-0.8 updated to ee23c09; drop tools/rig/wasabi_shim if the build no longer needs
  it; 68020 build if the kit's floor wants it.
  DONE (feature/wasabi-rig 82d1e58): pinned ee23c09, shim deleted, `make wasabi` and `wasabid.020` build (102 KB each) without it.
- [x] **WM2** Rig boot (tools/rig GO/startup): start wasabid (rig key, port per rig as
  tools/rig/wasabi.py does) instead of amiagent; `rig.py start` waits for wasabi's ping.
  DONE: GO starts `BOOTX:wasabid <port> name rigN` after `SetEnv wasabi.key`; paths.wasabi_port/WASABI_KEY are the one source;
  agentpatch.py and its test deleted; rig 2 `start --max` came up with no amiagent (Status FULL lists wasabid only).
- [x] **WM3** `ami.py` over wasabid: every function the importers call (count from the
  imports: key 84, window 51, exec 48, script 25, uitree 22, get 21, ...): same signature,
  same return shapes; gap calls fail with a named error. Host test with wasabi's mock daemon
  (wasabi tests/ has one) for each function; ONE rig reachability run of the existing rig
  suites that use the most calls (pick 3, say which).
  DONE: ami.py = translator over wasabi's client (Conn, do_run, read_ui, grab_pixels, ...); tools/test_rig_ami.py (mock daemon)
  covers every function; rig 2: prefs_rig (42 ami calls), snip_rig (13), pf1_rig (12) pass; tabs_rig (15) and menus_rig (11) see
  Gaps. Named gaps: BREAK, POINTER. Mapped: raw keys, region grab, gadget by id, tick-timed script (host-timed waits).
- [x] **WM4** Mac MCP: `.mcp.json` server `amiga` -> wasabi_mcp.py through a wrapper with the
  same rig/env selection as tools/rig/amimcp.py (paths.py port rule); amimcp target, wrapper
  and test replaced. Reachability: stdio initialize, tools/list, one run on rig 2.
  Evidence (branch feature/wasabi-mcp-kit: tools/rig/wasabimcp.py, tools/test_rig_wasabimcp.py (6 checks, ONLY=rig_wasabimcp), amimcp target, wrapper, test and amimcp.rev removed; stdio initialize + tools/list (19 tools, amiga_*) + amiga_run "echo wasabi-mcp-ok" returned rc 0 against RIG 3 (not rig 2: other agent) with wasabid 0.9b2 started by rig.py wasabi on)
- [x] **WM5** NAS: image installs wasabi (pinned, one pin file read by Makefile and
  Dockerfile, as amimcp.rev was), mcp.json -> wasabi_mcp.py, `amiga-target` writes wasabi's
  host/port/key, skill amiga-upterm text updated; as-claude drop-root kept; tests updated.
  Evidence (tools/nas/wasabi.rev (ee23c09 full sha) read by the Dockerfile; Makefile wasabi target must read it (one line, owner merges, test_the_makefile_wasabi_target_reads_the_one_pin_file is red until then); mcp.json -> /opt/wasabi/wasabi_mcp.py; amiga-target writes ~/.config/wasabi/config 0600 keeping other lines; seeding records carried over from amimcp; compose tag 7; 6 tests in tools/test_uptelnetd.py AmigaTargetTool, 4 fail on the old tree)
- [x] **WM6** Kit: one daemon option (wasabid 0.8, MIT) replaces amiagent + the 0.2 wasabi
  option; Install, Installer, Uninstall, MANIFEST, README, LICENSES; upgrade removes our old
  amiagent and old wasabid; dist tests; rig install case.
  Evidence (dist/install.dos, Install.installer, Uninstall, README, LICENSES, SOURCE.txt, kitsize, Makefile dist (daemon and client from build/wasabi, no binary in git); dist/net/amiagent-0.13.2 moved to tools/rig/amiagent-0.13.2 for the rig until WM2/3 drop it; tests/test_dist_installer.py WasabiDaemonOption (7 tests); make dist-check green on a kit re-staged from the main build + mkkit.py base (a from-scratch make dist needs vendor/ NDK, not in a worktree); tools/rig/install_rig.py --only wasabi on rig 3: 29 of 29 (old amiagent removed, wasabid 0.2 replaced by 0.9b2, key, startup block, started, Mac client pings it, Uninstall clean))
- [ ] **WM7** Docs and skills: RULES.md, tools/rig/README.md (Debugging recipe, MCP), NAS
  README, amiga-pi skills, memory notes that name amiagent.
- [x] **WM8** (done in that repo by another session: 4e44b0865, 98806b0c8, 280d855fe, merge 6578dd989; vendored client tools/wasabi pinned ee23c09; owner M1 on the Replay open) Up Rough Demo System: `a1200.py`'s own AMI0 client + its 5 tests move to
  wasabi's client (that repo's rules; its own branch, no push).
- [x] **WM9** Other repos: neovim-amiga (2 scripts import vtcon's ami.py: verify they work
  through the adapter), DCTelnet vendored copies (replace or delete), list any others found.
- [ ] **WM10** Gaps list for creep kept current in the research doc (owner relays).

## Verification

Host: ami.py adapter tests against wasabi's mock daemon; make test (affected suites);
dist-check. Rig (2 or 3, 060): rig boot with wasabid only, 3 rig suites through ami.py, MCP
stdio run, kit install case. Owner: NAS deploy, real Amiga.

WM9 exemption: DCTelnet (dctelnet-vtcon, dctelnet-v2) holds a vendored snapshot of vtcon under src/third_party/vtcon (ami.py, rig.py, vttest_rig.py): it changes only by that repo's resync from vtcon, which another session owns; nothing edited from here.
