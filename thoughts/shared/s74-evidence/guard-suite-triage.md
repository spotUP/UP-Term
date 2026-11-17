---
date: 2026-11-12
topic: TEST-HIDDEN-FAILURES guard-suite triage (wb_lock_guard, ibase_lock_guard, flat_on_hang_wild_call)
status: read-only triage, no fix applied
---

# Guard-suite triage (s74)

Each suite was run alone (`nice -n 19 python3 tests/<file>`) on the working tree. None of the flagged source files
(amarchy/dopustb_dos.c, amarchy/flat.c, amarchy/strut_patch.c, amarchy/demo.c) is modified in the working tree
(`git status --short`), so every failure below is in committed (HEAD) code. The dirty amarchy/amarchy.c is not flagged by
any of the three. No FOREIGN EDIT verdicts.

Dating: every flagged line was committed after the guard it breaks (blame dates below vs guard last change), so the
guards caught new code, they did not fall behind old code, except for the flat case where the rule itself is too broad.

## tests/test_wb_lock_guard.py (guard last changed 1a515dbe, 2026-11-06)

Rule: no amarchy/*.c or handler/*.c except handler/pubscr.c may call `LockPubScreen(` with any argument (NULL included);
a bare call on a closed Workbench, or one listed but still private (LoadWB opening it), makes Intuition reopen it and can
deadlock against IPrefs closing it for the boot RTG switch (BOOT-31-WB-DEADLOCK, S57-FREEZE-2IN9). All three binaries
below already link handler/pubscr.c (Makefile AMARCHY_SRC / AMARCHY_MENU_SRC / AMARCHY_GUI_SRC, amarchy-flat rule), so
the helper is available everywhere.

### 1. amarchy/dopustb_dos.c:47 and :49 (cc4641a77, 2026-11-08) - REAL FAULT (low)
dtd_screen() locks the screen name DOpus reports, then falls back to `LockPubScreen(0)`. Line 47 is fairly safe in
practice (DOpus answered, so its screen is up) but line 49 hits the default public screen, which is Workbench; if
Workbench is closed or still private (boot, LoadWB in flight, or the user closed WB with DOpus on its own screen and the
name lookup failed) this reopens Workbench / takes the S57 freeze path. Trigger: an Amarchy menu/gui/daemon build that
queries DOpus toolbar metrics while DOpus runs.
Fix: line 47 `ipd_lock_pub_screen(name)`, line 49 `ipd_lock_default_pub_screen()`. Note dtd_screen shadows IntuitionBase
with a local OpenLibrary; the helpers use the program's global IntuitionBase, which every Amarchy program opens at start,
so the local open can stay for the font/gfx reads or be dropped.

### 2. amarchy/flat.c:2553 (e8b2ff929, 2026-11-11) - REAL FAULT (highest of the three)
cg_feed() runs inside amarchy-flat's CreateGadget patch, i.e. in the context of ANY task that creates a GadTools
CYCLE_KIND gadget while flat-system-look is enabled, and calls `LockPubScreen(NULL)` there. Any program opening a
GadTools window with a cycle gadget during boot (before/while LoadWB opens Workbench) or while Workbench is closed makes
Intuition reopen Workbench from inside a foreign task: same deadlock / freeze class as BOOT-31 and S57-FREEZE-2IN9.
Trigger: flat look on + any prefs/tool program with a cycle gadget (the comment names Time prefs) opened at boot or with
WB closed. Hang, not a crash.
Fix: `scr = ipd_lock_default_pub_screen()` (pubscr.c is linked into amarchy-flat), unlock unchanged.

### 3. amarchy/strut_patch.c:281 (e3e4151f7, 2026-11-09) - REAL FAULT (low)
inst_window() (OpenWindow patch, Installer windows only) locks "Workbench" bare when the NewWindow has WBENCHSCREEN type.
The stock OpenWindow that follows would open Workbench anyway, so the extra reopen risk is small, but the call runs
before the original and on a private (LoadWB-opening) Workbench node it takes the S57 path the helper exists to avoid.
Trigger: the Installer opening its window during a boot-time install script.
Fix: `scr = lock = ipd_lock_wb_screen()` (or `ipd_lock_pub_screen("Workbench")`); a 0 result already returns 0
(no resize, stock behaviour).

## tests/test_ibase_lock_guard.py (guard last changed fa3f00f6, 2026-11-05)

Rule: inside a LockIBase ... UnlockIBase span only list reads and the queue-only calls (ChangeWindowBox, WindowToFront,
WindowToBack, ZipWindow). ActivateWindow and ScreenToFront are explicitly forbidden; win.c's own header documents that
both run after UnlockIBase, and tile.c:455-463 does exactly that.

### 4. amarchy/demo.c:293 ActivateWindow() and :301 ScreenToFront() (ece7e6e28, 2026-11-07) - REAL FAULT (medium)
stop_demo() restore: the second lock span (291-303) calls ActivateWindow(g_snap.active) and ScreenToFront(g_snap.front)
while holding IBase. ScreenToFront goes through Intuition's depth arrangement, which needs the Intuition state lock on
the input task's side; a caller sleeping for it while holding IBase can deadlock Intuition (whole-machine input freeze).
Whether OS 3.1's ScreenToFront is synchronous enough to deadlock here was not verified on a rig (unverified; source read
only). Trigger: the Amarchy demo ending or being stopped, with the screen that was in front at demo start no longer in
front.
Fix: in the span compute `act = g_snap.active && member(g_snap.active)` and `front = there && g_snap.front !=
IntuitionBase->FirstScreen`, UnlockIBase, then call ActivateWindow / ScreenToFront (the tile.c pattern; the small
window-may-close race is the one tile.c already accepts).

## tests/test_flat_on_hang_wild_call.py (guard last changed 06257750, 2026-11-01)

Rule: every call through a saved LVO pointer (tsw, rsw, dsw) must pass the library base last, in a6, with a typedef
carrying `__reg("a6")` (s28 flat-ON hang: stock Text ran with a6 = SysBase).

### 5. flat.c:1333 `((fieldbodyfn)g_r->dsw[FLAT_EL_FIELDBOX].saved)(rp, rect, info, gfx)` (edf78743b/1eb68dd66, 2026-11-09)
### and flat.c:1347 bevelbodyfn call (same series; not reported because the assert loop stops at the first failure,
### it fails the same way) - STALE TEST
Both fail test_saved_lvo_typedefs_take_a6 and test_saved_lvo_calls_pass_the_library_base. Since 0c, dsw[el].saved is
not always an LVO: when flat_body_install succeeds it is the trampoline into a dopus5 internal BODY with stack
convention (flat.c:1322-1327 comment, install at flat.c:3305-3320). The guarded prologues confirm the bodies do not take
a6 as input: field `48 e7 20 32 41 ef 00 14` = movem.l d2/a2-a3/a6,-(sp); lea 20(sp),a0 and bevel `48 e7 38 32 ...`
save a6 and read their args (GfxBase is the 4th stack argument for FieldBox) from the stack frame
(amarchy/flat_core.c:197-199). So no wild call. The bevel has no LVO at all (d4_lvo[] = 0). The register-convention
paths (d4_drawbox :1313, d4_fieldbox :1319) still pass dlib in a6 and pass the test.
Fix (test only): exempt the stack-convention body typedefs, e.g. skip saved calls whose typedef has no `__reg(` at all
(stack convention) or keep an explicit allow-set {fieldbodyfn, bevelbodyfn} with a comment pointing at the body-hook
design, and keep the LVO rule for register-convention typedefs. Also make the loop collect all failures instead of
stopping at the first.

## Not checked
- No rig run: the deadlock consequences in 1-4 are from source reading and the guards' own evidence, not reproduced.
- Whether OS 3.1 ScreenToFront/ActivateWindow under LockIBase deadlocks in practice (vs. merely stalls).
- Other suites from TEST-HIDDEN-FAILURES.

## Fixes

Applied in worktree `.claude/worktrees/agent-a8be404bb209e2d9a`, branch `worktree-agent-a8be404bb209e2d9a` (from main 47b39de8):
source fixes 8d8942be, test fix 18ca108a. Not merged, not pushed.

Helper fit, checked before each swap: `ipd_lock_pub_screen` / `ipd_lock_default_pub_screen` (handler/pubscr.c:36, :54) return
a screen locked by LockPubScreen(name) or 0, so the existing `UnlockPubScreen(0/NULL, scr)` at each site stays correct.
`ipd_lock_wb_screen` (amarchy/iconpen_dos.c:119) is `ipd_lock_pub_screen("Workbench")`. Link check (Makefile): Amarchy =
AMARCHY_SRC line 1105 (pubscr.c + iconpen_dos.c), amarchy-menu = AMARCHY_MENU_SRC line 1171, amarchy-gui =
AMARCHY_GUI_SRC line 1185, amarchy-flat = link line 1213 (pubscr.c + iconpen_dos.c).

- flat.c cg_feed (was :2553): `LockPubScreen(NULL)` -> `ipd_lock_default_pub_screen()`; unlock unchanged; 0 keeps side = 1.
  Note: the helper puts a MAXPUBSCREENNAME+1 name buffer on the foreign task's stack inside the CreateGadget patch
  (cg_feed itself is outside the flat_cg_c entry frame); stack depth not measured.
- dopustb_dos.c dtd_screen (:47/:49): `ipd_lock_pub_screen(name)` and `ipd_lock_default_pub_screen()`; include
  ../handler/pubscr.h added. The local IntuitionBase shadow stays (used for UnlockPubScreen and CloseLibrary); the
  helpers use the global IntuitionBase, opened by amarchy.c, menu.c and gui.c at start.
- strut_patch.c inst_window (:281): `ipd_lock_wb_screen()`; a 0 result returns 0 (stock behaviour), as before.
- demo.c stop_demo (:291-303): `act` and `front` are computed under LockIBase, UnlockIBase, then ActivateWindow and
  ScreenToFront run (same order as before), the tile.c pattern.
- tests/test_flat_on_hang_wild_call.py: STACK_BODY_TYPEDEFS = {fieldbodyfn, bevelbodyfn} exempt from the a6 rules, with
  the reason in a comment; a new test fails if an exempt typedef is no longer called through dsw.saved; both a6 tests
  now collect every failure and report them together.

Suites (each alone, `nice -n 19 python3 tests/<file>`): test_wb_lock_guard OK (2), test_ibase_lock_guard OK (2),
test_flat_on_hang_wild_call OK (4).

Compile: `make build/amiga/Amarchy build/amiga/amarchy-menu build/amiga/amarchy-gui build/amiga/amarchy-flat` all built
(vc -warnings-as-errors). amarchy-gui needed build/amiga/menu_icons.inc, whose generator needs fontTools (missing in this
python); the file was copied from the main checkout's build/amiga (generated 2026-11-08), not regenerated. Build logs:
worktree build/guard-fix-build.log, build/guard-fix-build2.log. Worktree build dir 7 MB, nothing over 10 MB.

Not proven: nothing ran on an Amiga or a rig. That the guarded locks avoid the freeze at boot, that the Time prefs cycle
gadget offset still comes out right with flat look on, that the demo end still activates and fronts correctly, and the
cg_feed stack headroom are all unverified.

## Rig check

Rig 9 (default 060 machine, KS 3.1, purpose "up-term-ba s74: guard fix regression check"), started from the worktree:
`rig.py start` refreshed Amarchy 400080, amarchy-flat 242164, amarchy-menu 686432, amarchy-gui 569560 bytes (the
worktree builds, sizes equal); Status showed the running Amarchy, amarchy-flat, amarchy-menu, amarchy-gui from UP-Term:bin
/ Rushhours:Tools/UP-Term/bin. Rig stopped and its folder removed at the end. No screenshot taken.

1. Time row: PASS. amarchy-flat STATUS before: `flat=on`. After `Run SYS:Prefs/Time`: `time row installed=yes calls=18
   changed=1 reason=ok integer=54,18,53,14,14 cycle=142,18,178,14,15`. gadprobe (main checkout build/amiga/gadprobe,
   `gadprobe Time ALL`): year INTEGER id 14 `box=12,15,29,8` hex = 18,21 41x8 (stock 60,35 41x8 in the ledger: moved
   left 42 to the frame box 12, width 41 kept); month CYCLE id 15 `box=41,12,ff,e` hex = 65,18 255x14 (right edge 319
   unchanged), exactly the measured 39.1 result "month 142 w178 -> 65 w255" of flat-time-row-core.md; calendar left 12.
2. No hang: PASS. Time closed by its Cancel gadget (gclick id 9; window gone from uitree), then `Echo shell-alive-1`
   answered. Break C to amarchy-flat: STATUS `flat=absent`, `Echo shell-alive-2` answered; restarted: STATUS `flat=on`,
   `time row installed=yes`, `Echo shell-alive-3` answered. Time was not reopened after the reload.
3. Amarchy demo: start/stop PASS, active-window restore PARTLY CHECKED. `amarchy_demo_rig.py --fresh`: passed 5 of 5
   (tour reached scene 3, status running=1, a key stopped it: DONE, running=0, why 1064; every window box equal to
   the pre-demo box). After the stop, uitree: `W 1 0 20 600x200 active "demohost"` (the window the script opened before
   the tour) and `Echo shell-alive-4` answered. Not proven: the script does not record which window was active just
   before the start, so "the previously active window is active again" is inferred, not compared. A second start by
   the raw ARexx `DEMO START` verb (not the CLI form the script uses) returned running=0 after 20 s (not investigated);
   `DEMO STOP` then and `Echo shell-alive-5` answered, active window unchanged (demohost).

Not tested: boot with the fixed binaries from a cold startup-sequence beyond the one rig boot (they did boot and run),
dtd_screen (DOpus not installed on this rig: [WARN] no dopus5.stamp, though STATUS showed dopus=68.16 patched=3),
strut_patch inst_window path explicitly, the cg_feed stack headroom (no `wasabi ps` taken), OS 3.2, PAL hires (side 2).
