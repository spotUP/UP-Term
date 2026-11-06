---
date: 2026-11-03
author: vtcon desk hunt agent (opus, read-only, no rig)
topic: AMARCHY-31-LIVE-COLOURS - themed Workbench colours on AmigaOS 3.1 without the palette.prefs freeze
status: design proposed, not implemented, not rig-proven
---

# Themed colours on 3.1 (Kickstart 40.68, IPrefs 40.7, Palette 40.2)

## 1. What froze on 3.1

Two separate things were filed under DESK-31-CRASH (thoughts/spot/todos.md:3171, :3264, :3276):

a. **The palette freeze (still the reason for the gate).** FLAT-31-PALETTE-HANG verdict, rig 5, 2026-10-30 (todos.md:3264):
   "On --os 31 (IPrefs 40.7) ANY write of ENV:Sys/palette.prefs freezes the guest: Palette USE with flat file or stock file,
   with Stack 262144, and a bare Copy to ENV:Sys/palette.prefs without the editor; screenmode.prefs touch is fine. No Guru,
   no debug text." The trigger is the FILE WRITE that IPrefs is notified about, not the editor, not our data (stock bytes
   freeze too), not SetPrefs, not LoadRGB32. The desk rig's own `Palette FROM stock USE` (live_pens_differ) fired it in the
   first DESK-31-CRASH run. **What IPrefs 40.7 does inside after the notify was never pinned** (no Guru, no snoop trace kept);
   whether a real A1200 3.1 does the same is unverified (todos.md:3264).
   Gate: amarchy/desk_core.c:1187-1190 `desk_palette_editor_ok` (>= 47), used through amarchy/desk_dos.c:203-208
   `dk_palette_editor_ok` in dk_flat_pens (:689), desk_dos_flat_pens_refresh (:1036), desk_dos_start_apply (:1424, :1443)
   and desk_gate_mask (desk_core.c:1192-1199, palette + icontrol bits).

b. **The second loss (fixed, not a palette issue).** Pre-47 wbpattern.prefs write makes Intuition post a "reset the
   Workbench screen" System Request while our windows are open; we reopened during IPrefs' second Font reset. Fixed in
   19da2833 (desk_iprefs_idle, wait for IPrefs idle). Rig 7 desk case PASS 3.1 x3 + 3.2 x1 (todos.md:3171).

## 2. Path to themed colours on 3.1: live SetRGB32, never palette.prefs

The freeze needs a palette.prefs write; SetRGB32 on the Workbench ViewPort does not touch ENV:. Amarchy already pokes the
Workbench ViewPort live in four places:

- amarchy/desk_dos.c:592-620 `dk_live_pens` - role pens via `desk_pen_colour` (desk_core.c:329), only the pens no palette
  entry reaches (`desk_live_role_pens`, desk_core.c:307: 4-7 on depth > 3; ZERO pens at depth <= 3).
- amarchy/iconpen_core.c:224-228 `ip_sync` -> `desk_live_follow` (desk_core.c:336) - re-pokes those pens on every new
  Workbench screen (reopen after Font / WBPattern resets). Same pen set, so also zero at depth 3.
- amarchy/gthemes_dos.c:150-175 `desk_preview_theme` -> `desk_preview` (desk_core.c:1207): pens 0..7 = desk_role(t, i),
  SetRGB32 (gthemes_dos.c:142-147). Rig 7 PASS 9e04c45d (todos.md:3241). Rig 7 was the default 3.1 machine for the
  DESK-31-CRASH runs the same day, so this is probably already a 3.1 SetRGB32-on-WB-pens proof without a freeze -
  UNVERIFIED (the log does not name the OS).
- amarchy/gthemes_dos.c:36-60 `wb_pens_apply` - pens 0-3 bg/fg/dim/accent; runs on 3.1 today after a PICK
  (gthemes_dos.c:226-230, because dk_flat_pens returns 0 on 3.1), but its order does not match the flat pen map
  (desk_dos.c:673 comment), it misses pen 4 (button) and pen 7 (backdrop), it never runs at boot (desk_dos_start_apply has
  no fallback), and a Workbench reopen wipes it (follow covers 0 pens at depth 3).

**Design (one source, reuse dk_live_pens + desk_live_follow):**

1. desk_core: give the live role-pen set a "palette not writable" mode. `desk_live_role_pens(depth, out, writable)`:
   writable = 1 keeps today's set; writable = 0 returns every role pen 0..DK_ROLES-1 below 2^depth (and at depth > 3 also
   the screen pens `desk_palette_pen(4..7, depth)` the WBPattern tile maps to, colour of entry e = desk_pen_colour(e)).
   Arrays `int pens[4]` / `uc_u32 rgb[4]` grow to 8 (+4 for depth > 3): desk_core.h:110,122, desk_dos.c:597,
   iconpen_core.c:218-219. `desk_live_follow` takes the same flag. Host test: depth 3, writable 0 -> pens 0..7, pen 3 =
   GR_ACCENT, pen 7 = gt_darker; writable 1 -> 0 pens (unchanged).
2. desk_dos: export `dk_palette_editor_ok` (desk_dos.c:203) as the single "palette writable" answer; `dk_live_pens` passes it.
   desk_dos_start_apply: when `!dk_palette_editor_ok()` and (flat or DT_PALETTE in mask), set `dk_live_want = 1;
   dk_live_flat = flat;` so the existing tail (desk_dos.c:1525-1526) pokes after `dk_windows_open` / `dk_wait_iprefs`
   (i.e. after the Font + WBPattern resets the start plan itself causes). desk_dos_apply: same when dk_flat_pens returned 0
   because of the gate; then gt_dos_select must NOT also run wb_pens_apply (return the DT_PALETTE bit, as desk_dos.c:1053-1054
   does for 3.2), so one writer owns pens 0-7.
3. iconpen: the `o->theme` callback (iconpen_core.h:46) also returns the writable flag (amarchy-icons links desk_dos? if not,
   read the SYS:Prefs/Palette version through the same desk_palette_editor_ok on the file bytes), so every Workbench reopen
   re-pokes all 8 pens on 3.1. Without this the colours are lost on the first Font / pattern reset.
4. Restore (flat off / theme part off): `dk_live_pens(0, ...)` already uses `desk_live_baseline` from the saved
   palette.prefs; on 3.1 no file was written, so baseline = ENV/ENVARC palette.prefs entries = stock: correct.
5. After the poke, `dk_flat_sync()` (desk_dos.c:535) so amarchy-flat recomputes its held dim/inbar pens from the new pen
   0/2 colours (stale held pens is the open S32-INACTIVE-BAR suspicion, inactive-bar-colour.md).

**Gates, each:**

- Palette (desk_core.c:1187-1190, desk_gate_mask palette bit): the FILE write must stay gated on 3.1. Open the COLOURS by the
  live path above, not the gate.
- IControl (desk_gate_mask, desk_dos.c:1443): stays. FLAT-R1: 3.1 accepts the file and changes nothing (ic_GUIGeometry is
  3.2-only); it only buys a Workbench reset. Nothing to gain.
- Pointer (desk_core.c:1182-1185, desk_dos.c:194): stays gated for the pointer.prefs write (Pointer 40.x editor has no
  CLI USE path we verified; not part of this task). The pointer COLOURS are sprite pens 17-19 (+16..31): could be poked live
  with SetRGB32 the same way if wanted; separate row.
- Workbench border (desk_dos.c:757 WBNC): stays. 3.1 workbench.library has no borderless option; no file can do it.

## 3. Backdrop pattern and flat frame pens on 3.1

- Backdrop: the start apply already writes wbpattern.prefs on 3.1 (PLAN 18 includes DESK_START_PATTERN,
  fresh-stock-look.md), tile pen 7 (desk_backdrop_pen_at, desk_core.c:409; FLAT-COLOUR-OVERHAUL). At depth 3 that is screen
  pen 7, stock 3.1 colour EE9900 - the orange seen. Live pen 7 = `desk_pen_colour(t, 7, flat)` = gt_darker(t) makes it
  themed. Nothing else needed. (s27: on 3.1 ROOT pen 1 landed but pixels 0,0,0 = stock pen 1 black; same cause.)
- Flat frames: amarchy-flat draws with fixed pens text=2 line=3 back=0 outline=3 button=4 (desk_flat_glyph_pens,
  desk_core.c; STATUS in fresh-stock-look.md). Pen 3 = DK_ACCENT, pen 4 = DK_SELECTION: live pens 3 and 4 make them themed.
- RISK, not reachable this way: the Workbench DrawInfo pen MAP (palette.prefs PALT pen tables, desk_pens_flat:
  shine = shadow = fill = surface) also only comes from palette.prefs. On 3.1 the stock map stays: TEXTPEN/SHADOWPEN = 1
  (-> MUTED, window text grey-ish), SHINEPEN = 2 (-> FG, a bright highlight line on non-hooked 3D frames), FILLPEN = 3
  (-> ACCENT active title fill). So 3.1 gets the theme COLOURS but not the full flat SOLID-FILL look where DrawInfo drives the
  drawing (title bars, GadTools bevels not hooked by amarchy-flat). flat.c decides via dri_Pens[SHINEPEN]/[BACKGROUNDPEN]
  (flat.c:416, :477), which on 3.1 are 2/0, distinct: check the frame hook still draws flat with that pair. Owner decision
  whether "colours only" on 3.1 is enough; the full map needs the Workbench to reopen with new pens, which only IPrefs does.
- RISK: whether the WB screen palette on 3.1 is reloaded by IPrefs on events other than a palette.prefs notify (e.g. its
  own reset after Font) beyond the screen reopen ip_sync already follows. The proof below measures after a Font reset.

## 4. Smallest rig proof (one 3.1 rig)

1. Build with the change; `UPTERM_RIG=<free> rig.py start --max --purpose "<session>: 3.1 live colours"` (no --os: 3.1).
   Check `Version` = Kickstart 40.68 first (the RIG-START-OS-FLAGS lesson).
2. Fresh-all install (installer_rig.py ONLY fresh-all, INSTALLER_KEEP=1), reboot, wait for Amarchy start + 10 s.
3. Text checks: `List ENV:Sys` shows NO palette.prefs (gate held); wasabid still answers 60 s later (no freeze);
   sentinel AMARCHY_DESK_LIVE_PENS (new, = desk_dos_live_pens) >= 1; dripens (tools/rig/tests/amiga/dripens) pens 0..7 =
   desk_pen_colour of the theme (tokyo-night: pen0 1A1B26, pen2 A9B1D6, pen7 gt_darker).
4. Pixels (screenshot, haiku/PIL): backdrop area pixel (e.g. 400,300 on an empty desk) == gt_darker(theme), EE9900 count 0
   (fresh-stock-look baseline 337815); a flat window outline pixel == theme accent, 6688BB count 0.
5. Reopen check: run SYS:Prefs/Font USE (or a theme pick that writes font) -> Workbench reopens -> repeat step 3/4
   (ip_sync follow). Fails today by design -> regression evidence.
6. `rig.py stop` + clear.
Stub-fail: writable forced to 1 in desk_live_role_pens -> step 3/4 show stock colours.
