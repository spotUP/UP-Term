---
date: 2026-11-11
session: s72
topic: DCTelnet RMB menu shifted left / garbage on 640x256x4 screen (rig 3)
status: evidence, read-only investigation
---

# DCTelnet menu hunt

## agent 14: menu path patches and fresh rig differences

Read-only. No rig commands, no builds, no edits to source. Items not checked in
source are marked UNVERIFIED.

### Q1. What patches the Intuition menu path or screen bar

amarchy-flat (`amarchy/flat.c`), jump-table swap via `flat_swap` + `libpatch_done`
(`amarchy/libpatch.h`), no SetFunction. Vectors listed in `amarchy/flat_core.h:145-148`:

- graphics Text (LVO -60): `tx_install` flat.c:1774, handler `flat_tx_c` flat.c:1749-1772.
- graphics RectFill (LVO -306): `rf_install` flat.c:2381-2405, handler `flat_rf_c`
  flat.c:2341-2379. This one touches menus directly:
  - `ghost_menu` flat.c:1987-2009 recolours Intuition ghosted menu items and the
    separator in place (rp->Layer NULL, off-screen menu bitmap) via `flat_gm_recolour`.
  - `ghost_title` flat.c:1961-1985 redraws a disabled screen title through a scratch
    bitmap (AllocBitMap at rp bitmap depth, ClipBlit / BltBitMapRastPort).
  - Pen comes from `flat_gm_pen(&r->st, depth, ...)` with r->st = Workbench pens
    (comment flat.c:335). On a non-Workbench 4-plane screen those pen numbers index
    DCTelnet's 16-colour palette, not Workbench's.
  - Gate: `on && rp->AreaPtSz && rom && !w` (flat.c:2353), planar bitmaps only.
  - `rf_gbox` (called flat.c:2373, defined flat.c:2322) is gated only by `on`, not by
    ROM caller.
- dopus5 DrawBox / DrawFieldBox (flat.c:1305, 3043), BOOPSI dispatcher replacement
  c2_disp / c8_disp (flat.c:2852, 2886). Not on the menu path.
- Screen-bar depth gadget FLAT-C7 `c7_screen_which` flat.c:142-162 (only screens in the
  aspect slot table); `menu_screen_aspect` used flat.c:1944.

Amarchy commodity (`amarchy/amarchy.c`):

- AM-BORDERBLANK: `borderblank_install()` amarchy.c:121 (via `borderblank_follow`, prefs
  AM_BORDERBLANK, default on). Patches intuition OpenScreen / OpenScreenTagList
  (`amarchy/borderblank_patch.c:154,159`, ipatch). Every native-mode screen
  (`bb_native_mode`, borderblank_core.c:4-9, modeid & 0xFFFF1000 <= 0x000A1000) gets
  VideoControl VTAG_BORDERBLANK_SET + MakeScreen + RethinkDisplay after it opens;
  `blank_open_screens` does the same to every open native screen at install.
  DCTelnet's screen is PAL:HighRes 0x00029000, so it is a target. Added 9b05c48f
  (2026-11-01).
- strut patch OpenWindow / OpenWindowTagList (strut_patch.c:334,337; installed
  amarchy.c:704, removed amarchy.c:768).
- WBRESET-EXT CloseWorkBench -78 / OpenWorkBench -210 (`amarchy/wbreset_patch.c`).
- Commodity filters (`amarchy/cx.c`): LCOMMAND rawkey (cx.c:97-115); RMB filter `rgate`
  (cx.c:117-135, gate cx.c:58-69) swallows only when `bar_takes_right_press()`
  (`amarchy/bar.c:677-688`) is true: front screen is the bar screen and pointer is on the
  agents segment. On DCTelnet's screen the press passes on. Watch filters cx.c:236-273
  only record.

amarchy-menu, amarchy-gui: grep finds no LVO / SetFunction patches; own windows only
(gui.c:1376 MENUDOWN in its own window; win.c:229 ShowTitle). UNVERIFIED beyond grep.

SolidWin 1.12 (binary only, `dist/solidwin/SOURCE.txt`): input.device handler for
opaque window moves. Effect on menus UNVERIFIED (no source).

Rig startup: `tools/rig/user_startup.py` blocks rig-upterm-shadows, rig-wasabid,
rig-solidwin; amarchy-flat starts from Startup-Sequence before IPrefs, Amarchy /
amarchy-index from WBStartup (`tools/rig/rig.py:346-358`).

### Q2. Removal and flag-off behaviour

- flat RectFill: `on = r->st.enabled && !r->st.held && rp` (flat.c:2348); off means
  stock RectFill. flat Text: `flat_gh_text` runs whether on or off; `r->tx.armed &&
  tx_armed` can SetAPen while off (pen restored after). Otherwise stock Text.
- `rf_remove` flat.c:2407-2425: if `flat_swap_remove` does not return FLAT_REMOVED
  (another patch on top), the vector stays and the handler passes through. Reported
  via `stuck_off` flat.c:2534-2548. So "off" can be true while the vector is still
  ours; the code path is then stock, except the Text pen detail above.
- borderblank: removed by `borderblank_remove` (ipatch_passthru, then
  `clear_recorded` clears the blank on screens it recorded). It follows the Amarchy
  commodity's lifetime and the AM_BORDERBLANK pref, NOT the amarchy-flat switch.
  "Amarchy flat off" leaves border blank active on DCTelnet's screen.

### Q3. Fresh rig vs older rig folder

- Every rig start clones a fresh sys.hdf (`fresh_sys_hdf`, rig.py:816-835). ENV /
  ENVARC on DH0 are fresh on every start either way; deleting build/rig3 does not
  change them.
- Host-side state lost with the folder: `build/rig3/screenmode.name` (rig.py:1244),
  re-applied each start by `sys_hdf_screenmode` (rig.py:1279-1289). Now absent, so the
  default is rtg (rig.py:1312, 1318-1325): Workbench uaegfx 800x600x24, FS-UAE window
  800x600 with no `zoom` line (current `build/rig3/vtcon-rig.fs-uae` confirms). aga /
  aga256 add `zoom = 640x512` and window 1280x1024. Old rig 3 value UNVERIFIED (no
  record; s69 bisect rig 68 is rtg too).
- Rig 3 config: A1200, 68060-NOMMU, uaegfx, JIT, Kickstart 40.068 (3.1). Amarchy,
  amarchy-flat (235528 bytes), amarchy-gui, amarchy-menu dated 11 Nov 09:03.
- What can shift / corrupt a menu on 640x256x4 native next to an RTG Workbench:
  border blank (VideoControl + MakeScreen + RethinkDisplay on the native viewport),
  FS-UAE native display without zoom (auto crop / centre changes as the menu layer
  appears), and flat's RectFill recolour using Workbench pens on DCTelnet's bitmap.

### Q4. DCTelnet.Prefs

`build/rig3/vtc/dct/DCTelnet.Prefs` (460 bytes): modeid 0x00029000 (PAL:HighRes),
640x256, depth 4, font topaz.font 8, VT102, xemvt340.library, xprzmodem.library.

Archive comparison (`~/Downloads/DCTelnet-PETSCII/`):
- `DCTelnet/DCTelnet.Prefs` (444 bytes): mode / size / depth fields all zero (no
  screen mode chosen: factory state).
- `DCTelnet-v2/DCTelnet.Prefs` (444 bytes): modeid 0x00029000, 640x256, depth 8,
  font "Term...".
- Rig 3 file differs in size (460 vs 444) and in 248 bytes from the factory one. So
  rig 3 prefs are NOT factory defaults: a native PAL:HighRes mode was saved, with
  depth 4 and topaz 8. The owner's known-good v2 prefs use depth 8 on the same mode.
  `~/Downloads/DCTelnet-release/` holds only an lha (not unpacked).

### Q5. Top 3 causes, cheapest experiment each

1. AM-BORDERBLANK on DCTelnet's native screen (new since 2026-11-01, independent of
   the flat switch). Experiment: quit the Amarchy commodity (or set AM_BORDERBLANK off),
   close and reopen the DCTelnet screen, hold RMB.
2. Rig 3 lost `screenmode.name`, so Workbench is RTG and FS-UAE has no `zoom`; the
   native 640x256 screen is shown by FS-UAE auto crop. Experiment: `rig.py aga` (or
   aga256) on rig 3, then DCTelnet, hold RMB. Alternative: pick a uaegfx mode in
   DCTelnet prefs.
3. amarchy-flat RectFill / Text patch drawing with Workbench pens on a 4-plane menu
   bitmap (`ghost_menu`, `rf_gbox` not ROM-gated). Experiment: Break amarchy-flat,
   confirm vector restored (flat STATUS, no `stuck_off`), hold RMB. Also try DCTelnet
   with depth 8 (v2 prefs) to see if the colour loss follows depth.
