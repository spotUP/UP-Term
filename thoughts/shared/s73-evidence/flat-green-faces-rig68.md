---
date: 2026-11-11
topic: flat look, green (00ff9f) unselected button faces on rig 68 Time Preferences
status: narrowed (code path proven, the holder of the pens not identified read-only)
---

# Green button faces on rig 68: root-cause trail

Read and measure only. Rig 68 was not changed (no Copy/SetEnv/reboot, Time Preferences left open).

## Correction to the brief's premise

The earlier "passing" state was green as well. In
thoughts/shared/s73-evidence/rig68-main-eyecheck-state.md:49 and :53 (11:40 start from main, after the same
`Copy ENV:Sys/palette.prefs ENVARC:... CLONE` + reboot) the calendar cells were already `00ff9f`. In
flat-wide-input-rig68.md:75 the cell fill was `00ff9f` too, logged as "palette environment difference". Only the
older "agent 13" run saw a9b1d6 / 7aa2f7. So the 12:39 stop and 13:53 restart did not cause the green; it shows on
every boot of this rig setup since at least 11:40.

## (a) Which pen fills the faces, and the colour map

`VTC:amarchy-flat STATUS` (rig 68, now):

    pens text=2 line=3 back=0 outline=3 button=4 ptext=0
    dopus ... selrgb=3e4254 pensel=12
    pens-held 0 missing 8 9 10 11 12 13 14 15 refs=1 screens=0 applies=0 dim=0 knob=12 inbar=1

The face pen is `button=4` = DK_SELECTION (amarchy/gthemes_core.h:67 enum DK_BG=0, MUTED=1, FG=2, ACCENT=3,
SELECTION=4, DARKER=5, GREEN=6, BACKDROP=7; flat_core.h:30 pen_button = DK_SELECTION; desk_core.c:200).

No read-only colour-map dump exists (GetRGB32 is only in amarchy internals, no verb prints it). Pen table built from
the palette.prefs entries on disk (decoded ENV: and ENVARC:Sys/palette.prefs, identical, 434 bytes), the IPrefs
entry->pen rule at depth 4 (desk_core.c:321-331: entries 0-3 on pens 0-3, entries 4-7 on pens 2^depth-8+entry =
12-15), and pixel colours of known elements in the window grab:

| pen | role | colour | source |
|---|---|---|---|
| 0 | DK_BG (window ground) | 1a1b26 | palette entry 0, grab 35181 px |
| 1 | DK_MUTED | 414868 | palette entry 1 |
| 2 | DK_FG (text) | a9b1d6 | palette entry 2, grab |
| 3 | DK_ACCENT | 7aa2f7 | palette entry 3, grab (Save/Use/selected day) |
| 4 | DK_SELECTION (button face) | **00ff9f** | NOT in the file at depth 4; grab of the faces; = rig stock wb16[4] (tests/test_render.c:888) |
| 5-7 | DARKER/GREEN/BACKDROP | stock (unmeasured; wb16: f0f0ff d2d2e1 ebebeb) | not reached by the file at depth 4 |
| 8-11 | icon pens | stock (unmeasured) | |
| 12 | entry 4 = theme DK_SELECTION colour | 3e4254 | slider track, knob=12 and dopus pensel=12 by ObtainBestPen match |
| 13 | entry 5 | 0e0e14 | file |
| 14 | entry 6 | 9ece6a | file |
| 15 | entry 7 | 0e0e14 | file |

Depth 4 is inferred from the holder plan (`missing 8..15` = desk_pen_plan_for depth 4/5, desk_core.c:204-205) and
pen 12 carrying entry 4. (ami.py `screens` reports depth byte 0x18 for "Workbench Screen"; that daemon field does not
match BitMap.Depth here, unverified what it reports.)

## (b) Why the holder holds nothing

- `screens=0 applies=0` are not evidence: ip_screens_held counts slots >= 1 only (iconpen_core.c:622-628), and
  applies counts plans of slots >= 1 (iconpen_dos.c:286-289). The Workbench is slot 0.
- `missing ...` is printed only when h->screen is set (iconpen_core.c:269). So the holder did bind the Workbench
  screen and ran ip_holder_obtain: every PENF_EXCLUSIVE ObtainPen of 8-15 failed (iconpen_dos.c:49,
  iconpen_core.c:159).
- The same ip_sync (iconpen_core.c:325-333) obtains the live role pens 4-7 exclusively and writes the theme colour
  only `if (h->rhave[k])`. Pen 4 shows stock 00ff9f, so its exclusive obtain failed too.
- ip_holder_verify (iconpen_core.c:228-256) re-tries when any wanted pen becomes free (`gained` -> recheck). Still 0
  held now means pens 4 and 8-15 are still held by other clients, continuously since boot.
- Who holds them: not identified read-only (no pen-owner dump). Candidates on this boot: DOpus 5 (DO_LAUNCHER,
  dopus 68.16 patched) and Workbench colour-icon remap, both share pens with ObtainBestPen; amarchy-flat itself
  shares 12 (knob) and 1 (inbar). Amarchy starts in S:User-Startup:80, before C:LoadWB (Startup-Sequence:80).
- Not the ENVARC copy: the copied file only decides which colours IPrefs puts on 0-3 and 12-15; it never reaches
  pen 4 at depth 4.

## (c) Wrong colour or wrong index

Wrong colour on the right index. The pen map says button = pen 4 (DK_SELECTION) as designed; at depth 4 IPrefs puts
the DK_SELECTION colour (entry 4, 3e4254) on pen 12, so pen 4 depends entirely on the live role write
(FLAT-PEN5-MAGENTA). That write never happens:

- iconpen_core.c:330-332: role colour set only when the exclusive obtain succeeded.
- iconpen_dos.c:548-566 ipd_role_write: returns -1 only when no holder record exists; with a record that does not
  own the pen (ip_role_claim fails) it returns 0 and writes nothing.
- desk_dos.c:731-733 dk_live_pens: falls back to the direct SetRGB32 only on `< 0`
  (S57-PENS47-UNOWNED). With amarchy-flat running (refs=1) and the pen unowned, the theme colour of 4-7 is dropped
  silently.

## Root cause

Invariant: every pen the flat pen map draws with (here the button face, pen 4 = DK_SELECTION) carries its theme
colour on the Workbench screen. The design routes 4-7 only through an exclusive ownership claim; when another
client shares the pen first, neither the holder (rhave=0) nor dk_live_pens (ipd_role_write returns 0, not -1)
writes the colour, and the pen map still points the face at pen 4. Layer: the pen holder / live role pen path
(amarchy/iconpen_core.c:325-333 + iconpen_dos.c:548-566 + desk_dos.c:731), not palette.prefs and not the rig.

## Next decisive measurement (needs a tool or state change, not done)

A read-only verb in amarchy-flat STATUS printing, for pens 0-15: GetRGB32 colour, PalExtra share count, and
rhave[] of the holder. That proves pen 4 = 00ff9f and names how many clients share 4 and 8-15. Then one boot with
DOpus disabled would show whether DOpus is the sharer (state change on rig 68, owner's call).

## Hunt 2: who holds the pens (2026-11-11, rig 68, measured)

Method: the Workbench screen's PalExtra read from guest memory with an ARexx one-liner through `ami.py rexx`
(rexxsupport IMPORT: IntuitionBase+60 FirstScreen, Screen+48 ColorMap, ColorMap+40 PalExtra; pe_FirstFree/NFree/
FirstShared/NShared at +46, pe_RefCnt at +54 (read as one UWORD per pen: the byte read gave 0 for every even pen,
which a shared pen cannot have), pe_AllocList at +58, pe_SharableColors at +66), plus `VTC:amarchy-flat STATUS`,
sampled every ~17 s from 2 s to 125 s after the daemon answered. Two fresh boots (rig.py stop + start clones DH0:
anew; no file on the system disk was edited). The rig was frozen on arrival (daemon silent 150 s), so it was
restarted.

| boot | screen title | free | shared list | exclusive | STATUS |
|---|---|---|---|---|---|
| default (`--max`, DO_LAUNCHER runs) | "Directory Opus ..." depth 4, 1 screen | NFree 0 | all 16 pens (FirstShared 4, NShared 16) | none | `pens-held 0 missing 8..15 ... dim=10 knob=10 inbar=3` |
| `--no-dopus` (ROM Workbench) | "Copyright (c) 1985-1993 ..." (stock WB title) depth 4 | NFree 0 | 0 1 2 3 10 11 12 13 14 15 (NShared 10) | 4 5 6 7 8 9 | `pens-held 2 missing 10..15 ... dim=0 knob=12 inbar=11` |

Default boot share counts pens 0-15: 3 3 3 2 | 1 3 2 1 | 1 1 1 3 | 1 1 3 2. Identical in all 8 samples
(t+2 s .. t+125 s): no pen was ever free, so the holder's verify retry never had a chance.

Answers:
1. Holder: Directory Opus 5 running as the Workbench (process DO_LAUNCHER; the Workbench screen carries its title).
   With it, every one of the 16 pens is shared, pens 4-11 included (pen 4 share count 1). Without it (`--no-dopus`,
   same build, same palette.prefs), the Amarchy holder owns 4-7 exclusive (role pens: theme colour written) and
   8-9. Pens 10-15 stay shared without DOpus too: 12-15 are the palette.prefs entries 4-7 that Intuition/IPrefs
   share (counts 3 1 2 1), 11 is amarchy-flat's own inbar ObtainBestPen and 10 a further sharer (not named).
2. Steady state while DOpus runs: NFree 0 and all pens shared from the first sample to the last; DOpus never frees
   them. Whether the holder bound an earlier screen instance before DOpus opened/took over this one was not
   measured (boot order not traced); either way the outcome is fixed for the lifetime of DOpus.
   Separate, also steady and DOpus-independent: the depth-4 plan wants icon pens 8..15, but 12..15 are always
   shared by the palette.prefs last-four pens, so `pens-held 8` is unreachable at depth 4 by design.
3. Not new for 8..15, new only as a rig-68 symptom for pen 4. Earlier depth-4 records never reached 8:
   thoughts/shared/research/s32-evidence/fresh-stock-look.md:40 (rig 47, OS 3.1, no DOpus WB) `pens-held 4 missing
   12 13 14 15`; thoughts/shared/research/s31-evidence/filled-f11-31.md:24 `pens-held 0 missing 8..15` (DOpus on
   screen, flat not running); thoughts/shared/s73-evidence/side2-aga-rig69.md:62 `pens-held 0 missing 8..15`.
   Every `pens-held 16` and the S57-PENS47 4-of-4 role-pen proofs are depth-8 screens (240 free pens, so DOpus
   and the holder never collide). No record shows pen 4 owned on a depth-4 Workbench with DOpus.

Consequence for the root cause above: on a 16-colour Workbench with DOpus as the Workbench, the exclusive-claim
design for role pens 4-7 cannot succeed; the face colour needs a path that does not depend on owning pen 4.

## Fix proof 612dbd6c (rig 68) -- INCONCLUSIVE (2026-11-11 ~16:20)

Build: worktree /Users/spot/Code/UP-Term/vtcon-s73-face, build/amiga/amarchy-flat 239868 bytes (not rebuilt).
Method: `ami.grab((0,14,332,203))` of W0 "Time Preferences" (grab rows = screen rows, not doubled), colour counted
per uitree gadget interior (box inset 1 px) and at fixed points; scripts measure.py / analyse.py in the session
scratchpad faceproof/ (deleted after). Coordinates below are window-relative.

### BEFORE (old build 239792, rig as left by the FLAT-SPACING eye check, owner root = main checkout)

| element | colour |
|---|---|
| unselected day cell (17,55), calendar interior | 00ff9f (14390 px in calendar) |
| Cancel (233,198 87x14) | 00ff9f (901 px) |
| month cycle "November" (142,32 178x14) | 00ff9f (1856 px) |
| Save / Use | 7aa2f7 |
| selected day (150,74) | 7aa2f7 |
| window ground | 1a1b26 |
| Hours / Minutes slider track | **7f007f** (880 / 890 px), not 3e4254 as expected |
| year field interior | 7f007f |

Census: 1a1b26=35181 00ff9f=17147 7aa2f7=7131 7f007f=5165 a9b1d6=2391 3e4254=272 8f43a0=78 4473ff=24 996fb6=7.
STATUS: `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`; `dopus ... selrgb=3e4254 pensel=10`;
`pens-held 0 missing 8 9 10 11 12 13 14 15 refs=1 screens=0 applies=0 dim=10 knob=10 inbar=3`.
So in that boot pensel/knob were pen 10 (not 12) and the track pen showed 7f007f: the theme colour 3e4254 was not
on the track pen. (Env note 5 of flat-wide-input-rig68.md: 7f007f = palette.prefs missing in that boot.)

### Install

    cd /Users/spot/Code/UP-Term/vtcon-s73-face
    UPTERM_RIG=68 timeout 170 python3 tools/rig/rig.py stop
    UPTERM_RIG=68 timeout 590 python3 tools/rig/rig.py start --max --idle-minutes 0 --purpose "s73: FLAT-GREEN-FACES proof (held-pen faces)"
      -> "refreshed amarchy-flat 239868 bytes", "up"; WARN DOpus 5.104 not installed in this rig's VTC: drawer
    List VTC:amarchy-flat UP-Term:bin/amarchy-flat -> 239868 both (16:20:10)
    STATUS -> flat=absent (S:User-Startup on the fresh DH0 clone has no amarchy-flat block)
    Run >NIL: UP-Term:bin/amarchy-flat BOOT     (amarchy_gui_rig.FLAT_BOOT) -> flat=on
    Run >NIL: SYS:Prefs/Time                    (ami.py open fails: build/amiga/wbrun not built in the worktree)

### AFTER (new build 239868) -- environment NOT comparable

The stop+start cloned a fresh DH0: ENV:Sys/palette.prefs and ENVARC:Sys/palette.prefs do not exist (List), so the
Workbench runs the stock palette, no theme. Census: aaaaaa=51210 000000=6752 6688bb=5274 ffffff=4059 6a6a6a=92
3f3f3f=9. Day cell, Cancel, month, Save, Use, track, window ground all aaaaaa; selected day 6688bb; no 00ff9f.
STATUS: `flat=on calls=15`; `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`;
`dopus=68.16 patched=3 calls=0 flat=0`; `dopus bevel ... selrgb=3e4254 pensel=10`;
`pens-held 0 missing 8 9 10 11 12 13 14 15 refs=1 screens=0 applies=0 dim=10 knob=10 inbar=3`;
`frame=hooked calls=115 flat=0`.
Save/Use lost the accent too (aaaaaa, white/black bevel pixels present) and frame flat=0: the flat look is not
being drawn in this boot, so the faces' colour says nothing about the fix. Re-proof needs the themed environment
(theme apply through the Amarchy menu, amarchy_desk_rig.py ~l.200-245, which writes palette.prefs) restored first.
STATUS still prints `button=4`; whether the fix changes that field or only the fill path is unverified.

End state: rig 68 up, owner {"root": vtcon-s73-face, purpose "s73: FLAT-GREEN-FACES proof (held-pen faces)"},
idle watchdog off, amarchy-flat 239868 resident, W0 "Time Preferences" 0,14 332x203 active.

## Fix proof 612dbd6c attempt 2 (rig 68, main boot + swapped binary) -- PASS (2026-11-11 ~16:30)

Rig started from the main checkout (owner root /Users/spot/Code/UP-Term/vtcon), so the boot carries the theme:
window ground 1a1b26, Save/Use 7aa2f7, ENV:Sys/palette.prefs present (ENVARC:Sys/palette.prefs absent). The start
still printed `[WARN] Directory Opus 5.104 is not installed in this rig's VTC: drawer`, but dopus5.library is
resident (STATUS `dopus=68.16 patched=3`) and pens 8..15 are missing as in Hunt 2. Method as attempt 1
(measure.py / analyse.py, `ami.grab((0,14,332,203))`, window-relative points, gadget interior inset 1 px).

Commands (cwd /Users/spot/Code/UP-Term/vtcon, every one `UPTERM_RIG=68 timeout 170`, start `timeout 590`):

    python3 tools/rig/rig.py stop
    python3 tools/rig/rig.py start --max --idle-minutes 0 --purpose "s73: FLAT-GREEN-FACES proof (held-pen faces)"
      -> "refreshed amarchy-flat 239792 bytes", "up"
    python3 tools/rig/ami.py open SYS:Prefs/Time                               # BEFORE grab (boot-started old binary)
    python3 tools/rig/ami.py put <worktree>/build/amiga/amarchy-flat RAM:amarchy-flat   # List: 239868
    python3 tools/rig/ami.py exec 'Status'        -> Process 12: UP-Term:bin/amarchy-flat
    python3 tools/rig/ami.py exec 'Break 12 C'    -> STATUS flat=absent (clean quit, no still_chained)
    python3 tools/rig/ami.py exec 'Run >NIL: RAM:amarchy-flat BOOT'  -> flat=on chained=0
    python3 tools/rig/ami.py gclick "Time Preferences" 9   (Cancel, gadget id 9); then open SYS:Prefs/Time  # AFTER
    CONTROL: Break, `Run >NIL: UP-Term:bin/amarchy-flat BOOT` (239792), reopen Time, grab
    END: Break, `Run >NIL: RAM:amarchy-flat BOOT` again, reopen Time

The binary swap goes through RAM: (ami.py put) and not VTC:, because the rig gate (rig.py ensure_current) copies the
owning checkout's build/amiga/amarchy-flat back over VTC:/UP-Term:bin copies before every rig command. No file under
build/rig68 or the checkouts was changed. A reboot brings the old binary back.

Confound found and controlled: the boot-started amarchy-flat reported `pensel=10 knob=10 dim=10` (pen 10 = 7f007f
on the track); any restart of amarchy-flat (old or new binary) reports `pensel=12 knob=12 dim=0 inbar=1` and the track
becomes 3e4254. So the control run (old binary restarted the same way) separates the fix from the restart.

| element | BEFORE old, boot (pensel=10) | CONTROL old, restarted (pensel=12) | AFTER new 239868 (pensel=12) |
|---|---|---|---|
| unselected day cell (17,55) | 00ff9f | 00ff9f | **3e4254** |
| calendar interior | 00ff9f 14390 | 00ff9f 14390 | **3e4254 14398** |
| Cancel (233,198) | 00ff9f 901 | 00ff9f 901 | **3e4254 901** |
| month cycle (142,32) | 00ff9f 1856 | 00ff9f 1856 | **3e4254 1856** |
| Hours / Minutes track | 7f007f 880/890 | 3e4254 880/890 | 3e4254 880/890 |
| year field interior | 7f007f 149 | 3e4254 149 | 3e4254 149 |
| Save / Use | 7aa2f7 | 7aa2f7 | 7aa2f7 |
| selected day (150,74) | 7aa2f7 | 7aa2f7 | 7aa2f7 |
| ground | 1a1b26 | 1a1b26 | 1a1b26 |

Census (332x203):
- BEFORE: 1a1b26=35181 00ff9f=17147 7aa2f7=7131 7f007f=5151 a9b1d6=2397 3e4254=272 8f43a0=85 4473ff=24 996fb6=8
- CONTROL: 1a1b26=35181 00ff9f=17147 7aa2f7=7131 3e4254=5409 a9b1d6=2403 666c85=92 4473ff=24 8188a6=9
- AFTER: 1a1b26=35429 3e4254=22309 7aa2f7=7131 a9b1d6=2402 666c85=92 4473ff=24 8188a6=9 (zero 00ff9f)
- END state regrab (new binary again): 1a1b26=35429 3e4254=22308 7aa2f7=7131 a9b1d6=2403 666c85=92 4473ff=24 8188a6=9

Other colours: 4473ff (24 px, x13..207 y64..196) in all runs; 8f43a0/996fb6 (BEFORE) vs 666c85/8188a6 (CONTROL,
AFTER) sit at x103..226 y169..175 (slider knob area), they follow the pensel=10/12 change, not the fix.

STATUS lines:
- BEFORE: `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`; `dopus=68.16 patched=3 calls=1 flat=1`;
  `dopus bevel ... face=1 ... selrgb=3e4254 pensel=10`; `pens-held 0 missing 8 9 10 11 12 13 14 15 refs=1 screens=0
  applies=0 dim=10 knob=10 inbar=3`; `frame=hooked calls=16 flat=0`.
- AFTER: `flat=on calls=17 chained=0`; `pens ... button=4 ...` (unchanged); `dopus=68.16 patched=3 calls=0 flat=0`;
  `dopus bevel ... face=0 ... ground=1 ... selrgb=3e4254 pensel=12`; `pens-held 0 missing 8..15 refs=1 screens=0
  applies=0 dim=0 knob=12 inbar=1`; `frame=hooked calls=164 flat=6`.
- CONTROL: same pens-held / pensel=12 line as AFTER.

Verdict: PASS. With the same pen state (pensel=12, track 3e4254) the old build fills day cells, Cancel and the month
gadget with 00ff9f and the new build with the track colour 3e4254; 00ff9f is gone from the window; Save/Use, selected
day and ground unchanged. Not shown by this run: the boot-time pensel=10 state with the new binary (the new binary
was never boot-started); there the expected face colour is pen 10's 7f007f, which is itself the palette fault of env
note 5 (flat-wide-input-rig68.md). STATUS `button=4` is unchanged by the fix.

End state: rig 68 up, owner root /Users/spot/Code/UP-Term/vtcon, idle watchdog off, RAM:amarchy-flat 239868 resident
(Process 9), W0 "Time Preferences" 0,14 332x203 active. A reboot or a rig restart puts the 239792 build back.

## Fix proof 6ea5ce82 (rig 68, plain boot) -- PASS (2026-11-11 ~17:16)

Question: with build/amiga/amarchy-flat 240036 bytes (built from 6ea5ce82, which re-picks held pens when the palette
changes, 1 s watch) started by the normal boot, no manual restart, does "Time Preferences" show button faces in the
theme colour 3e4254? Method as attempt 2 (faceproof/measure.py and analyse.py, window-relative points, gadget
interior inset 1 px). No source edited, no rebuild; the rig gate copied the main checkout's 240036 build on start.

Commands (cwd /Users/spot/Code/UP-Term/vtcon, every one `UPTERM_RIG=68 timeout 170`, start `timeout 590`):

    python3 tools/rig/rig.py stop      -> stopped (saved 51 rig-written files to build/rig-saved/68/20261111-171259)
    python3 tools/rig/rig.py start --max --idle-minutes 0 --purpose "s73: FLAT-SPACING + FLAT-WIDE-INPUT owner eye check"
      -> "refreshed amarchy-flat 240036 bytes", "up" (same dopus5 [WARN] as before)
    (20 s settle)
    python3 tools/rig/ami.py exec 'VTC:amarchy-flat STATUS'                    # 17:15:45, first reading
    python3 tools/rig/ami.py exec 'Status'     -> Process 12: Loaded as command: UP-Term:bin/amarchy-flat (boot-started)
    python3 tools/rig/ami.py exec 'List UP-Term:bin/amarchy-flat VTC:amarchy-flat'  -> both 240036, 17:14:34
    python3 tools/rig/ami.py exec 'List ENV:Sys/palette.prefs ENVARC:Sys/palette.prefs'
      -> ENV:Sys/palette.prefs 434 bytes 17:14:48; ENVARC copy absent
    python3 tools/rig/ami.py open SYS:Prefs/Time
    python3 faceproof/measure.py boot6ea5 ; python3 faceproof/analyse.py faceproof/grab-boot6ea5.pkl

STATUS (first reading, boot-started process, never restarted):
- `flat=on calls=47 chained=0`
- `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`
- `dopus=68.16 patched=3 calls=1 flat=1`
- `dopus bevel calls=0 face=1 ... selrgb=3e4254 pensel=12`
- `pens-held 0 missing 8 9 10 11 12 13 14 15 refs=1 screens=0 applies=0 dim=0 knob=12 inbar=10`
- `frame=hooked calls=16 flat=0`
pensel was 12 at the first reading, so the 5 s second reading was not needed. Before 6ea5ce82 a boot gave
`pensel=10 knob=10 dim=10`.

| element | measured |
|---|---|
| unselected day cell (17,55) | 3e4254 |
| calendar interior | 3e4254 14398, 1a1b26 13873, a9b1d6 1511, 7aa2f7 492 |
| Cancel (233,198) | 3e4254 901 |
| month cycle (142,32) | 3e4254 1856 |
| Hours / Minutes track | 3e4254 880 / 890 |
| year field interior | 3e4254 149 |
| Save / Use | 7aa2f7 916 / 935 |
| selected day (150,74) | 7aa2f7 |
| ground | 1a1b26 |

Census (332x203): 1a1b26=35429 3e4254=22327 7aa2f7=7131 a9b1d6=2398 666c85=80 4473ff=24 8188a6=7.
00ff9f = 0, 7f007f = 0. Small differences from attempt 2 AFTER (3e4254 22327 vs 22309, 666c85 80 vs 92, 8188a6 7 vs 9)
sit in the slider knob area x103..226 y169..175 (clock time moved the knobs).

Verdict: PASS. The boot-started 6ea5ce82 build holds pensel=12 without a manual restart, and every flat face in
Time Preferences is 3e4254 with no 00ff9f or 7f007f pixel.

End state: rig 68 up, main checkout, idle watchdog off, purpose "s73: FLAT-SPACING + FLAT-WIDE-INPUT owner eye check",
boot-started UP-Term:bin/amarchy-flat 240036 (Process 12), W0 "Time Preferences" 0,14 332x203 active. Grab pickle deleted.

## Full-width proof 0f72dd7b (rig 68, plain boot) -- PASS (2026-11-11 ~17:58)

Question: with build/amiga/amarchy-flat 237320 bytes (0f72dd7b, revert of f4e1369e: read-only display boxes are no
longer shrunk to their text) started by the normal boot, do the Time Preferences display boxes keep the full width the
program gave them? Method as "Fix proof 6ea5ce82" (faceproof/measure.py copied to scratchpad/fullwproof/ with only the
output path changed; plus a per-row colour-run dump of the grab). No source edited, no rebuild.

Commands (cwd /Users/spot/Code/UP-Term/vtcon, every one `UPTERM_RIG=68 timeout 170`, start `timeout 590`):

    python3 tools/rig/rig.py stop     -> stopped (saved 51 rig-written files to build/rig-saved/68/20261111-175050)
    python3 tools/rig/rig.py start --max --idle-minutes 0 --purpose "s73: full-width display boxes owner eye check"  -> up
    python3 tools/rig/ami.py exec 'List UP-Term:bin/amarchy-flat'  -> 237320, 17:55:44 (VTC:amarchy-flat same)
    python3 tools/rig/ami.py exec 'Status'  -> Process 12: Loaded as command: UP-Term:bin/amarchy-flat (boot-started)
    python3 tools/rig/ami.py exec 'VTC:amarchy-flat STATUS'
    python3 tools/rig/ami.py open SYS:Prefs/Time ; python3 fullwproof/measure.py fw

STATUS: `flat=on calls=47 chained=0`; `rectfill installed=yes calls=646 hits=20 ... gboxes=0 side=1 looks=1`;
`dopus bevel ... selrgb=3e4254 pensel=12`; `pens-held 0 missing 8..15 ... dim=0 knob=12 inbar=10`. No `gfit` or `nfit`
field anywhere in the output (grep count 0).

Window W0 "Time Preferences" 0,14 332x203 active. Gadget list (screen coords): year integer 60,35 41x8, month cycle
142,32 178x14, Hours 126,154 194x11, Minutes 126,166 194x11, calendar 12,53 311x100, Save/Use/Cancel at y198.
Measured x below are window columns of the drawn face (border/face inset 1 px from the box edge).

| element | drawn face x | box the program gave |
|---|---|---|
| upper-left date (year) box, window rows 19..30 | 55..105 (corner rows 56..104, 57..103) | 54..106 (= gadget 60..100 + 6 px each side) |
| bottom date+year line, window rows 167..178 | 13..318 (corner rows 14..317, 15..316) | 12..319 (308 wide) |
| calendar, window rows 39..138 | 13..318 (36 full rows) | gadget 12..322 |
| Hours slider frame, window row 160 / 161 | 127..318 / 128..317 | 126..319 |
| month cycle, window rows 19..30 | 143..318 | 142..319 |

Right edges: bottom line 318 = calendar 318 = slider 318 = month cycle 318. Both boxes span the full program width.

Census (332x203): 1a1b26=33437 3e4254=24316 7aa2f7=7131 a9b1d6=2399 666c85=82 4473ff=24 8188a6=7.
00ff9f = 0, 7f007f = 0. Against 6ea5ce82 (3e4254 22327, 1a1b26 35429): +1989 face / -1992 ground pixels, the area the two
shrunk boxes now cover again. Faces Cancel / month cycle / Hours / Minutes / calendar interior all 3e4254; Save/Use 7aa2f7.

Verdict: PASS. Both read-only display boxes are drawn at the full width the program gave them; the bottom line's right
edge lines up with the calendar, sliders and month cycle at x=318 (face) / 319 (box); flat faces stay 3e4254 with no
00ff9f or 7f007f pixel; STATUS carries no gfit/nfit field.

End state: rig 68 up, main checkout, idle watchdog off, purpose "s73: full-width display boxes owner eye check",
boot-started UP-Term:bin/amarchy-flat 237320 (Process 12), W0 "Time Preferences" 0,14 332x203 active. Grab pickle deleted.
