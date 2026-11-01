---
date: 2026-10-31
topic: AfA OS 4.8 as a base for Amarchy rounded window corners
status: research done (no rig run)
source: https://aminet.net/package/misc/os/AfAOS4.8 (AfAOS4.8.lha, 2,265,670 bytes; readme)
---

# AfA OS 4.8 vs rounded corners

## Verdict

AfA OS does not help. It ships NO layers.library and no layers module of any kind. Every AfA binary
opens the ROM `layers.library` and uses only classic LVOs. AfA's intuition_lib.exe has AROS
`ChangeWindowShape` / `HideWindow` / `ShowWindow` compiled in, but they are dead code: they are never
installed with SetFunction. ChangeWindowShape calls a layers LVO (-246) that no layers AfA runs
against provides. The 6-10 week estimate in round-corners-spike.md stands. AfA saves nothing on the
hard part (smart-refresh ClipRects vs ROM graphics), because it never replaced layers.

## Evidence

Archive contents (non-.info files under AfA_OS/): AfA_OS_Libs/{diskfont,intuition,picture_datatype,
afa_system,exec,icon,graphics}_lib.exe, Optional/AfA_OS_Libs/zune_lib.exe, libs/oop.library 41.1,
libs/freetype2.library 1.3, C/AfA_OS_Loader 3.10 (19.04.2016), C/morelibspace 46.2,
Optional/Devs/morelibspace_module. `find -iname '*layer*'` gives no matches.

Version strings: graphics_lib.exe "Version 4.8 (22.11.2010)", intuition_lib.exe "Version 4.8 (17.01.2011)".
All of them contain the string "layers.library" plus `_LayersBase` (they open the ROM layers).

### Q1: layers / graphics, loading
- No layers.library. graphics_lib.exe (149 KB) is a partial graphics replacement: SetFunction patches on
  the ROM graphics.library for text/TTF, alpha, RectFill etc. (readme 4.8: "In the graphics_lib.exe a
  problem with RecFill was fixed that occure if it was used in the Border area"). It is not a full
  graphics.library.
- Loading: `C:AfA_OS_Loader` "after (beneath) the AddDatatypes command in your startup-sequence". It
  runs SYS:AfA_OS_Libs/*.exe, which SetFunction the ROM libraries. `morelibspace [REBOOT|REVERSE]`
  must come first in the startup-sequence, before SetPatch. It is a resident that enlarges ROM library
  jump tables so AfA can add LVOs past the V40 end. Evidence: intuition_lib.exe installs
  `_Intuition_IsWindowVisible` at Intuition LVO -834 (`pea 1263c _Intuition_IsWindowVisible;
  pea fffffcbe` before the SetFunction jsr). Docs line 34: "Morelibspace don't work on all Systems jet".
  The docs never mention LoadModule; morelibspace_module only works "together with BlizKick, LoadModule".

### Q2: shape call / LVO clash
- No layers, so no exported ChangeLayerShape. intuition_lib.exe `_Intuition_ChangeWindowShape` (0x7120)
  does LayersBase -120 (LockLayerInfo), `jsr` through -244(a6), which is the vector for LVO **-246**,
  then -138 (UnlockLayerInfo). AROS layers.conf puts ChangeLayerShape at -222 (spike line 131), so AfA
  was compiled against a different, private ordering, -246, which nothing ships.
- The function is dead: the patch install list (41 `pea _Intuition_X` + SetFunction sites) does NOT
  include ChangeWindowShape, HideWindow or ShowWindow. Their only references are self-references
  (DoSyncAction/DoASyncAction trampolines inside their own bodies).
- On 3.2 the -222..-240 clash does not arise, because AfA never touches the layers jump table.
  -246 happens to sit just past the 3.2 V45 slots, which is the same slot the spike proposes.

### Q3: smart-refresh ClipRect offset problem
Not solved, never faced: AfA uses the ROM layers.library with its classic ClipRect layout, so ROM
graphics just works. No AfA code builds hidden ClipRect views.

### Q4: OS support
Docs: "Kickstart 3.1; OS 3.1 better 3.5 or 3.9 best with BoingBags"; "if you use only OS3.1 there is no
PNGIcons and Skining possible" (the FAQ fix is to copy the 3.5/3.9 SetPatch, workbench.library and
icon.library). Requires an RTG card (CGX/P96), an FPU and a fast CPU. No mention of 3.2 anywhere in
the readme or docs (last release 2011 libs / 2016 loader / 2017 upload). On 3.2, intuition_lib.exe would
SetFunction-patch Intuition 47 with AROS-derived intuition code (41 functions incl. OpenScreen,
CloseScreen, MoveWindow, WindowToFront/Back, ZipWindow, ChangeWindowBox). That is a stability risk on
3.2, unverified.

### Q5: Source and license
- Docs: "AfA_OS uses the same Licence Model as AROS" (AROS Public License, MPL-1.1 derived; binaries
  and source are redistributable).
- No source in the archive. Aminet has no AfA source package (search "afa": only misc/os/AfAOS4.8 and
  an unrelated docs/misc/AfA guide). The AROS GitHub master tree (full recursive listing, not
  truncated) has no path matching "afa". Docs line 1176 says AfA code was merged into AROS ("see AROS
  SVN Log"), so the useful parts are already in AROS itself. Unverified: whether the author's old
  site or the AROS SVN archive holds an AfA tree.
- Shipping AfA binaries in a public repo is permitted under APL, but there is nothing in them for
  rounded corners.

## Q6: Use for Amarchy?
- 3.1: no. No layers to reuse. The intuition ChangeWindowShape glue is ~60 lines of 68k that just
  locks the LayerInfo, calls ChangeLayerShape and refreshes. Amarchy would write that itself in an
  hour once a layers with ChangeLayerShape exists.
- 3.2: no, and AfA's intuition patches are an extra risk on Intuition 47.
- Effort vs fresh port: unchanged, 6-10 weeks (3.1 only: 4-6). AfA removes zero items from the spike's
  week table. The only reusable idea is its loader pattern: morelibspace (grow a ROM jump table) plus a
  SetFunction loader. That is not a whole-library replacement via LoadModule, which a layers port
  needs.
- Biggest risk: unchanged. The hyperlayers ClipRect view/offset convention vs ROM graphics, and the
  3.2 V45 LVO slots.
- Cheapest proof test: unchanged from round-corners-spike.md lines 26-27. Build hyperlayers m68k with
  the classic 31 LVOs plus ChangeLayerShape at -246, LoadModule it on rig 8 (3.1), open two
  overlapping smart-refresh windows, shape the front one, then drag and depth-cycle. Pass = the corners
  show the window behind, correct after a move, with no trash in the back window. AfA cannot shortcut
  this test because it has no layers to load.
