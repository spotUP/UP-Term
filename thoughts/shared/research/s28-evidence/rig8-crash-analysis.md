# s28 rig 8 flat-on hang/crash: code analysis (read-only, no rig)

Question: which s28-crash-hunt change makes flat-on hang or crash, and why?
Scope read: diff origin/main...HEAD of amarchy/flat.c, flat_core.[ch], flat_glyph.c, flat_shared.h, desk_dos.c, desk_core.c (wbborder).
Note: `Stack 262144` (run 2) sizes Amarchy's own stack only. The flat patches run on the caller's stack
(input.device, Workbench, app tasks), so run 2 does not rule out a stack overflow inside a patch.

## Ranked suspects

1. cd832495 FLAT-BUTTON-DISABLED-DIM, amarchy/flat.c:865-884 dim_refresh (called at 1000/1011 on every CTRL_D).
   amarchy-flat calls ObtainBestPen for g_dim_pen on the Workbench ColorMap at start (also with flat off) and keeps it.
   Flat-on rewrites Palette.prefs, and IPrefs then closes and reopens the Workbench screen with a new ColorMap.
   dk_flat_sync (desk_dos.c:638) sends CTRL_D, and dim_refresh calls ReleasePen(NEW cm, pen obtained on the OLD cm).
   That decrements a pen the process never got from that map (one owned by Workbench, an icon or a NewIcons remap),
   and the pen can go back on the free list while someone still owns it. That corrupts the PalExtra share counts and
   free list, so a later ObtainPen/ObtainBestPen or ReleasePen (Workbench icon remap, our next dim_refresh) can spin
   or corrupt memory. That fits a 100% CPU guest with no Guru. Main does not have this code.
   Probe: start amarchy-flat with flat off. Run `SYS:Prefs/Palette FROM ENV:Sys/Palette.prefs USE` (Workbench reset,
   no flat change). Then `Break <amarchy-flat CLI> D` and open a drawer with colour icons. A hang or garbled pens
   means YES. As a control, do the same without the Break D. It stays alive.

2. a4ef1fc9/0d5b6763 + 50dd81ad/53c962cc, amarchy/desk_dos.c:562-578 and 621 (dk_flat_pens "own" path + dk_wbborder).
   Flat-on now fires three prefs editors back to back: Palette (it waits for the reset and IPrefs idle), then WBPattern
   (570) and Workbench/WBNC border (621) with NO dk_wait_iprefs. Compare dk_apply's WBPattern put at 393-395, which waits.
   IPrefs and workbench.library then handle a backfill change and a full icon re-layout while the screen from the
   Palette reset is still reopening. This is the DESK-31 class of race (a window opening inside a reset).
   Probe: turn flat on with amarchy-flat NOT running (only Amarchy). A hang means YES (desk path, independent of
   patches). No hang moves the cause to the patches (suspects 1/3).

3. cd832495 ghost replay in the RectFill patch, amarchy/flat.c:381-404 + flat_core.c:403-440 (flat_gh_text/match).
   No lock guards the ring, and every task writes to it (frame hook and Text patch). Records are keyed on Layer
   pointer + task, and they survive the Workbench reset, so a freed layer address gets reused. gb->str is any Text
   caller's pointer, possibly a stack buffer, and it is replayed later through tsw.saved from INSIDE the RectFill hook
   on input.device's small stack. A nested Text plus the 272e6143 corner arrays (flat_glyph.c draw_round, about 240
   bytes, plus a ReadPixel call) add stack depth in the patched path. Overflow or a bad return address fits run 1's
   "Illegal instruction 7fff at 1000DDBC" (a jump into data in Fast RAM).
   Probe: rebuild with flat_gh_match returning 0 (one line) and repeat the run 2 sequence on rig 8 3.2.
   No hang/Guru means YES. Cheaper: read `amarchy-flat` status ghits just before flat-on. ghits > 0 makes it live.

Not suspect: flatshared layout change (gh inserted before tcalls). Only amarchy-flat allocates and reads it
(FindSemaphore), and the asm stubs flat_rf.s/flat_tx.s use no struct offsets. Off-path dk_wbborder writes nothing
with no Workbench.prefs (desk_wbborder returns 0), so there is no notify loop.
Order to run: probe 2 first (splits desk path vs patches in one run), then 1, then 3.

## Which process owns the theme-pick crash (static read, 2026-11-01)

Question: which process, started or driven by the theme-pick flat path, owns the wild PC at 1000DEB4 with USP=10003D80, and why is its stack wrong?

Answer (unverified, needs the probe below): **not a process Amarchy launches. The suspect is a system task that runs amarchy-flat's patches on its own small stack during the Workbench reset the theme pick causes: IPrefs (first choice) or input.device.** The stack overflows below its tc_SPLower into the boot-time allocations at 10002000-10004000, then an RTS pops text as a return address.

What the theme pick does that `put_flat_prefs(True)` alone does not:
- `tools/rig/amarchy_desk_rig.py` with no `--flat` runs `main()`. It writes `desktop-theme = on`, then `pick()` drives Style > Amarchy theme (lines 197-228, 364-394). `--flat` / `put_flat_prefs` only writes ENV:Amarchy/prefs, so Amarchy's reload reaches `dk_flat_sync` and nothing more.
- The pick reaches `gthemes_dos.c:224 desk_dos_apply` -> `dk_apply` (desk_dos.c:336-435). With the palette bit set this runs `SYS:Prefs/Palette FROM ... USE` (`dk_put` -> `dk_editor`, desk_dos.c:91-104). IPrefs then resets the Workbench screen (`dk_wait_reset`), and WBPattern, Pointer and Font follow. The flat-pens path (`dk_flat_pens`, desk_dos.c:498+) and `dk_wbborder` run Palette, WBPattern and Workbench USE in the same way.
- A screen reset redraws every screen title-bar gadget and window frame. The sysiclass/frameiclass hook and the RectFill and Text patches are installed process-wide, so `c8_skip` / `flat_rf_c` / `flat_tx_c` run on **whichever task Intuition draws from**: IPrefs (Close/OpenWorkbench), input.device (depth, refresh) and Workbench (window reopen). They do not run on amarchy-flat's 16384 stack.

Stack each path gets:
| process | started by | stack |
|---|---|---|
| Amarchy | rig `Stack 262144` + `Run` | 262144 |
| amarchy-flat BOOT | desk_dos.c:456 `pl_start(..., 32768, ...)`, cookie 16384 (flat.c:61-62) | 32768 [OK] |
| SYS:Prefs/Palette, WBPattern, Pointer, Font, Workbench | desk_dos.c:100 `SystemTags` sync, **no NP_StackSize** | the caller's CLI default (262144 under the rig shell; 4096 when Amarchy has no CLI) |
| IPrefs | startup-sequence, loaded at boot | the boot default, a few KB (unverified) |
| input.device | ROM, at boot | small, about 4 KB (unverified) |

Stack the patches add on a foreign task, per frame draw (flat.c:188-232):
- `role_of` (flat.c:171-173): `flat_box sib[32]` is 512 B, and it is called **twice** per filled frame (lines 221 and 224).
- `flat_frame_draw_round` (flat_glyph.c:225-228): `cut[12][4] + pen[12]` is 240 B. It is new in 272e6143 (round corners).
- `flat_state st` copy: 24 B. Then `c2_fill` -> RectFill, which is patched, so `flat_rf_c` nests.
- The disabled-dim ghost replay (cd832495, flat.c:383-402) calls SetDrMd/SetAPen/Move, the stock RectFill and then the **stock Text**, all from inside the RectFill patch, on the drawing task's stack.
- Peak is roughly 0.9-1.2 KB above Intuition's own deep boopsi chain, on a 4 KB-class stack. Before e99080cb (role_of) and 272e6143 the patches added under 100 B.

Why the registers fit this picture:
- USP=10003D80 and ISP=10002318 are both in the lowest boot-time allocations. ROM tasks and the first startup-sequence loads (IPrefs is one of the first) get their stacks there. A program's or Amarchy's stack would sit far higher.
- The text at USP (a gai_strerror table: "Request canceled ... system error / Unknown error code", next to "Invalid argument") is **in none of build/amiga/***. It is in os-322.hdf at byte 4537650, a system TCP/SSL component loaded early. So SP walked out of a small low stack into a neighbouring early hunk, which may have been freed. It did not walk into an Amarchy hunk.
- No Guru: an overflow corrupts memory silently. The RTS pops a text longword, and the CPU then runs nearby heap (the DOS boot-node block) in a loop with D1=3. That matches both runs (1000DDBC Illegal 7fff, 1000DEB4 loop).

Ruled out by reading: amarchy-flat's own stack (16384, it only Waits); `pl_start` floors (Amarchy launchers honour the cookies); the flatshared layout (see above).

Second, smaller defect found on the way: `dk_editor` (desk_dos.c:100) gives the prefs editors no NP_StackSize. When Amarchy runs without a large CLI stack (WBStartup), they get 4096. That breaks the rule-6 floor. It is not this crash (the rig shell had 262144).

### Decisive probe (one rig run)
1. Debugger rig 8 (3.2, `--max --debugger`), Amarchy, flat and menu as before. Put build/amiga/taskdump in RAM:.
2. **Before** the pick: `RAM:taskdump` with no names. Write down `stack lower-upper` and `sp` for IPrefs, input.device, Workbench and « Amarchy ».
3. Run `tools/rig/amarchy_desk_rig.py flat`. At the hang, read USP/ISP and `ThisTask` (ExecBase+0x114, read as a longword at an even address) and that task's `tc_SPLower/tc_SPUpper`.
4. YES (patch stack overflow on a system task) when USP 10003D80 lies just **below** tc_SPLower of IPrefs or input.device and ThisTask is that task. Then rerun with amarchy-flat held (prefs with flat off, or a held marker) and the same pick: no hang confirms it.
   NO when ThisTask's stack range does not touch 10003D80, or the hang persists with amarchy-flat held. Then the next suspect is the IPrefs reset itself on 3.2, independent of the patches.
Fix direction if YES (not done here): the patches must not take stack on a foreign task. Make `sib[]`/`cut[]` static under the semaphore, or one shared buffer in flatshared, and compute `role_of` once per draw. Also give `dk_editor` `NP_StackSize`, with `pl_stack` floor 32768.
