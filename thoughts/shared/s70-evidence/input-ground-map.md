# Input ground map (read-only analysis, worktree vtcon-dopus-dialog-flat)

Target: every text input draws no outline, ground 3E4254. Paths relative to the worktree.

## 1. Where the cycle gadget gets 3e4254
- No file sets the literal 3e4254 in amarchy/ (grep). It is the tokyo-night GR_SELECTION role: tests/test_gthemes.c:1618 (6th column = SELECTION 0x3e4254).
- Role row: amarchy/flat_role_core.c:9 { FLAT_ROLE_BUTTON, GTE_BUTTON_FACE, GR_SELECTION } ; Workbench rule in amarchy/desk_core.c:140-146 (sel = gt_elem_rgb(GTE_BUTTON_FACE, GR_SELECTION) -> s->sel_rgb[3]).
- amarchy-flat obtains the pen: amarchy/flat_core.c:1088-1091 (ObtainBestPen from sel_rgb -> p->sel -> store->pen_sel), copied per draw at amarchy/flat.c:913.
- Drawn by flat_frame_draw, amarchy/flat_glyph.c:524-527: face = s->pen_button ? pen_button : pen_back, fn(... face) for role FLAT_ROLE_PLAIN (cycle/scroller knob), called from raa_frame_draw (flat.c:932). Header comment flat_core.h:30 (pen_button) and :37 (pen_sel "the pen holding the button fill"; pen_button is only a role number, never drawn with).
- Theme token for inputs: amarchy/gthemes_core.c:566 "text-field" (GTG_BACKGROUND, "string gadget fill"); enum GTE_TEXT_FIELD gthemes_core.h:209. Only consumer today: amarchy/themeprev_core.c:67 (theme preview). No flat code reads it; no shipped theme sets it (per brief; not re-verified).
- Note: the RGB 3E4254 equals GR_SELECTION only for tokyo-night; owner wording "the same grey as cycle" = role BUTTON/SELECTION, or token text-field defaulting to it. UNVERIFIED which the owner wants for other themes.

## 2. GadTools/Intuition string and integer frames today
- flat_frame_fill amarchy/flat_core.c:722: answered ? (!edges_only || push) : push. Header contract flat_core.h:529-531: "string fields, listviews and bevel boxes stay outlines".
- Hook: c8_draw amarchy/flat.c:894-940 (frameiclass dispatcher). fill from IA_EdgesOnly (flat.c:902-906). NO frame-type detection (FRAME_RIDGE etc. not tested; grep shows FRAME_BUTTON only at flat.c:2596, a test object). A string gadget frame is therefore recognised only negatively: not push_gadget_of, not border_button_at, not smart_knob_of (flat.c:921-930).
- Result for a string frame: raa_frame_draw(..., fill=0, role_of(...)) -> flat_frame_draw fill=0 branch = 1 px outline in s->pen_line, 4 sides, flat_glyph.c:529-533. So today: OUTLINE, no fill.
- Interior ground: not touched by flat. It is strgclass's own repaint: StringExtend Pens/ActivePens, default from DrawInfo (BACKGROUNDPEN-ish) -> panel ground 1a1b26. UNVERIFIED (Intuition internals, not in repo).

## 3. Directory Opus fields today
- flat_dopus_ring amarchy/flat_core.c:317-322: FIELDBOX -> 0 always; DRAWBOX raised -> 0, recessed -> 1; BEVEL -> 1.
- d4_fieldbox flat.c:1096-1100 and body hook d4_fieldbody (installed flat.c:2805-2825); both -> d4_flat(FLAT_EL_FIELDBOX) -> d4_flat_pens flat.c:1042-1085.
- Path for a field: flat_dopus_decide (SKIP when shine==back pens) -> rp branch flat.c:1062: flat_dopus_ring==0, so ReadPixel(rp, x0+side, y0+1) gives the interior pen (flat_dopus_face_pen, core.c:334), st.pen_line = that pen, flat_dopus_box_draw (flat_glyph.c:536-539) = flat_frame_draw fill=0 -> 1 px border painted in the INTERIOR pen. Net: border invisible, interior untouched (stays DOpus/strgclass ground 1a1b26). "No ring" is a border-in-ground-colour, not an absent draw.
- dopusstrgclass: no reference anywhere in amarchy/ (grep strgclass: only a comment flat_core.h:556). Only dopusiclass is patched (flat.c:2608-2615, 2836).

## 4. StringExtend / pen patching
- grep StringExtend|sex_|SGM_|STRINGA_|ActivePens in amarchy/: NO hits (only the flat_core.h:556 comment). No existing code sets StringExtend pens.
- SetFunction/CreateGadgetA/AddGList/RefreshGList: no hits in amarchy/flat.c (grep). Patches are class-dispatcher swaps via class_slot() flat.c:2575 (sysiclass, frameiclass flat.c:2588-2619, dopusiclass), the Text() LVO hook (flat.c:1146), DrawBox/DrawFieldBox/bevel body hooks (flat.c:2745-2840), NewObjectA only used to probe classes (flat.c:230, 2578). Other patchers (desk_*, ts_*) not searched: UNVERIFIED whether any other file hooks gadget creation (grep other amarchy/*.c for CreateGadget returned nothing in first pass only on flat.c).
- Candidate points for a StringExtend patch: (a) strgclass dispatcher swap via class_slot("strgclass"/"dopusstrgclass") intercepting OM_NEW/ OM_SET to fill sex Pens (GadTools string/integer are strgclass objects; UNVERIFIED that CreateGadgetA STRING_KIND uses strgclass with StringExtend), (b) CreateGadgetA LVO hook in gadtools.library (none exists).

## 5. Tests
- tests/test_flat_core.c (host): test_dopus_no_ring :563 (flat_dopus_ring asserts :567-580), test_dopus_box_outline :530, test_dopus_decide :424, test_dopus_body_hook :332, test_dopus_bevel_hook :286, test_glyph_frame_bg :809, test_frame_pen :2542; flat_frame_fill asserts :1499-1501. Registered in main at :3492-3541.
- tests/test_amarchy_frames.py exists (no string/field/input hits by grep). tests/test_gthemes.c:1618 role table; tests/test_desk.c:1701 pen expectations; themeprev_core text-field use.
- Reachability test must drive the product entry point (UNVERIFIED which: rig or host harness).

## 6. Recommendation
- Extend the closest mechanism: the class-dispatcher swap (class_slot, flat.c:2575) already used for frameiclass/dopusiclass, adding strgclass (and dopusstrgclass) so StringExtend Pens/ActivePens are set to a pen obtained like pen_sel (flat_core.c:1088 ObtainBestPen path) from the text-field token (gthemes_core.c:566, fallback GR_SELECTION), plus drop the outline: in c8_draw route a string frame to a no-draw / ground-colour border (needs a string-frame detector, none exists: gadget under the frame has GTYP_STRGADGET, mirror push_gadget_of/button_gadget_of flat.c:780 ff).
- Do NOT: fill at FieldBox time or in c8_draw/d4_flat_pens. Design note dopus-dialog-flat-design.md:43-44: "strgclass repaints with its StringExtend pens, so the ground is set there (pens at gadget creation or at the FieldBox call), never by a fill at FieldBox time" -- a fill is overpainted by the next strgclass refresh and erases cursor/text. Also do not reuse pen_button/pen_sel directly as role (pen_button is a role number, flat_core.h:37).
- Per-screen pen: the pen must be per screen like pen_sel (st_scr, flat.c:913); depth <3 screens lack sel pens (flat_core.c:1088 depth>=3 guard).

## agent 46: input ground (UNFINISHED, rig proof not obtained)
- Commits on branch input-ground (worktree vtcon-input-ground): be0f3280 (feature), d8aaeaa6 (sentinels ground=/sg=/ext= on the "dopus bevel" STATUS line). Binary 230568 bytes. flat-budget PASS (312, headroom 72, measured on be0f3280). Host: flat_core 4259 checks 0 failed; mutation (near4 margin 4->40) bites, restored.
- Hook point: frame-draw time. c8_draw (frameiclass) and d4_flat_pens (FIELDBOX) call string_ground, which finds the string gadget under the box in win->FirstGadget and sets StringExtend Pens[1]/ActivePens[1] = pen_sel; c8_draw then draws no outline. No dispatcher swap (no strgclass hook).
- Unload safety: only two pen numbers (UBYTE) written into the foreign StringExtend; no pointers into patcher memory.
- Rig 68 census (MakeDir requester, region 300,250..700,400): interior stays 1a1b26 (46925-47629 px), 7aa2f7 3720-4369 px, 3e4254 only at the Cancel button (438 px). After the sentinel build: dopus bevel calls=3 face=48 nopix=1 ground=0 sg=0 ext=0.
- Reading: sg=0 means string_gadget_at never saw a GTYP_STRGADGET in the window's gadget list (counter sits inside the walk, only reached when a candidate is found; not yet separated from "walk never called"). DOpus fields are probably not plain Intuition string gadgets reachable from FirstGadget (own dopusstrgclass / custom gadget). Hook point is wrong for DOpus; needs a dopusstrgclass or RectFill-interior route.
- Rig steps: (1) FAIL, (2) not run, (3) not run, (4) not run, (5) not run, (6) PASS (Version answers).
- Unverified: GadTools/Intuition string gadgets having StringExtend; frame-before-interior order; theme token for non-tokyo-night; other amarchy/*.c gadget-creation hooks; depth<3.

## agent 47: what the inputs are (UNFINISHED, no measurement obtained)
- Diagnostic commit on branch input-ground: adf4b18d "Record input gadget kinds in flat STATUS for the input-ground hunt" (flat.c, flat_shared.h only; revert with git revert). Builds, flat-budget PASS (chain 312, headroom 72). Binary build/amiga/amarchy-flat 233500 B.
- Adds: sgcalls/sgn counters in string_gadget_at; a 16-record table (kinds F = RectFill inside the last FieldBox or inside a STRGADGET box, D = FieldBox call, S = string_gadget_at hit); STATUS prints "diag ..." + "R ..." lines, then "G ..." lines: every STRGADGET / CUSTOMGADGET of every window and requester ReqGadget list (type, flags, activation, box, SpecialInfo, StringExtend pens, boopsi cl_ID chain), walked under LockIBase into a static buffer and printed after UnlockIBase.
- Rig 68: after `Break 13 C` + `Run RAM:amarchy-flat`, `RAM:amarchy-flat STATUS` hung the Amiga (wasabi daemon silent 25-150 s) on two attempts. First attempt printed under LockIBase (deadlock, fixed in adf4b18d); second attempt with the fix hung too (cause not isolated: STATUS walk vs the new RectFill diag walk in flat_rf_c are both candidates). Third boot (reboot, put, Break, Run, then Version/Status) result not read before budget ran out; rig answers Version at the end.
- Tables A/B/C: NOT MEASURED. Optional dopus source grep: no local dopus5 source found (only hippoplayer.se vendor dir, grep for dopusstrgclass there empty).

## agent 48: gadprobe results

Probe: tests/amiga/gadprobe.c, commit 9af9f959 on input-ground. Rig 68, stock boot, dopus5 lister + IPrefs System Request on screen. T hex includes GTYP_SYSGADGET-type high bits; low 3 bits = type (4 STR, 5 CUSTOM). All gadgets were in win->FirstGadget (L=FG); no requester ReqGadget list held any gadget.

### A: MakeDir requester (window "Directory Opus Request")
```
W"Directory Opus R" L=FG T=8045 F=8014 A=8001 box=ffea,0,18,e SI=0 CL=buttongclass/gadgetclass/rootclass
W"Directory Opus R" L=FG T=8025 F=8024 A=8000 box=0,0,0,d SI=0 CL=gadgetclass/rootclass
W"Directory Opus R" L=FG T=5 F=9280 A=6001 box=b,38,c0,b SI=1 CL=dopusstrgclass/strgclass/gadgetclass/rootclass
W"Directory Opus R" L=FG T=5 F=9000 A=1 box=7,47,4a,11 SI=0 CL=dopusbuttongclass/gadgetclass/rootclass
W"Directory Opus R" L=FG T=5 F=9000 A=1 box=59,47,3b,11 SI=0 CL=dopusbuttongclass/gadgetclass/rootclass
W"Directory Opus R" L=FG T=5 F=9000 A=1 box=a0,47,2f,11 SI=0 CL=dopusbuttongclass/gadgetclass/rootclass
```
### B: lister path field (window "Ram Disk,  1,020..." lister; string rows only)
```
W"Ram Disk,  1,020" L=FG T=5 F=8028 A=2003 box=8,ffea,ffe2,b SI=1 CL=dopusstrgclass/strgclass/gadgetclass/rootclass
```
### C: Time Preferences (SYS:Prefs/Time)
```
W"Time Preferences" L=FG T=8045 F=8014 A=8001 box=ffea,0,18,e SI=0 CL=buttongclass/gadgetclass/rootclass
W"Time Preferences" L=FG T=8065 F=8014 A=8001 box=ffd3,0,18,e SI=0 CL=buttongclass/gadgetclass/rootclass
W"Time Preferences" L=FG T=8025 F=8024 A=8000 box=0,0,0,d SI=0 CL=gadgetclass/rootclass
W"Time Preferences" L=FG T=104 F=8204 A=2801 box=3c,15,29,8 SI=1 EXT=1 SE=1 P=2,0 AP=2,0
W"Time Preferences" L=FG T=5 F=8000 A=1 box=c,27,137,64 SI=0 CL=(private)/gadgetclass/rootclass
```

| input | list | GadgetType | class chain | StringExtend / pens |
|---|---|---|---|---|
| A MakeDir field (box b,38,c0,b) | FG | 0x5 custom | dopusstrgclass/strgclass/gadgetclass/rootclass | n/a (boopsi): SpecialInfo set; no StringInfo walk done |
| B lister path (box 8,ffea,ffe2,b, A=2003) | FG | 0x5 custom | dopusstrgclass/strgclass/gadgetclass/rootclass | n/a (boopsi) |
| C Time prefs string (box 3c,15,29,8) | FG | 0x104 (STRGADGET) | none (classic) | EXT=1 SE=1, Pens 2,0 ActivePens 2,0 |

StringExtend pens writable? A: not directly (boopsi, dopusstrgclass renders itself; strgclass StringInfo not read by the probe: unverified). B: same as A. C: yes (EXT=1, SE=1 in its probe line).
Unverified: the Time prefs string box is only 41x8 (maybe not the visible date field); a Workbench-wide GadTools string with other pens untested. Probe does not print strgclass internals.

## agent 49: boopsi string ground (branch input-ground, commits da8e2dcf probe, 94513af0 fix)

Probe (gadprobe extended, rig 68): MakeDir field and lister path field print `T=5 BSTR=1 EXT=1 SE=1 P=1,0 AP=1,0 CL=dopusstrgclass/strgclass/gadgetclass/rootclass`: Extension is SET, GACT_STRINGEXTEND set, so writing Pens[1] is possible. Time Prefs GadTools field: `T=104 EXT=1 SE=1 P=2,0 AP=2,0`. Ground pen is Pens[1] = 0 (1a1b26 panel).
Fix 94513af0: flat_string_ext(type, act, has_info, has_ext, strg_chain) accepts type 5 with strgclass in the chain (gadget_class_has, shared with the propgclass walk). Host: flat_core 4263 checks 0 failed (new type-5 checks fail on the old function). make amiga ok, flat-budget PASS (chain 312, headroom 72). Binary 230700 B md5 2e7fcf0b.
RESULT: THE FIX DOES NOT WORK ON THE RIG. string_ground is never reached. Evidence: a temporary diag build (not committed, reverted) wrote x0/y0 into sground, x1/y1 into sgseen and 0xDEAD into sgext at the entry of string_ground; after opening MakeDir and Time Prefs STATUS still read `ground=0 sg=0 ext=0`. So neither the d4 FIELDBOX branch (flat.c ~1119) nor c8_draw (~971) calls string_ground for these inputs. The `dopus bevel calls=3 face=3 d5=2` are the three push buttons only.
Census rig 68 (clean reboot, Break 13 C, Run RAM:amarchy-flat): (a) MakeDir field interior rows 306..321 x 336..535: 1a1b26 3156 px + e5e4d9 44 px (cursor/text), 3e4254 ZERO, no 7aa2f7 frame around the field (a 1 px 7aa2f7 line at y=305 spans the window width: a window rule, not the field); whole region 3e4254 438 px = Cancel button only. FAIL. (b) lister path field: not measured (budget). (c) Time Prefs field: Pens stay 2,0 (gadprobe), unchanged. FAIL. (d) 1 px 1a1b26 border: field interior and panel are the same colour, so a border cannot be told apart; not measurable. (e) FAIL: ground=0 sg=0 ext=0. (f) Break 13 C -> flat=absent, Run -> flat=on OK; exec Version answers. 3 listers open/close NOT run.
Rig trap: a second Break + Run cycle of the patcher in one boot leaves the DOpus body hooks dead (`dopus bevel calls=0` although patched=7); only a rig reboot restores them. One Break/Run per boot.
Next hunt: why string_ground is not reached: (1) is c8_draw reached at all for the GadTools string frame; (2) does dopusstrgclass call DrawFieldBox via the jump table or the body hook (d4_fieldbody), or draw the field itself with RectFills. Measure with a per-element counter at d4_flat entry. Pens-write alone cannot work until the draw path is found.
Unverified: (b), 3 listers, Time field pixels. Screenshots deleted. Rig 68 left running with the TEMP-DIAG build as RAM:amarchy-flat (flat=on, process 9), no requester open.

## agent 50: strgclass dispatcher ground

Status: UNFINISHED. Nothing committed. Worktree /Users/spot/Code/UP-Term/vtcon-input-ground (branch input-ground, HEAD 94513af0) holds uncommitted edits: amarchy/flat.c, flat_core.c/h, flat_shared.h, tests/test_flat_core.c.

- Pure function flat_string_render_write + 7 checks: `make test ONLY=flat_core` 4270 checks, 0 failed. Revert-fails check not run.
- Build: make amiga rc 0, make flat-budget PASS (h_Entry variant).
- Variant 1 (strgclass h_SubEntry, entry 0): STATUS gs=1 sg=0 ext=0 ground=0 after MakeDir + lister. gadprobe: P=1,0 AP=1,0 unchanged, EXT=1 SE=1, class dopusstrgclass/strgclass/gadgetclass/rootclass. MakeDir field census all 1a1b26, no 3e4254. FAIL (a)(b)(d).
- Variant 2 (strgclass h_Entry via class_slot entry 1, register convention): STATUS gs=1 sg=0 ext=0 ground=0 after MakeDir; gadprobe still P=1,0 AP=1,0 on both the MakeDir field and a lister field. FAIL (d). Hook installed but the render of dopusstrgclass objects never reaches it. Census not repeated for this variant (counters prove no write happened).
- Checks (c), (e), (f): not run.
- Not verified: why the hook never sees dopusstrgclass dispatch. Candidates: dopusstrgclass dispatcher calls strgclass through a cached/copied entry pointer or its own cl_Dispatcher, not through the hooked slot; or class_slot entry 1 is not the live dispatch word. Next step: read class_slot and how dopusiclass is hooked, then hook dopusstrgclass itself (the leaf class), as the object's class chain begins there.
- Dead draw-time path (string_gadget_at/string_ground): untouched; counters 0 confirm unreached before the fix, but sgseen/sgext now shared with g9_ground, clean up on removal.
- Step 5 map: not done.

## agent 52: class address probe
Rig 68, one boot, probe+STATUS commit on branch input-ground (see git log). Build 231664 B.
(i) resident build (UP-Term:bin/amarchy-flat, older: STATUS has no gs/sg), lister open, gadprobe:
gadget d=0 id=dopusstrgclass class=10029518 entry=10bded78 sub=00000000 data=10028d7c super=1000c450
gadget d=1 id=strgclass class=1000c450 entry=00fde028 sub=00fd5060 data=1000b04c super=1000c270
gadget d=2 id=gadgetclass class=1000c270 entry=00fde028 sub=00fd370a super=1000c238
gadget d=3 id=rootclass class=1000c238 entry=00fde028 sub=00fd360c super=00000000
fresh strgclass d=0 id=strgclass class=1000c450 entry=00fde028 sub=00fd5060 (same as chain d=1); fresh dopusstrgclass d=0 class=10029518 entry=10bded78 (public, NewObject ok)
(iii) after Break C + Run new build (STATUS gs=1), new lister + MakeDir requester open, gadprobe:
gadget d=1 id=strgclass class=1000c450 entry=1046fe2c sub=00fd5060 data=1000b04c super=1000c270   (gadget d=0 dopusstrgclass unchanged 10029518/10bded78)
gadget d=2 gadgetclass entry=00fde028; d=3 rootclass entry=00fde028
fresh strgclass d=0 class=1000c450 entry=1046fe2c sub=00fd5060; fresh dopusstrgclass d=1 strgclass class=1000c450 entry=1046fe2c
STATUS: dopus bevel calls=3 face=3 nopix=0 d5=2 ground=0 sg=0 gs=1 gslot=0xf3f21003 gcode=0xe46a0104 ext=0
Answers:
- chain strgclass IClass == fresh NewObject strgclass IClass: YES (1000c450 both, both boots).
- patcher slot == &that IClass h_Entry: UNVERIFIED. STATUS gslot=0xf3f21003 is not a plausible address (expected 1000c458 = IClass+8); the new hex print is wrong or reads garbage. But the h_Entry word of 1000c450 changed from 00fde028 to 1046fe2c exactly when the patcher ran, so the patcher wrote that IClass's h_Entry.
- word in slot while patcher runs == gate code: probably yes (1046fe2c is a RAM block, not ROM, appears only with patcher); STATUS gcode=0xe46a0104 does NOT match it, so the STATUS hex fields are unreliable (print bug, not investigated).
- dopusstrgclass h_Entry 10bded78: RAM (0x10xxxxxx), not ROM; presumably dopus5.library code (library base not read, unverified); h_SubEntry 0; a public "dopusstrgclass" exists (NewObject by name succeeded).
- Most likely: the h_Entry word of the shared strgclass IClass is hooked (1046fe2c) yet sg=0, so the dispatch path from dopusstrgclass/ROM into strgclass does not go through h_Entry (strgclass keeps h_SubEntry 00fd5060, so DoSuperMethod/DoMethod likely call h_SubEntry), or no strgclass method ran since the patcher started (a gadget render was not forced). Not distinguished.

## agent 53: true counters
Commit 92fa7640 on branch input-ground (STATUS reads the fields from the semaphore record `r` under Forbid; no g_r left in print_status; new sentinels sgpen/sgdep/sgsel/sgnoext printed spen/sdep/ssel/snoext). flat_core host 4270 checks 0 failed; flat-budget PASS; gadprobe builds. Rig 68, one reboot, dripens `0 1 2 0 0 3 0 0 2 2 0 2`, one Break 13 C + Run of RAM:amarchy-flat.
STATUS after Run: `ground=0 sg=0 gs=1 gslot=0x1000c458 gcode=0x1003133c ext=0 spen=0 sdep=0 ssel=0 snoext=0` (gslot/gcode now real; gadprobe shows strgclass entry=1003133c = the gate code, so the hook IS installed).
After NewLister: `sg=4 ext=0`. After MakeDir open: `calls=3 sg=56 ext=3 ground=0 spen=0 sdep=8 ssel=0 snoext=0`. After close: `sg=172 ext=3 ground=0`.
gadprobe: MakeDir field `EXT=1 SE=1 P=1,0 AP=1,0` (dopusstrgclass/strgclass), lister path field `EXT=1 SE=1 P=1,0 AP=1,0`.
Census region 300,250..700,400 with MakeDir open: 3e4254 438 (Cancel button only), 1a1b26 46580, 7aa2f7 4369, a9b1d6 249. Field interior unchanged.
VERDICT (c): hook is called (sg>0), GM_RENDER with GadgetInfo and StringExtend arrives (ext=3, snoext=0), but ground=0 because pen=-1 (spen=0 means pen+1=0). depth 8 passes, so flat_input_pen refuses on `!s` or `!s->pen_sel`: ssel=0 = st_scr(r, gi_Screen) NULL (screen has no slot) or pen_sel 0. Not separated further (unverified which of the two; one more sentinel or printing pen_sel in the pens line would split it).
Also seen: ext=3 vs sg=172, so most dispatches are other methods; only 3 GM_RENDER with GadgetInfo for the MakeDir field. Step (a) not applicable.
Rig end state: 68 running, no requester open (MakeDir closed via Cancel). RAM:amarchy-flat, RAM:gadprobe, RAM:dripens, RAM:gp*.txt left on the rig RAM.

## agent 54: pen_sel split
Commits on branch input-ground: 3f9831d1 (sentinels sscr swscr swb slive sm0..sm3 snogi), then a fix commit (STATUS line buffer 256 -> 512; swscr read guarded by TypeOfMem). Build before the buffer fix FROZE the rig at Run/STATUS (the new STATUS line exceeded the 256-byte stack buffer): reproduced twice, gone with 512. host: flat_core 4270 checks 0 failed, flat-budget PASS (before the buffer change; buffer change is print_status only).
Rig 68, one reboot, dripens flat set, one Break 13 C + Run. STATUS after Run: all sentinels 0. After NewLister: `sg=0 ext=0` (no strgclass dispatch at all this time; agent 53 saw sg=4).
STATUS with MakeDir open: `ground=0 sg=28 ext=3 spen=0 sdep=8 ssel=0 snoext=0 sscr=0x1011f7d8 swscr=0x1011f7d8 swb=0x1011f7d8 slive=0x00000000 sm0=00000101:1 sm1=00000001:3 sm2=00000002:1 sm3=00000003:23 snogi=0`
pens line: `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`, `pens-held 16 refs=1 screens=0 applies=0`.
CASE: sscr == swscr == swb and slive == 0 (case 2): gi_Screen IS the Workbench screen; the live r->st has pen_sel 0, so st_scr returns live state with no selection pen. Not a screen-resolution bug; no fix made. Next: why r->st.pen_sel stays 0 (flat_core.c ~1120 sets store->pen_sel only when depth>=3 and sel_rgb nonzero; flat_state_for ~1299 fills it per-screen slot). Cancel button's 3e4254 is not proof r->st.pen_sel is set (flat.c ~979 takes pen_sel from st_scr of the window screen, which for WB is r->st too: unexplained, unverified).
Methods (MakeDir field): 0x101 x1, 1 (GM_RENDER) x3 all with GInfo, 2 (GOACTIVE) x1, 3 (HANDLEINPUT) x23; snogi=0, so no GM_RENDER without GInfo.
Lister path field: after NewLister no strgclass dispatch at all (sg=0); not typed into/redrawn in this run: unverified, no census/gadprobe run (case 2 stops).
Rig end state: 68 running, no requester open. RAM:amarchy-flat, RAM:gadprobe, RAM:dripens left on rig RAM.

## agent 55: boot resident proof
Host (b59c4222): make amiga ok (232808 B), gadprobe builds, flat-budget PASS, flat_core 4270 checks 0 failed.
Rig 68: installed resident is Rushhours:Tools/UP-Term/bin/amarchy-flat (UP-Term: assign). Backup of the old one: Rushhours:Tools/UP-Term/bin/amarchy-flat.main (227660 B). Branch build (232808 B) copied over the installed file, rig.py reboot, no Break/Run. After boot: process 13 `UP-Term:bin/amarchy-flat` is the resident (no RAM: copy).
NOTE: S:Startup-Sequence on rig 68 has NO ";BEGIN UP-Term Amarchy-Flat" block and S:User-Startup no flat line (greps: none; 84-line file, only AddDataTypes/IPrefs hits). Something else starts the resident at boot (not found).
STATUS after boot: `flat=on calls=193 ... pens text=2 line=3 back=0 outline=3 button=4`, `ground=0 sg=13 ext=7 spen=0 sdep=8 ssel=0 snoext=0 sscr=swscr=0x10146c30 slive=0x00000000 sm0=6:3 sm1=1:7 sm2=102:1 sm3=101:1`, `pens-held 16 refs=1 screens=0 applies=0`.
VERDICT: slive still 0 as boot resident; desk apply did not run (applies=0, screens=0), so sel_rgb stays 0. NewLister/MakeDir/census not done (stop rule). Question for next hunt: who writes sel_rgb at boot (dk_flat_sync_theme in amarchy/desk_dos.c ~575, called by Amarchy process 8) and why it has not run on this rig; Amarchy is running (process 8), ENVARC:amarchy/apps rewritten 22:25 today.

## agent 56: gadtools CreateGadgetA hook design

All paths under `vtcon-input-ground/`. "UNVERIFIED" = not read from source here (NDK/gadtools facts from memory).

### 0. Key finding before building anything
The frameiclass hook already reaches classic GadTools string frames: `c8_draw` (amarchy/flat.c:960) calls `string_ground()` (flat.c:1000-1001, defined 842-853), which walks `win->FirstGadget` (`string_gadget_at`, flat.c:809-834; GTYP_STRGADGET passes `flat_string_ext`, flat_core.c:342-347), writes `Pens[1]=ActivePens[1]=pen` and returns 1 = no outline. So the draw-time path exists for gadtools strings. If it does not show on Workbench Prefs, candidates (all UNVERIFIED, no rig run): (a) GadTools draws its string bevel not through frameiclass IM_DRAW at that time, or in GT_RefreshWindow AFTER the interior was already painted with the old Pens (order: pens written by the bevel draw come too late for the first paint); (b) `flat_string_frame` 4 px tolerance (flat_core.c:359-362) misses the bevel box (GadTools bevel is 2 px outside the gadget box: 2 px each side, within tolerance, but UNVERIFIED for INTEGER_KIND); (c) gadgets not in `win->FirstGadget` yet. Measure which before choosing the hook; a create-time write fixes (a) only.

### 1. Install / remove pattern
- No SetFunction anywhere in flat.c (grep: none). Library-vector hooks are done by swapping the JMP operand: `entry = (uchar*)Base + LVO; slot = entry + 2` (flat.c:1713, 1724). Class hooks swap a Hook slot (flat.c:2785-2802).
- `flat_swap_install(flat_swap *w, uintptr_t *slot, uintptr_t ours)` flat_core.h:113, impl flat_core.c:77-89 (saves old, refuses double install, caller holds Forbid). `flat_swap_remove(w, slot, st)` flat_core.h:118, flat_core.c:91-106 (restores only if slot still ours, else FLAT_STILL_CHAINED, swap goes inert).
- Record fields: `flat_swap xsw` (cf. `gsw` flat_shared.h:29), `struct flat_gate *xgate` (flat_shared.h:31, 90), slot pointer `xslot` (cf. `r->tslot`).
- In-flight safety (FLAT-UNLOAD-INFLIGHT): never-freed AllocMem gate copied from an asm template, counter/closed/orig/handler magic words patched (`gate_make` flat.c:1308-1327, patch table 1286-1289, `gate_open` 1329, `gate_close` ~1337, `gate_reopen`, `gate_wait` 1352-1356). Remove order is exactly `tx_remove` flat.c:1733-1752: gate_close, Forbid, flat_swap_remove, libpatch_done, Permit, gate_reopen on refusal, gate_wait on success. Gate record is kept across install/remove (flat.c:1718-1721).
- `libpatch_done(lib)` (amarchy/libpatch.h:13) after a vector swap (flat.c:1728, 1743).
- Budget gate: `make flat-budget` (Makefile:1219-1225) -> tests/test_flat_budget.py; ENTRY_MAX 64, HELPER_MAX 80, CHAIN_MAX 384, HEADROOM_MIN 64 (test_flat_budget.py:19-22); ENTRIES list line 23 and STUBS dict line 27 must get the new entry (`_gt_cg_c` or similar) and its stub .s. A hook running on the caller's stack must stay within that; the post-hook does a handful of loads and two byte stores, so this is easy if it is a dispatcher of tiny helpers.
- Closest sibling to copy: the Text hook. Install `tx_install` flat.c:1710-1731, remove `tx_remove` 1733-1752, C body `flat_tx_c` 1691-1710 (calls saved vector at 1704 via typedef `textfn`, flat.c:1268), asm gate `amarchy/flat_tx.s:1-33`. It is the only gate whose handler takes register args and returns (the dg template flat_dg.s is for hook-dispatchers and returns whatever the handler returns in d0, so it is the better template for a RETURN VALUE; see 2).

### 2. Calling the original, register arguments
- Text pattern: stub pushes d0,a0,a1, `jsr _flat_tx_c`, `lea 12(a7),a7`, count-out, rts (flat_tx.s:11-21). d0/d1/a0/a1 are scratch for library callers; vbcc C preserves the rest (flat_tx.s:2). The handler calls the saved vector with a6 = the library base (flat.c:1268, 1704: `__reg("a6")` last arg).
- Class pattern: `g9_disp_reg(__reg a0 hook, a2 obj, a1 msg)` flat.c:1124-1129: registers arrive unchanged through `flat_dg_tpl` (flat_dg.s:9-17: `jsr handler; subq count; rts`, handler return value d0 preserved), and the handler calls `((oldfn_reg)gsw.saved)(...)`.
- For CreateGadgetA: copy the dg template (it does not touch a registers and keeps d0). Handler: `ULONG gt_cg_reg(__reg("d0") ULONG kind, __reg("a0") struct Gadget *prev, __reg("a1") struct NewGadget *ng, __reg("a2") struct TagItem *tags, __reg("a6") struct Library *gtb)` { g = ((cgfn)r->xsw.saved)(kind, prev, ng, tags, gtb); post(g, kind, ng); return g; }. Caveat: dg template jumps `jsr handler` with the caller's registers live, so a6 (gadtools base) is available to the handler; the saved ROM routine needs a6 = base (same as Text, flat.c:1268). d1/a0/a1 scratch, so a handler declared with `__reg` a0/a1/a2/d0/a6 matches (UNVERIFIED: that vbcc `__reg` prologue saves d2-d7/a2-a6 it uses; the existing hooks rely on it, flat.c:1124).
- LVO of CreateGadgetA is -30 (gadtools_lib.fd), INTEGER_KIND = 3, STRING_KIND = 12: UNVERIFIED (from memory; no gadtools header grepped in the repo: `grep gadtools` in flat.c returned nothing). The library must be opened (OpenLibrary + keep open, as `dopus5.library` at flat.c:2945, closed 2960/2915) so it cannot expunge while the vector is ours; patch only when the jump is a plain `jmp abs.l` (`flat_jmp_target(entry)` check as flat.c:1715, returns 0 otherwise).

### 3. Reaching StringExtend safely
- Project already has the headers: `struct StringInfo` / `struct StringExtend` used at flat.c:847-848 and 1119-1120 (`((struct StringInfo *)g->SpecialInfo)->Extension`, `Extension->Pens[1] = ActivePens[1]`). No local NDK offsets file needed; no `sg_Extension` spelling in the project (grep).
- Validity: `GACT_STRINGEXTEND` = Activation bit 0x0800 (flat_core.c:346, flat_core.h:`flat_string_render_write`), type GTYP_STRGADGET (type bits 4). Checks before writing: g != NULL (CreateGadgetA returns NULL on failure), `(g->GadgetType & GTYP_GTYPEMASK)==GTYP_STRGADGET`, `g->Activation & 0x0800`, `g->SpecialInfo != NULL`, `->Extension != NULL`, pen >= 0. All of it is `flat_string_ext(type, act, has_info, has_ext, 0)` (flat_core.c:342) plus `pen >= 0`, exactly what `flat_string_render_write` (flat_core.c:349-352) wraps, minus the method test. Pens are plain UBYTEs, no pointer retained, so unload leaves nothing dangling (flat.c:840 comment).
- Kind filter: write only for kind 12 and 3. Other kinds return a gadget whose SpecialInfo is not a StringInfo (e.g. SLIDER), so the type check must stay in as well. Caveat: with INTEGER_KIND, ng_Flags `PLACETEXT` label gadgets etc are separate; the returned gadget is the string gadget (UNVERIFIED).
- Do not walk any list: the returned `g` is the only gadget touched. Do not follow `g->NextGadget`.

### 4. Which screen
- NewGadget has `ng_VisualInfo` only; GadTools VisualInfo is private. I believe its first long is the Screen pointer (vi_Screen) and the second the DrawInfo: UNVERIFIED. No gadtools private header in the repo.
- Draw state API: `flat_draw_state(const flat_draw_tab *d, const flat_state *live, const void *screen)` flat_core.c:1351-1358: NULL or Workbench -> live (`r->st`); listed screen -> its slot; unknown screen -> `d->plain` (pens 0, so `flat_input_pen` returns -1, flat_core.c:334). Wrapper `st_scr(r, scr)` flat.c:350-353 (g_draw table, written by draw_sync under Forbid, flat.c:3205+), so a hook reads without any Intuition call: budget-safe.
- Options: (A) recommended: take `scr = *(struct Screen **)ng->ng_VisualInfo`, accept it ONLY when it equals `g_wb_scr` or is found in g_draw (`flat_draw_epoch(&g_draw, scr) != 0` is the cheap membership test, flat_core.c:1360-1367); otherwise write nothing (fail closed; the draw-time paths in section 0 still cover it). Risk: wrong private layout makes it never match (no pen written, harmless). Do not dereference it without `TypeOfMem(ng->ng_VisualInfo)` (as flat.c:1112 does for the window). (B) use the Workbench state always (`st_scr(r, NULL)`): wrong pen on a custom public screen (TE-10.6b per-screen pens, flat.c:331-336), can write an index that is another colour there. Unsafe. (C) call `GetVisualInfo`-inverse / `LockPubScreen` in the hook: Intuition lock from a foreign task stack, forbidden by the budget and by the earlier freeze. Rejected. (D) defer: remember the gadget and write at first GM_RENDER-equivalent: there is none for GTYP_STRGADGET, so the existing c8 path (section 0) is that deferral.

### 5. Pen lifetime
- Pens: `flat_pens_refresh_on` flat_core.c:1093-1128: releases the slot's previous pens (`flat_pens_release`) then obtains new ones; `store->pen_sel` updated at 1120-1123 only when obtain >= 0. Run from `pens_run` flat.c ~3255-3270 (modes refresh / follow / verify).
- A gadget created before a re-obtain keeps the OLD pen number in Pens[1]/ActivePens[1] for its lifetime. After a theme change or desk apply, that index may be freed, then reused by another colour (ObtainPen shares/reallocs): STALE PEN IS POSSIBLE. Effect: the input shows a wrong colour (or a flat pen of a different kind) until something rewrites it. Nothing rewrites GadTools gadgets except the c8 path (flat.c:1000) when its frame redraws, and nothing at all for boopsi strgclass except the per-render g9_ground (flat.c:1119-1120), which is why that one is safe and a create-time write is not. Stated plainly: a create-time-only write is the weakest option for long-lived windows (Workbench Prefs stays open across a theme change). Mitigation if the hook is built: also keep the c8/`string_ground` rewrite (already there), and record each written (gadget, pen) for nothing; do not store gadget pointers (FreeGadgets frees them).

### 6. Border
- Frame restyle today: the frameiclass hook (FLAT-C8, flat_core.h:290-296: "every GadTools bevel and ridge: buttons, strings ..."), `c8_draw` flat.c:960-1010, decision `flat_frame_decide` (flat_core.h:296) -> SKIP -> `raa_frame_draw`; for a string frame `string_ground` returns a pen and c8_draw returns 1 with NO frame drawn (flat.c:1000-1001): invisible outline. Same for dopus fields: `d4_flat` FIELDBOX path (flat.c:1182-1186, `string_ground` then `flat_dopus_face_pen`, `flat_dopus_ring` flat_core.c:317).
- Will it stay invisible on the new ground? Yes where c8_draw runs and `string_gadget_at` finds the gadget: the box is not drawn at all. Where frameiclass is not the drawer (UNVERIFIED which GadTools routine draws a string's bevel: gadtools bevels go via `DrawBevelBoxA` -> frameiclass in V39+, I believe, UNVERIFIED), the stock bevel (shine/shadow) would show around the new ground and flat_frame_decide can not hide it. A create-time pen alone then gives ground without removing the bevel.

### 7. Host test
- Existing coverage: tests/test_flat_core.c:567-579 (`flat_input_pen` cases 567-574, `flat_string_render_write` 576-579, via CHECK_INT). The OS-side `g9_ground` is not host-testable; the pure functions are.
- Pure function to add in flat_core.c/.h next to `flat_string_render_write` (flat_core.c:349): `int flat_gt_string_ground(unsigned kind, unsigned gadget_type, unsigned activation, int has_info, int has_ext, int has_gadget, int pen)` returning 1 only when kind is STRING_KIND (12) or INTEGER_KIND (3), has_gadget, and `flat_string_ext(...)` and pen >= 0. Cases: kinds 12 and 3 write; kind 1 (BUTTON), 9 (SCROLLER), 11 (SLIDER) never; NULL gadget; no GACT_STRINGEXTEND; no Extension; pen -1; pen 0 is a pen. Reachability (project rule) still needs ONE rig test through the real entry (Workbench Prefs, sentinel count `r->gtground` in flat_shared.h next to `sground` line 86 and in the status line at flat.c:2525).

### Recommended design (5 lines)
1. First measure why the existing c8 path misses Prefs strings (section 0 a/b/c): reachability counters `sgseen/sgext/sground` already exist (flat_shared.h:82-86).
2. If a create-time hook is still wanted: JMP-slot swap of gadtools.library LVO -30, dg-template gate + `gt_cg_reg` handler, remove via the `tx_remove` sequence, library held open.
3. Post-hook: kind in {12,3}, g non-NULL, `flat_gt_string_ground(...)` true, screen from `*(Screen**)ng_VisualInfo` validated against g_wb_scr/g_draw, pen from `flat_input_pen(st_scr(...), depth)`; write two bytes only.
4. Keep the c8 `string_ground` rewrite as the lifetime repair for stale pens; add counter `gtground`.
5. Add the entry to test_flat_budget.py ENTRIES/STUBS and the pure function to tests/test_flat_core.c.

### Top risks
1. Stale pen index after pens_run re-obtain (section 5), no repair for GadTools gadgets except the c8 redraw.
2. VisualInfo-to-Screen layout and LVO -30 / kind numbers are from memory (UNVERIFIED); wrong guess fails closed only if validated.
3. Bevel may not come from frameiclass, leaving a visible stock bevel on the new ground (UNVERIFIED).

Tool calls: 17 (read-only; no scratch files).

## agent 57: resident theme fields at start
Commit 3a43df53 on input-ground (FLAT-BOOT-THEME-FIELDS). `desk_flat_full` (desk_core.c) = desk_flat_state + desk_flat_inbar + desk_flat_look; used by dk_flat_sync_theme (both places) and by flat.c main before the instance guard (static gtheme, `gt_dos_load_current`). gthemes_dos.c links Directory Opus / Workbench-pen code, so gt_dos_read/current/load/load_current moved to new amarchy/gthemes_cur_dos.c, linked by Amarchy, amarchy-menu, amarchy-gui (via gthemes_dos.c lines) and amarchy-flat. Unreadable theme file: gt_load yields the built-in default theme (host test uses a failing reader and gets nonzero sel_rgb). Look fields are zero like on the desk apply path (gt_dos_load_current attaches no model). STATUS now prints `selrgb=RRGGBB pensel=N` from the record copy taken under Forbid.
Binary 232808 -> 233316 B. Tests: flat_core 4272 checks 0 failed; flat_single_instance OK. Without the fix: flat.c main back to desk_flat_state -> test_flat_single_instance fails ("'desk_flat_full(' not found"); desk_flat_full without the inbar call -> flat_core fails at test_flat_core.c:2143 (memcmp) and :2144 (sel_rgb zero).
Rig 68 (installed 233316 B, reboot, IPrefs System Request cancelled, no Break/Run): STATUS after boot `selrgb=3e4254 pensel=252 spen=253 ssel=252 slive=0x000000fc ground=0` (before: ssel=0 slive=0). After NewLister + MakeDir: `ground=0 sg=51 ext=16 spen=253 ssel=252 selrgb=3e4254 pensel=252`. Census 300,250..700,400: 1a1b26 46580, 7aa2f7 4369, 3e4254 438 (bbox 483,325..529,341 = Cancel button only), a9b1d6 249. Field interior 336..535 x 306..321: 1a1b26 3156, e5e4d9 44 (text), 3e4254 ZERO, 7aa2f7 zero in rows 306/322+ (a 7aa2f7 line at y=305 spans the window). No 1a1b26 ring distinct from the field (field and surround are both 1a1b26).
VERDICT: step 1 PASS (resident holds selrgb and pen_sel at boot). Input ground still FAILS: pensel nonzero, field not 3e4254. Open numbers: ground=0 although spen=253/ssel=252, and spen (253) != pensel (252). Stopped per instruction; no further fix.
Rig end state: 68 running, branch build resident, MakeDir closed (3e4254 count 0 after Cancel click), no requester.

## agent 58: extend flag
Cause: ground hook tested Activation 0x0800 (GACT_LONGINT). Fix = flat_string_has_extend(flags, activation) = Flags 0x0400 (GFLG_STRINGEXTEND) or Activation 0x2000 (GACT_STRINGEXTEND); commit 0a75c973 on branch input-ground. Test fails on the old predicate (flat_core.c:576,577,583,588 FAIL), passes on the fix (4281 checks, 0 failed).
Host: make test ONLY=flat_core OK, ONLY=flat_single_instance OK, make amiga OK, make flat-budget PASS.
Rig 68 (reboot after put): STATUS before MakeDir `ground=4 sg=17 ext=11 snoext=0 spen=253 pensel=252`; after NewLister + MakeDir `ground=10 sg=52 ext=14 snoext=0`.
Census MakeDir field 336..535 x 306..321: 3e4254 2275, 1a1b26 881, c1bdab 44 (text), 7aa2f7 0. Region 300,250..700,400: 3e4254 2954, 7aa2f7 4369 (buttons/frames elsewhere). Requester closed via Cancel click 506,333. Note: the ami.py verb is `rexx`, not `rx`.

## agent 59: coverage
Rig 68, build 0a75c973 resident. STATUS after Q1-Q3: `dopus bevel calls=10 face=79 d5=22 ground=16 sg=705 ext=57 snoext=0 spen=253 pensel=252 selrgb=3e4254`.
Q1 MakeDir field (this run the requester sat 6 px left/2 px lower than the earlier census): true field box 330..529 x 308..322 (200x15). Region 320..550 x 295..330: 3e4254 2695 (bbox 330,308..529,330; includes Cancel button rows 325+), 1a1b26 4767, text c1bdab 66 at 334..339 x 310..320, 7aa2f7 764 (window rules at x=323 and x=536 plus y=305: not a field ring). Structure of the field: 1 px 3e4254 outer ring (rows 308/322, cols 330/529); 1 px 1a1b26 inner ring (rows 309/321 x 331..528, cols 331 and 528 x 310..320); 3e4254 interior 332..527 x 310..320. So the 881 earlier 1a1b26 px = the 1 px inner ring (2x198 + 2x11 = 418) plus the area outside the field box in the 336..535 window; no unfilled area right of text or above/below. Ring is 1a1b26 not 7aa2f7.
Q2 Lister path field 213..633 x 562..576 (421x15): 3e4254 5347 in region 205..640 x 556..580 (text 414868 108 = "Path" placeholder), 1 px 1a1b26 inner line at col 214, no 7aa2f7 ring (7aa2f7 only at window border x=209/640). After click at 400,569: 3e4254 5281, cursor c1bdab 66 px at 263..268 x 564..574. After typing x: 3e4254 5270, field still ground. Backspace restored 5281 (Esc did not clear).
Q3 SYS:Prefs/Time: year integer gadget 54..106 x 32..45 and date string 12..319 x 180..194: interior 1a1b26 (3e4254 count 0 in both), 1 px 7aa2f7 outline at the box edge. FAIL (ground no, outline yes). Closed by Cancel click 276,204.

## agent 60: prefs hunt (rig 68, resident 0a75c973; measure + source read, no fix)

Q1 Time Prefs: window 0,14 332x203. ONE string gadget: year integer, screen 60,35 41x8, gadprobe `T=104 F=8204 A=2801 SI=1 EXT=1 SE=1 P=2,0 AP=2,0` (GadTools STRGADGET, has-extend via GACT 0x2000). NO gadget exists for the date field 12..319 x 180..194 (not in win->FirstGadget, not in uitree; uitree has only Save/Use/Cancel, year integer, month button, Hours/Minutes props, calendar custom 12,53 311x100).
Q2 The 7aa2f7 outline is c8_draw -> raa_frame_draw (flat.c ~1001), the frameiclass IM_DRAW hook, after string_ground (flat.c 991) returned -1. STATUS before/after opening Time: sg 2026->2069, ext 61->99, ground 24->24: string_gadget_at IS reached and finds the gadget (flat_string_ext true), then flat_string_frame fails on geometry: frame box 54..106 x 32..45 (53x14) vs gadget 60..100 x 35..42: out by 6 px left/right and 3 px top/bottom; near4 tolerance is 4 (flat_core.c 359-363). Date field: no gadget, string_ground can never hit (UNVERIFIED what draws its text: app-drawn text in a bevel).
Q3 MakeDir field: outer box 330..529 x 308..322 = d4_fieldbox/d4_fieldbody (flat.c 1184-1192), which REPLACES the stock FieldBox and draws only the 1 px ring (flat_dopus_box_draw, flat_dopus_ring=0). Gadget (gadprobe, dopusstrgclass) box rel 11,56 192x11 => screen 334..525 x 310..320 (window left 323 from the measured rules; INFERRED). strgclass paints with Pens[1]=0xfc (ground now works: ground 24->30). The pen-0 ring (cols 331/528, rows 309/321) is the requester background left uncovered between our ring and strgclass's paint: nothing draws it (INFERRED, no diag build). 
Q4 fix: (a) invariant: every input gets full ground, no outline. Time: flat_string_frame must accept the GadTools bevel (gadget +6 x, +3 y; keep 10 px group box rejected) and c8_draw must FILL the whole frame box with the ground pen when string_ground>=0 (a bare return leaves the 5x2 px margin around the gadget as panel colour). (b) MakeDir/lister: when string_ground>=0 in d4_flat_pens, fill the whole box x0..x1,y0..y1 with the ground pen (fn c2_fill) instead of only the ring. Tests: test_flat_core.c flat_string_frame(54,32,53,14, 60,35,41,8)==1 (fails now: near4 6>4) plus a flat_dopus_box_draw ground-fill test; rig census: Time year box and MakeDir field all 3e4254 + text, zero 1a1b26/7aa2f7 inside the box.
State: MakeDir cancelled, Time closed, RAM:gadprobe deleted, one lister (title shows "Aborted..." DOpus status text).

## agent 61: full ground rounded
Commit 55bd83c8 on branch input-ground. flat_string_frame now 6 px horizontal / 4 vertical; both callers (c8_draw, d4_flat_pens) call ground_box -> raa_ground_frame_draw (raa_frame_draw, ids 0, plain role, pen_button = ground: the knob/button rounded fill + corner blend). Rig 68, amarchy-flat 233800 bytes, rebooted.
Census (colour: count inside the box):
- lister path field 213..633 x 562..576: 3e4254 6187, 414868 108 (text), 1f2130 8, 30344b 8 (corner blends), 1a1b26 4 (one per corner), 7aa2f7 0.
- MakeDir field 331..529 x 308..322: 3e4254 2905, c1bdab 66 (text), 1f2130 6, 30344b 6, 1a1b26 2 (corner pixels), 7aa2f7 0.
- Time Prefs year 54..106 x 32..45: 3e4254 622, a9b1d6 100 (text), 1f2130 8, 30344b 8, 1a1b26 4, 7aa2f7 0.
Corner TL 3x3 (lister, Time): 1a1b26 1f2130 30344b / 1f2130 3e4254 3e4254 / 30344b 3e4254 3e4254.

## agent 62: lister path text
Cause (measured, rig 68): the lister path gadget is a dopusstrgclass string gadget with gadprobe `P=1,fc AP=1,fc` (Pens[0]=1 text, Pens[1]=ground). DOpus chose colour register 1. dripens on the flat-themed Workbench (dri pens `0 1 2 0 0 3 ...`): register 1 = 414868, TEXTPEN (dri pen 2) = register 2 = a9b1d6 (the Time Prefs text, GadTools P=2). So 414868 is the app's own pen number, not a patcher pen. The MakeDir "text" c1bdab (66 px, 6x11 at 334..339) is the CURSOR of an empty focused field, not text.
Fix: commit on branch input-ground, flat.c g9_ground writes Pens[0]/ActivePens[0] = flat_input_text_pen(dri_Pens[TEXTPEN], depth). Host: flat_core 4295 checks 0 failed; before the fix test_flat_core.c:584 fails to build (implicit declaration of flat_input_text_pen).
Rig: UNVERIFIED. After `rig.py reboot` the Workbench pens came back STOCK (`0 1 1 2 1 3 ...`, TEXTPEN = register 1 = 414868; Amarchy desk theme not applied, ENV prefs empty, wbpens=0) so the DrawInfo TEXTPEN is itself 414868 and the fix is a no-op there; the flat-pens state of the owner's screen was not restored (amarchy_desk_rig.pick() picked vantablack; prefs reset to tokyo-night, pens still stock).

## agent 63: rig restore + path text census
- Cause measured: ENVARC:Amarchy/prefs held `amarchy-gui-theme = vantablack` (written 23:29:03) so boot applied vantablack (ENV:AMARCHY_THEME=vantablack, stock pens). Why the desk apply then left stock pens: unknown.
- Fix: ENVARC:Amarchy/prefs set to `amarchy-gui-theme = tokyo-night` + `flat-system-look = on`; one rig.py reboot. After: dripens `0 1 2 0 0 3 0 0 2 2 0 2`, ground 1A1B26, ENV:AMARCHY_THEME=tokyo-night, DESK_START_WRITES=1, STATUS flat=on.
- Lister path field 213..634 x 562..577: ground 3f4551 6187, a9b1d6 108, 1f2130 8, 30344b 8, 1a1b26 4 (corners), 414868 0, 7aa2f7 0. PASS.
- Time prefs year box 54..107 x 32..46: 3f4551 622, a9b1d6 100, 1f2130 8, 30344b 8, 1a1b26 4, 7aa2f7 0, 414868 0 (box now filled 3f4551, no 7aa2f7 outline, unlike agent 41).
