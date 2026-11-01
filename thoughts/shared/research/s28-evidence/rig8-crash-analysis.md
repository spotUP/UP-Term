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
