---
date: 2026-11-05
topic: S57 icl AA shade pens pink (255,119,238)/(255,85,170) on rig 144 boot1
status: static analysis only, no rig run; one testable question open
---

# icl shade pens pink on rig 144 boot1

Source: thoughts/shared/research/s57-evidence/rig96-freeze-wb-redraw.md "Fix run" (commit 703365f0). main b4406ecf.

## 1. Every palette writer on the Workbench screen (grep SetRGB4/SetRGB32/LoadRGB4/LoadRGB32/ObtainPen in amarchy prefs handler render install dist)

| site | pens | owned? |
|---|---|---|
| amarchy/gthemes_dos.c:51 wb_pens_apply | 0-3 | no (Intuition DrawInfo pens; theme rewrite by design) |
| amarchy/gthemes_dos.c:146 pv_set (desk preview) | 0-7 | no, restored after |
| amarchy/desk_dos.c:626 dk_live_pens | desk_live_role_pens(depth): 4-7 at depth 8 | no (never obtained) |
| amarchy/iconpen_core.c:235-236 ip_sync -> iconpen_dos.c:72 pen_set_rgb | same 4-7 (desk_live_follow) | no |
| amarchy/iconpen_core.c:122 ip_recolour -> pen_set_rgb | 32-47 at depth 8 | yes, only have[i] (PENF_EXCLUSIVE, iconpen_dos.c:46) |
| render/amiga_render.c:169/588 | scratch pens | yes, ObtainPen(-1, EXCLUSIVE) |
| handler/vtcon_handler.c:3238 | 0-15+ | own screen only, not Workbench |

No SetRGB4/LoadRGB4/LoadRGB32 anywhere in our tree. The pink is 12-bit (0xF7E, 0xF5A: every channel n*17); no theme
file or source contains ff77ee/ff55aa (grep of all UP-Term repos), and the pens-4-7 role colours (selection, darker,
green, backdrop=darker) are not pink. So none of our SetRGB32 writers produced it: their colours are theme 8-bit values.

Latent hazard (real but not this symptom): dk_live_pens and ip_sync write pens 4-7 without owning them. On an
8-plane Workbench those pens are reached by no palette.prefs entry, so nobody holds them; ObtainBestPen from any
client (icl, flat dim/knob/inbar, icons, datatypes) can be handed one as a free pen and is then recoloured on the next
sync. Root fix there: ip_sync obtains pens 4-7 PENF_EXCLUSIVE in the holder (like 32-47) or writes only pens it holds.

## 2. Cause named for the pink: icl's identity check lags a same-address Workbench reopen

- flat.c:794 `icl_cache_bind(&icl_pc, w->WScreen, ipd_epoch())`: identity = screen pointer + ipd epoch.
- The epoch only moves inside pens_sync (iconpen_dos.c:186-187), which runs from the ipd watch step, not at the reopen.
  ip_holder_verify (iconpen_core.c:149-171) detects a same-address reopen by asking the ColorMap (is_free), but only
  for ipd's own pens; icl has no such check.
- Window: Workbench closes and reopens at the same Screen address (boot-time IPrefs/screenmode reopen) -> labels are
  redrawn before the next pens_sync -> (screen, epoch) still match -> icl_cache_find (iclabel_core.c:137-146) returns
  pen numbers obtained on the OLD ColorMap. Key fg_rgb/bg_rgb still match because IPrefs loaded the same palette.
  The new ColorMap never allocated those pens: they show its fresh default colours (12-bit, pink) and are free for
  anyone. When the epoch moves later, bind forgets them without release (correct), but the label pixels already drawn
  stay pink until a repaint. Boot0/3 had no reopen inside that window -> grey.
- Same latent lag in flat_pens (flat.c:1745-1749 uses the same (id, ipd_epoch)).

Confidence: medium. Mechanism proven in code; the two facts not proven statically: (a) boot1 had a same-address WB
reopen before a pens_sync, (b) F7E/F5A are the OS 3.2 fresh-ColorMap defaults of the cached pen numbers.

## 3. Proposed fix (root, the identity layer)

Make the identity check ask the ColorMap at use time, the ip_holder_verify pattern (reuse it, do not invent):
in icl_label before trusting the cache, probe one held cache pen with ObtainPen(cm, pen, 0,0,0, PENF_EXCLUSIVE|
PENF_NO_SETCOLOR); success = the pen is free on this ColorMap = not ours = reopened -> ReleasePen the probe,
icl_cache_clear (no release of cached pens). Cleaner alternative: bump the epoch synchronously at Workbench close
(ipd hook on CloseWorkBench/OpenWorkBench) so every client (icl, flat_pens) sees it; that fixes both users at once.
Host test: tests/test_iclabel.c fake ColorMap where a cached pen reads free after a "reopen" with equal screen+epoch
-> cache forgotten, ObtainBestPen called again; fails on b4406ecf.

## 4. The one testable question (not run)

On rig 144 boot1-like run: log, in icl_label, (screen ptr, ipd_epoch, cm ptr) and for each cache hit whether the pen
is free on cm (exclusive NO_SETCOLOR probe). Prediction: hits on free pens occur after a WB reopen and before the
epoch bump, and GetRGB32 of those pens = F7E/F5A.
