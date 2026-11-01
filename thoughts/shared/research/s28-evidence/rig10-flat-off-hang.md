---
date: 2026-11-01
topic: s28 flat OFF hang, rig 10 (060, OS 3.2), build de007867
status: open - cause not yet named, no fix
---

# Rig 10: Amarchy stops answering after a flat prefs write

## Setup
- Rig 10 `--max --os 32`, purpose "Amarchy s28: flat OFF hang hunt".
- RAM:Amarchy = build/amiga/Amarchy from de007867 (reload() calls desk_dos_flat_sync).
- UP-Term:bin/amarchy-flat md5 18d0667922958fbae7265ee377007c20.

## Stack 262144 probe (mandatory first probe): the hang STAYS
- Started with `Stack 262144` then `Run >NIL: RAM:Amarchy`; ps showed STACK 262144.
- Wrote a prefs file with flat-system-look=on and flat-window-gadgets=on to ENV:Amarchy/prefs.
- amarchy-flat started: STATUS `flat=on calls=0 chained=0`.
- Right after this ON write Amarchy stopped answering: `rexx AMARCHY STATUS` gave no reply in 30 s.
- The task stays in state `ready` over repeated ps samples (busy loop, not a Wait). FREE 233502, so SP is about 28.6 KB deep.
- Conclusion: the stack size is not the cause, and the hang starts with the ON sync. The reported OFF symptom
  (STATUS stays flat=on) follows: Amarchy is already dead, so the OFF write is never handled.

## Where it spins
- 15 s snoop on `*Amarchy*`: zero DOS calls. Pure CPU loop.
- taskdump of the saved context: sp 102d4fae, stack 1029bf90-102dbf90. Saved PC/first register moved
  1000deba -> 1000deb4 between samples. 0x1000dexx is OUTSIDE the Amarchy code hunk
  (0x1010406c..0x1013f420), so the loop runs in other code in low fast RAM (a library or patch loaded early).
- Above the registers the stack holds a linear incrementing pattern (`000033e4 002133e4 102133e4 ...`) over about 24 KB,
  plus 1379 longs of 0x20000000. No Amarchy return addresses in that 24 KB.
- Amarchy return addresses only near the stack top: code offsets +30948, +30950 (x4), +30994 (x2), +309a4 (x2).
  The link map (vlink -M, relinked binary byte-identical to the build) puts them in `desk_contrast`
  (amarchy/desk_core.c:14, entry 0x30948, desk_luma at 0x30900). These are pure functions, so they are most likely
  stale frames from theme computation, not the loop itself.
- amarchy-flat task: state wait, stack 101e4630-101ec630.

## Open
- Name the owner of 0x1000deb4..0x1000deba (which library/patch) and the caller chain into it. Candidates in the
  ON path: keymap_reload_compact / cx_install after desk_dos_flat_sync (amarchy/amarchy.c:326-335), or a draw hook
  patched by amarchy-flat running on Amarchy's task (win_refresh_frames, amarchy/win_refresh.c:31, RefreshWindowFrame).
- A third taskdeep sample returned no output in 40 s (not explained).

## Part 2 (2026-11-01): who owns PC 0x1000deb4

Rig 10 (OS 3.2, 060), Amarchy md5 57c6fa0b, amarchy-flat md5 18d06679, started under Stack 262144. Writing
`flat-system-look = on` + `flat-window-gadgets = on` to ENV:Amarchy/prefs reproduces the hang: STATUS gets no
answer, Amarchy task ready (spinning), amarchy-flat waiting.

Tool: tests/amiga/addrowner.c (`addrowner ADDR [FIND]`, `addrowner STACK <task>`).

- 0x1000deb4 belongs to NO module: no resident module, no process seglist hunk, no DOS resident segment, no
  library/device/resource vector covers it. It is DATA: the boot-time DOS DeviceNode/FileSysStartupMsg block of
  the uae.device drive ("DOS\0" at de88 and df38, BSTR "uae.device" at df40).
- The bytes there decode by chance to a tight loop: `deb4: ori.b #1,d0 / deb8: move.b d1,d0 / deba: bne.s deb4`
  with d1=3, so it never exits. The CPU jumped into data (wild call), it is not a loop in any program.
- a6 = 0x1000df50 and 0x1000deb4 = a6 - 156. Consistent with `jsr -156(a6)` through a bogus base. 0x1000df50 is
  not in LibList, DeviceList or ResourceList.
- Live call chain (top of stack, mapped with the relinked map): startup -> main+0x432 -> tick_serve+0x40 ->
  tick_hooks_run+0x24 -> (a tick hook). Hooks: note_expire, bar_tick, tw_tick, wb_gate (amarchy.c:497-512).
  Stale frames deeper down are tw_tick's path: gt_dos_current -> gt_boot_term/gt_load_term -> gt_parse/gt_scale
  -> upconf_parse/upconf_get, plus bar_tick -> tw_text/tw_next.
- From sp+0x62 up ~28 KB the stack holds a generated pattern (longs 0x34340021 + 0x1000 each), not frames: some
  callee of the tick hook used a ~28 KB stack array and filled it.

Most likely owner: the tick hook tw_tick (amarchy/themewin.c:196-212). The flat switch counts as a theme change,
so tw_tick runs pens_release/pens_get and then `g_slot[i].fn(g_slot[i].win)` for each helper window. A garbage
`fn` or a library call with a stale/garbage base in a recolour callback lands in data. UNVERIFIED: which call.

Next probe: KPrintF (wasabi debug) brackets around each step of tw_tick and each g_slot[i].fn call, and around
bar_tick's sample/paint_all, then repeat the flat-ON write. Also print g_slot[i].fn and the library bases used
by the recolour callbacks before the call.

## Part 3 (2026-11-01): the wild call is amarchy-flat's own Text / RectFill fall-through, a6 = SysBase

- Cause: amarchy/flat.c called the stock graphics.library routines through their saved LVO pointers with C typedefs
  (`textfn`, `rectfn`) that had no `__reg("a6")` parameter. vbcc used a6 as scratch in flat_tx_c / flat_rf_c and
  called FindTask (a6 = SysBase) before the fall-through `((textfn)r->tsw.saved)(rp, str, count)`. ROM Text then ran
  with a6 = SysBase, read a garbage base and executed `jsr -156(a6)` into DOS DeviceNode data (the 0x1000deb4 spin).
  Disassembly of the old amarchy-flat confirmed no a6 load before the saved calls; the rebuilt one loads a6 before each.
- Why flat ON: the theme pick installs the Text patch; Amarchy's bar (bar_tick -> tw_text -> Text, every tick) is the
  first caller, so the commodity hangs right after the flat-ON prefs write. Same defect in flat_rf_c (RectFill after
  FindTask) and the dopus5 DrawBox/FieldBox saved calls (a6 must be the dopus5 base `dlib`); all fixed.
- The 28 KB "stack pattern" is `am_prefs` (char val[AM_NKEYS][UC_MAX_VALUE]) on the stack in gt_dos_current /
  amconf_load: not the cause.
- Fix: textfn, rectfn, boxfn, fieldfn take a trailing `__reg("a6") struct Library *`; saved calls pass GfxBase or dlib.
- Test: tests/test_flat_on_hang_wild_call.py (python3 -m unittest): OK with the fix, FAILED (failures=2) with the old flat.c.
- Rig 10 (OS 3.2, 060): amarchy_desk_rig.py --flat: flat ON pick -> flat=on, pens 2/3/0/3, glyph counter rose, Text and
  RectFill patches installed and called thousands of times, AMARCHY port answers. The second pick (flat OFF) timed out
  once at the menu step "Down to Style" (30 s, no stable state); a rerun of the OFF pick by itself passed: flat=off,
  Amarchy answers, amarchy-flat unloads (STATUS flat=absent). The one menu-step timeout is unexplained (unverified
  whether a slow redraw or rig load).
- Open: the input.device hang in rig10-thistask.md is plausibly the same bug (the patch runs on the caller's task);
  unverified.
