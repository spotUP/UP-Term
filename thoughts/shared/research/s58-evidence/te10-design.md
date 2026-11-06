---
date: 2026-11-05
topic: TE-10 design, a theme per public screen name (Amarchy theme editor, owner answer D10)
verdict: DESIGN READY, gated on two prerequisites (the S57 ip_wb_lockable fix is not on main; 8 owner decisions open)
---

# TE-10: a theme per public screen name

Evidence is file:line on main at 92059fac unless a worktree is named. Plan: thoughts/shared/plans/2026-11-05-amarchy-theme-editor.md
(section 5 at :215, section 6 at :235, TE-9 at :370, D10 at :455). Nothing was built or run for this design.

## 0. Prerequisites (block TE-10.1)

- P1. The S57 freeze fix is NOT on main. `git grep ip_wb_lockable -- amarchy tests` finds nothing; `ipd_lock_wb_screen`
  (amarchy/iconpen_dos.c:104-112) still calls LockPubScreen("Workbench") on any listed node with a screen, private or not.
  The fix lives only in worktree vtcon-s57-freeze9 (evidence thoughts/shared/research/s57-evidence/freeze-9boot-rig145.md,
  "Chosen: Option A"). TE-10 generalises that predicate, so it must merge first.
- P2. TE-8 (`gt_pen_budget`) must be on main: TE-10.5 calls it with the target screen's depth. Today only the editor row
  exists (amarchy/gthemes_core.c:2078 "Pen Budget"); `git grep gt_pen_budget` finds no function on main.

## 1. What holds per-screen state today

Everything below is bound to one screen, the Workbench, or to "whatever screen the window is on" with one slot.

| State | Where | Instances | Bound to |
|-------|-------|-----------|----------|
| Pen holder record `ipd_rec` {semaphore, name, `ip_shared`} | iconpen_dos.c:26-30, created by `find_rec` :198-216 under the public semaphore "Amarchy.IconPens" (iconpen_dos.h:14) | one per machine | Workbench, by name compare in `ipd_wb_screen_in` :92-99 |
| `ip_shared` {owners, `ip_holder h`, epoch} | iconpen_core.h:132-136 | one | one `h.screen` identity (iconpen_core.h:56), 16 icon pens from `IP_FIRST` 32 (:13-14) |
| Epoch | `ip_shared.epoch`, bumped in `pens_sync` iconpen_dos.c:190-191, read by `ipd_epoch` :245-255 | one counter | Workbench reopen only |
| Pen sync and recolour | `pens_sync` iconpen_dos.c:157-195, `ipd_recolour` :269-298 | one | `ipd_wb_screen_in` under the list lock |
| Icon label shade cache `icl_pc` | flat.c:650, bound per call `icl_cache_bind(&icl_pc, w->WScreen, ipd_epoch())` :800 | one slot | any window's screen, but stamped with the Workbench epoch; rebinding forgets the old pens without ReleasePen (comment :661) |
| flat extra pens `g_pens` (dim, knob, inbar, sel) | flat.c:1708, `pens_run` :1735-1761 | one | `ipd_lock_wb_screen` :1737 |
| `flat_state` (pen numbers `pen_text`..`pen_sel`, v5 look fields) | flat_core.h:19-51, shared under "Amarchy.Flat" (flat_core.h:15) | one | pen numbers are Workbench pen numbers, written by Amarchy from the theme (desk_core.c:142) |
| Drawn menu / editor window pens | gui.c `screen_bind` :1763-1773 (`g->scr`, `g->cm`), `pens_get` :1462 with `vr_pen_obtain` :1482-1488 | one per open gui window | the front public screen (`lock_front_screen`, handler/frontscr.c:38-44); already per screen in mechanism, but the theme is always AM_GTHEME |
| Theme name | `AM_GTHEME` "amarchy-gui-theme" (amconf_core.h key list), ENV:/ENVARC:Amarchy/prefs (amconf_core.h:1, :127-130), value cap 160 bytes (`UC_MAX_VALUE`, config/upconf.h:60) | one value | the desktop |
| Theme files | `GT_USER_DIR` ENVARC:Amarchy/themes (gthemes_core.h:17), `GT_ENV_DIR` ENV:Amarchy/themes (:291) | per theme | not per screen (no change needed) |
| Terminal palette | `GT_TERM_FILE` ENV:up-term/theme-active (gthemes_core.h:90) | one file | every console |
| Workbench palette pens 0-3 / 0-7 | `desk_dos_apply` (palette.prefs, desk_core.h:26, :103) or `wb_pens_apply` SetRGB32 fallback gthemes_dos.c:38-62 | one | Workbench via `ipd_lock_wb_screen` :51 |
| Public screen names on the bar | `g_scrname[BAR_MAX_SCREENS][32]` bar.c:66, filled by `sample_screens_on` :356-372 | one list | every public screen (the only existing enumeration) |

Single-instance and Workbench-bound: the holder record, its epoch, `g_pens`, `icl_pc`, `flat_state` pen numbers, AM_GTHEME,
GT_TERM_FILE, and the palette path. Per screen in mechanism already: the gui window's pen set (gui.c:1765-1773).

## 2. Data model

### 2.1 Theme keyed by public screen name

- The Workbench theme stays AM_GTHEME. It is not duplicated into the new map (single source of truth); a map entry named
  "Workbench" is refused by the parser.
- New map: one entry per other public screen name, `<screen name> = <theme name>`. Screen names are up to
  `MAXPUBSCREENNAME` 139 characters (NDK intuition/screens.h:579), so one entry alone can pass the 160-byte AM value cap.
  A single list value in Amarchy prefs therefore holds two or three entries at most. Recommended store (decision D10-5):
  a separate upconf file `ENVARC:Amarchy/screen-themes` with an `ENV:` copy, written like `amconf_save` (ENVARC then ENV,
  amconf_core.h:130), plus one switch key `screen-themes` (off/on, default on) in Amarchy prefs. D10's "new prefs key" is
  that switch key; the entries live in the file.
- Lookup is exact and case-sensitive, matching Intuition's own name match (iconpen_dos.c:96 uses strcmp; whether
  LockPubScreen compares case-sensitively is unverified, looked only in iconpen_dos.c and the S57 disassembly notes).
- No entry for a screen: behaviour per decision D10-2 (recommended: the screen is left untouched).
- Pure functions (new, beside `gt_name_ok` in gthemes_core.c): `gt_screen_map_parse(text, map)`,
  `gt_screen_map_write(map, buf, cap)`, `gt_screen_theme_of(map, screen, wb_theme, out, cap)`. The reader reuses the upconf
  parser that amconf uses (amconf_core.h:40 "settings table") rather than a new one.
- How the editor picks the screen: a "Screen" cycle row (decision D10-4) filled from the discovery snapshot (section 5),
  defaulting to `front_screen_name` (handler/frontscr.c:11-35), the same function the gui window binds with.

### 2.2 Pen holder per screen

- Extend, do not copy: `ip_shared` (iconpen_core.h:132) becomes slot 0 of a table `ip_screens` {count, slot[IP_SCREENS]},
  each slot {name[MAXPUBSCREENNAME+1], ip_shared}. Slot 0 is always "Workbench" and keeps every current behaviour (icon pens
  32-47, owners FLAT/ICONS/MAIN, iconpen_core.h:122-124). The record stays one allocation under "Amarchy.IconPens"
  (find_rec iconpen_dos.c:198), so every client still finds one semaphore. Size of the table is not trimmed (no size budget).
- What a non-Workbench slot holds: the theme's role and element pens obtained on that screen (exclusive on free pens, else
  shared best, section 4), not the 16 icon pens (icons are drawn only on the Workbench and Directory Opus windows,
  `icl_accept` flat.c:637-641). Directory Opus on its own public screen is the one case that wants icon label shades there;
  `icl_pc` per screen covers it (TE-10.6).
- Owner: Amarchy main only (`IP_OWNER_MAIN`, driven from the 1 s tick in amarchy.c:212-235). amarchy-flat reads the slot
  of the window's screen; it never obtains role pens on a foreign screen itself.
- Lifetime: screen appears in the list and is public, and the map names it -> slot created, theme read with the list
  unlocked, pens obtained under the list lock (the two-pass shape of `pens_sync` iconpen_dos.c:173-194). Screen gone from
  the list -> slot forgets its pens without ReleasePen (the ColorMap went with the screen; `pen_release` already refuses
  when `h->screen != sc`, iconpen_dos.c:49-54). Same name reopened -> new identity, slot epoch +1
  (`ip_sync_new_identity`, iconpen_core.h:111), `ip_holder_verify` catches a reopen at the same address (:81).
- Amarchy exit: main drops its owner bit on every slot; for each screen still listed and public, pens are released under
  the list lock on the verified identity; slot 0 keeps the current rule (released when the last owner of slot 0 goes,
  iconpen_dos.c:218-243).
- Epoch per slot: `ipd_epoch_of(name)`; `ipd_epoch()` (iconpen_dos.c:245) becomes the slot-0 call so its 10 existing
  users do not change.

## 3. Apply path per setting class (plan section 5)

| Setting | Per screen? | Why |
|---------|-------------|-----|
| Swatch, group, element colours (pens) | Yes | SetRGB32 on the target ViewPort, only on pens Amarchy obtained there (section 4). The Workbench keeps palette.prefs (`desk_dos_apply`) |
| Unsaved preview | Yes | `gt_dos_preview_model` gains the screen name; `pv_set` already writes a ViewPort (gthemes_dos.c:148-152) |
| Terminal palette | No, global | one file `GT_TERM_FILE` read by every console (gthemes_core.h:90); per screen would need the handler to key by screen (decision D10-8) |
| Menu font `AM_FONT` | No, global | one Amarchy key; not asked for |
| Screen, window title, icon font | No, global OS prefs | `font.prefs` is read by IPrefs for the Workbench only |
| Shell font | No, global | the UP-Term font row |
| Title bar height | No, global OS prefs | IControl prefs + `AM_FLATSIZE`; Intuition geometry for every screen |
| Flat look fields (radius, border, knob, glyphs, ghost, hover) | Possible, not recommended now | one `flat_state` (flat_core.h:44-50); per screen means a per-screen look block read in the draw hook (decision D10-7) |
| Gadget size, padding, order, scroll bar width | No, global | Intuition owns gadget geometry; reset-bound |
| Backdrop, icon label style and colour | No, Workbench only | wbpattern.prefs and icon labels belong to the Workbench (and Directory Opus) |
| Pointer colours 17-19 | To probe | whether the sprite colours follow the front screen's ViewPort or are global on 3.1 and 3.2 is unverified; smallest probe: SetRGB32 pen 17 on a second public screen, bring each screen to front, read the pointer pixel |
| Contrast warning, pen budget meter | Yes | pure, take the target screen's depth (`gt_pen_budget(m, depth, out)`, plan :238) |

## 4. Screens Amarchy does not own

- Safe: `ObtainBestPen` shared pens (never changes a colour); `ObtainPen` exclusive on a pen the ColorMap reports free
  (`ipd_pen_is_free` iconpen_dos.c:57-65), then SetRGB32 of only that pen; drawing Amarchy's own windows (gui, themewin)
  on that screen with those pens.
- Not safe: SetRGB32 on pens the opener uses (0-7, its DrawInfo pens, any pen it did not release to the sharing pool). That
  repaints another program's UI with no undo when Amarchy dies. Allowed only under decision D10-3.
- A screen opened without pen sharing has no free pens; whether every pen then reads as taken through `ObtainPen` is to
  probe (smallest probe: open a screen without `SA_SharePens`, run `ipd_pen_is_free` on each pen, print the mask).
- Pen budget per depth (plan section 6) applies per screen with the target depth: depth 3 folds every element to the
  nearest of what is there; depth 4 and 5 give no free pens on a foreign screen in practice (unverified, same probe);
  depth 8 and RTG CLUT give exact pens up to the free count; truecolour needs no pens.
- Pens not obtained: fold to `ObtainBestPen` nearest and list them in the meter (D7, plan :451). Never fail the apply.
- Visitor count: Amarchy must not keep a foreign screen locked. Pens are obtained under the list lock, not a
  LockPubScreen held over time, so CloseScreen of the owner is never blocked by Amarchy. Whether a LockPubScreen without a
  window raises `psn_VisitorCount` (NDK screens.h:569) is to probe; the design does not depend on it.

## 5. Discovery and timing

- Polling, not notification. Amarchy main's 1 s tick (amarchy.c:225) takes a snapshot under `LockPubScreenList`: for each
  node {name, `psn_Screen`, `psn_Flags`, depth, EHB}, copied into a local array, then unlock. A pure diff
  `ip_screens_diff(old, new, out)` gives per name APPEARED, GONE, REOPENED (pointer or depth changed), SAME. Reuse:
  the walk is the one in `sample_screens_on` bar.c:361-372 and `front_screen_name` handler/frontscr.c:20-34; extract it
  once (`pub_snapshot`) and let bar.c use it too.
- screennotify.library is third-party, not shipped (handler/screennotify.h:1-7), and our header declares only the
  Workbench client (:12, :20-21). It cannot be the base; polling is.
- PubScreenStatus: Amarchy never calls it (it is the call the Workbench task blocks in during the S57 ring).
- Lock order rule (generalised from S57, "Violated invariant"):
  1. Holder semaphore "Amarchy.IconPens" first, list lock second; never the reverse.
  2. No DOS I/O under the list lock (theme read first, as `pens_theme_cached`, iconpen_dos.c:115-145, FLAT-BOOT-FREEZE).
  3. No Wait, Delay or WaitTOF under the list lock.
  4. LockPubScreen under the list lock only when `ip_pub_lockable(listed, has_screen, flags)` is 1: listed, has a screen,
     not `PSNF_PRIVATE` (NDK screens.h:574). The S57 evidence shows the reopen path only for the name "Workbench"; for other
     names a private node returns 0. The predicate is applied to every name anyway (one rule, one test).
  5. A private node is "not open yet", never "gone": the slot is not dropped while the node is private (S57 Option B
     rejection: dropping would leave pens allocated on a live screen).
- Gap on main today: `act_screen_front` bar.c:613 and `lock_front_screen` handler/frontscr.c:43 call LockPubScreen with a
  name taken from the list, which can be "Workbench". Neither holds the list lock (no S57 ring) but both can take the
  reopen path on a closed Workbench (BOOT-31). tests/test_wb_lock_guard.py:10 matches only the literal "Workbench" and
  misses both. TE-10.1 closes this.

## 6. Steps (one sonnet agent each; every test written first and seen failing)

- TE-10.1 Generic lock helper. Files: iconpen_core.[ch], iconpen_dos.[ch], bar.c, handler/frontscr.c,
  tests/test_iconpen.c, tests/test_wb_lock_guard.py. Extend `ip_wb_lockable` (from P1) to `ip_pub_lockable`; extend
  `ipd_wb_screen_in` to `ipd_pub_screen_in(pl, name)` and `ipd_lock_wb_screen` to `ipd_lock_pub_screen(name)` (the old
  names stay as one-line calls with "Workbench"). Route bar.c:613 and frontscr.c:43 through it. Tests: test_iconpen
  "a private node of any name is not locked"; test_wb_lock_guard "no LockPubScreen with a non-literal name outside the
  helper". Fail-before: the guard test lists bar.c:613 and frontscr.c:43.
- TE-10.2 Screen map model. Files: gthemes_core.[ch], amconf_core.[ch] (switch key), tests/test_gthemes.c. Pure
  `gt_screen_map_parse/write`, `gt_screen_theme_of`. Asserts: exact lookup; missing name gives the D10-2 default;
  "Workbench" entry refused and AM_GTHEME returned; a 139-character name round-trips; an unknown theme name falls back
  to GT_DEFAULT. Fail-before: functions absent, suite does not link.
- TE-10.3 Holder table and diff. Files: iconpen_core.[ch], tests/test_iconpen.c. `ip_screens` table, slot find/add/drop,
  `ip_screens_diff`. Asserts: slot 0 is Workbench and unchanged by any diff; GONE forgets without a release call (fake ops
  count 0); REOPENED bumps that slot's epoch only; a PRIVATE node keeps its slot. Fail-before: absent.
- TE-10.4 Per-screen sync in iconpen_dos. Files: iconpen_dos.[ch], amarchy.c (tick). `pens_sync` loops the table under one
  list lock; `ipd_epoch_of(name)`; exit releases per slot. Host test via the existing fake ops in test_iconpen.c: two
  screens, closing the second releases nothing on the first. Fail-before: second screen gets no pens.
- TE-10.5 Apply per screen. Files: gthemes_dos.c, gthemes_core.[ch]. Pure `gt_screen_apply_plan(model, depth, free_mask,
  allow_owner_pens, out)` -> list of {pen, rgb, exclusive|best}; `gt_dos_apply_screen(name)` runs it under the list lock
  with SetRGB32 only on exclusive pens. Sentinel `gt_dos_screen_applies`, printed where `gt_dos_wb_pens` is. Asserts: no
  exclusive pen outside free_mask unless allow_owner_pens; depth 3/4/8/truecolour counts equal `gt_pen_budget`.
- TE-10.6 flat per screen. Files: flat.c, flat_core.[ch], iclabel_core.[ch], tests/test_flat_core.c. `g_pens` and `icl_pc`
  become per-screen arrays keyed by identity + slot epoch; the draw hook takes pen numbers from the slot of
  `w->WScreen` (Workbench slot = today's `flat_state` numbers). `icl_pc` releases on the screen that obtained, under the
  list lock, instead of forgetting (fixes the leak at flat.c:661 on a still-open screen). Asserts: pens of screen A never
  returned for screen B; switching windows between screens releases nothing. Unverified: whether the flat frame hook
  draws on non-Workbench screens at all today (no screen filter found by grep for WBENCHSCREEN or screen checks in flat.c).
- TE-10.7 Drawn menu and theme windows. Files: gui.c (`pens_get` :1462 takes the theme of `g->scr`'s name), themewin.c.
  Sentinel AMARCHY_THEME (gui.c:1474) already names the drawing theme. Host test in test_gthemes.c on the name choice.
- TE-10.8 Editor row. Files: gthemes_core.c (`er_add(PG_PAGE_THEME, SD_CYCLE, "screen", "Screen")` beside :2072),
  gui.c, gthemes_dos.c (Save writes the map entry for a non-Workbench screen, AM_GTHEME for Workbench). Asserts: row
  present, Save on "Workbench" leaves the map untouched. Fail-before: row absent.
- TE-10.9 Restore Original and exit. Files: desk_dos.c / gthemes_dos.c per D10-6. Asserts per the decision.

Every Amiga program touched keeps its `__stack` + `$STACK` cookie (flat.c:69 for amarchy-flat); no new program in steps
1-9. The rig helper of TE-9.6 is a new program and gets both.

### TE-9 per-screen case (TE-9.6)

1. Rig on main with TE-10 (`UPTERM_RIG=<n> rig.py start --max --purpose "<session>: theme editor per-screen check"`).
2. Open a second public screen "TE10" at depth 8 with pen sharing, with one window. No program on the rig is known to do
   this (unverified); a small rig helper `pubscreen` (C, `__stack` + cookie) is the smallest tool.
3. amarchy-gui > Settings > Theme Editor > Screen = TE10, Theme = gruvbox (any theme other than the Workbench one); Save.
4. Assert: `ENVARC:Amarchy/screen-themes` names TE10 = gruvbox; AM_GTHEME unchanged; `gt_dos_screen_applies` +1.
5. Pixels: TE10 to front, read the window title bar pixel = gruvbox title colour (or `gt_pen_budget` nearest); Workbench to
   front, title bar pixel unchanged from before step 3.
6. Close and reopen TE10: slot epoch +1 and the pixel of step 5 again. Run on 3.1 and 3.2; append to the test register.

## 7. Owner decisions

D10-1 Which screens are in scope?
- Workbench plus screens named in the map (Recommended): nothing changes on a screen the user did not name; one editor row.
- Every public screen, unnamed ones get the default theme: whole desktop themed; recolours programs that never asked.
- Workbench plus screens Amarchy opens itself: zero risk to others; does not meet D10's "per public screen name".

D10-2 A named-scope screen with no entry (or the switch off)?
- Left untouched (Recommended): foreign programs keep their look; the user opts in per screen.
- Gets the Workbench theme: one look everywhere; repaints programs that set their own palette.

D10-3 May Amarchy recolour another program's own pens?
- Free pens only, shared best for the rest (Recommended): never breaks the program's UI; on shallow screens most colours fold.
- Per-entry "Take Over Palette" switch, default off: exact colours on demand; the program's UI changes and is not restored if Amarchy crashes.
- Never touch foreign screens, only Amarchy's own windows on them: safest; the screen frame keeps its old colours.

D10-4 Where is the screen picker and what does it say?
- "Screen" cycle row at the top of the Theme page, defaulting to the front screen, Workbench listed first (Recommended): one place, matches gui binding to the front screen.
- A separate "Screens" page listing each public screen with its theme: overview of all; one more page and its own Save.
- Picker in the editor window title menu: no row space; hidden from keyboard users.

D10-5 Where are the entries stored, and the key name?
- File `ENVARC:Amarchy/screen-themes` (+ ENV: copy) and switch key `screen-themes` in Amarchy prefs (Recommended): no length limit; a second file to back up.
- One key `screen-themes` whose value lists the entries: one file; 160-byte cap holds two or three entries.
- Key per screen `screen-theme-<name>`: one file, no cap; the upconf key table is fixed (amconf_core.h:40), dynamic keys need a parser change.

D10-6 Restore Original and per-screen entries?
- Restore clears every entry and releases the pens on every screen (Recommended): one predictable "back to stock".
- Restore resets the Workbench only, entries stay: other screens keep their themes; "Original" then means two things.

D10-7 Flat look fields (radius, border, knob, glyphs) per screen?
- Colours per screen, look global (Recommended): one `flat_state` look block; smaller TE-10.6.
- Full look per screen: true "theme per screen"; per-screen look blocks read in the draw hook, a larger TE-10.6.

D10-8 Terminal palette per screen?
- Global (Recommended): one `GT_TERM_FILE`; consoles on TE10 show the Workbench theme's terminal colours.
- Per screen: consoles match their screen; handler change in vtcon, out of Amarchy's files.

## Not covered / unverified

- Pointer colour per screen (section 3, probe named). Free pens on a screen without pen sharing (section 4, probe named).
- LockPubScreen name case sensitivity and visitor count for a lock without a window (to probe).
- Whether amarchy-flat draws frames on non-Workbench screens today (grep only, no filter found).
- A rig program that opens a public screen (none found; helper proposed).

## 8. Owner answers (2026-11-05)

- D10-1: Workbench plus named screens. Replaced by the D10-2 answer below (the owner asked for "all", excludes and selection by criteria).
- D10-2: "Default + name rules". One Default cycle (All screens / Workbench only / None) plus an ordered rule list. Each rule: AmigaDOS name pattern (`DOPUS#?`, `MUI#?`) gives a theme or Exclude; the first match wins. The Screens page lists the open public screens with a tick, plus All / None / Invert / Add Rule buttons. Selection is by name only (no depth or kind criteria).
- D10-3: per-entry "Take Over Palette" switch, default off.
- D10-4: a separate "Screens" page.
- D10-5: own file `ENVARC:Amarchy/screen-themes` (plus ENV: copy) and one on/off key `screen-themes` in Amarchy prefs.
- D10-6: Restore Original clears every entry and releases the pens on every screen.
- D10-7: colours per screen, look global.
- D10-8: Global. One `GT_TERM_FILE`; consoles on every screen show the main theme's terminal colours. No handler change.

All eight decisions are answered. Design complete 2026-11-05.

## 9. Steps as changed by the owner answers

The answers replace the "screen map" (exact name gives theme) of sections 2 and 6 with a rule list. Steps not named here stay as in section 6.

Terms. Main theme = `AM_GTHEME` (today's Workbench theme). Rule = one line: AmigaDOS name pattern, then a theme name or Exclude, then the optional Take Over Palette flag. Default = what a screen with no matching rule gets.

Resolution of one public screen name (pure, `gt_screen_theme_of`):
1. Key `screen-themes` off: Workbench gets the main theme, every other screen is untouched (today's behaviour).
2. The first rule whose pattern matches the name wins (no-case match). Theme rule: that theme, with the rule's Take Over Palette flag. Exclude rule: screen untouched.
3. No rule matches: Default decides. All screens = main theme. Workbench only = main theme for "Workbench", untouched for the rest. None = untouched.

Interpretation by vtcon-c1, not an owner answer (the picked option's preview shows a `Workbench -> tokyo-night` rule): a rule may name Workbench. A theme rule that matches "Workbench" exactly is not stored in the file; the editor reads and writes `AM_GTHEME` for it, so the main theme has one home. An Exclude rule on Workbench leaves the Workbench untouched.

- TE-10.2 Screen rules model (was: screen map). Files: gthemes_core.[ch], amconf_core.[ch] (on/off key `screen-themes`), tests/test_gthemes.c. File `ENVARC:Amarchy/screen-themes` (plus ENV: copy): line 1 `default all|workbench|none`, then one rule per line in order, `<pattern> = <theme>|exclude [takeover]`. Pure `gt_screen_rules_parse/write`, `gt_screen_theme_of(rules, name, main_theme, &theme, &takeover)`. The pattern matcher must be a pure function the host test can run; search the tree for an existing AmigaDOS pattern matcher (vsh, upconf) and reuse it before writing one. Asserts: first match wins; `DOPUS#?` matches `DOPUS.1` and not `MUIPrefs`; the three Default values on an unmatched name; Exclude beats Default All; key off gives today's behaviour; a 139-character name round-trips; an unknown theme name falls back to `GT_DEFAULT`; rule order survives write then parse. Fail-before: functions absent.
- TE-10.5 Apply per screen. `allow_owner_pens` comes from the matching rule's Take Over Palette flag (D10-3), default off.
- TE-10.6 flat per screen. Colours per screen, look global (D10-7): one look block in `flat_state`, only the pen numbers are per screen.
- TE-10.8 Screens page (was: one "Screen" cycle row on the Theme page). A separate page "Screens" (D10-4): Default cycle (All Screens / Workbench Only / None); the ordered rule list with pattern, theme or Exclude, Take Over Palette tick, move up, move down, remove; the list of open public screens, each with a tick that shows whether the rules theme it; buttons All, None, Invert, Add Rule. Ticking an open screen adds or changes an exact-name rule for it. Save writes the file and the key. Asserts (host, row model): page and rows present; All sets Default all and drops exact-name Exclude rules; None sets Default none; Invert flips each listed screen; Save with no rule and Default workbench writes a file equal to today's behaviour.
- TE-10.9 Restore Original (D10-6): clears every rule, sets Default to Workbench only, releases the pens on every screen slot. Assert: after Restore the file holds no rule and the fake ops count one release per held pen on every slot.
- Terminal palette (D10-8): no step. Section 3 row stays "No, global".
- TE-9.6: step 3 becomes "Settings > Theme Editor > Screens: Add Rule `TE10` = gruvbox; Save". Step 4 asserts the rule line in `ENVARC:Amarchy/screen-themes`. Add step 7: rule `TE1#?` = Exclude above it, Save: TE10 pixel returns to its stock value, `gt_dos_screen_applies` unchanged for TE10.

## 10. Owner input 2 (2026-11-05, typed while work ran)

Owner text, verbatim:
- "about what screens to enable or disable it for i think the smartest thing would be if it listed all open screens and let the user anable or disable it for them"
- "default should be workbench only"

Changes to section 9:
- Default starts as Workbench only (file line 1 `default workbench` when no file exists; Restore Original already sets it, TE-10.9). `gt_screen_theme_of` step 3 is unchanged; only the initial value moves.
- TE-10.8: the main control of the Screens page is the list of open public screens (sampled as bar.c `sample_screens_on` does, through the guarded helper of TE-10.1), one row per screen with a tick. Tick on = the screen gets a theme, tick off = the screen stays untouched. All / None / Invert act on this list.
- Interpretation by vtcon-c1, not an owner answer: a tick writes an exact-name rule into `screen-themes` (tick on = theme rule with the main theme, tick off on a screen the Default would theme = Exclude rule). A screen that is not open keeps its stored rule and shows in the list as "not open".
- Open owner question: do typed name-pattern rules (Add Rule, from the earlier answer "Default + name rules") stay beside the tick list, or does the tick list replace them.

## 11. Owner answers 2 (2026-11-05, question tool)

- D10-9 Name rules: "Keep both". The tick list of open public screens is on top; Add Rule (name pattern, theme or Exclude, Take Over Palette) stays below it. The open question of section 10 is closed.
- D10-10 Workbench row: "Tick like the others". Workbench is the first row, ticked by default. Tick off = Amarchy leaves the Workbench colours alone; the theme choice stays stored in `AM_GTHEME`. This confirms the interpretation in section 9 (a Workbench theme rule lives in `AM_GTHEME`, an Exclude on Workbench leaves it untouched).
- D10-11 Ticked theme: "Main theme + per-row cycle". A new tick gives the screen the main theme at once; each row has a theme cycle to pick another theme.

Effect on the steps:
- TE-10.2: the model must hold "Workbench off" (an Exclude rule for the exact name Workbench) without losing `AM_GTHEME`. Test: parse + write round trip keeps the main theme name; `gt_screen_theme_of("Workbench")` answers untouched while the rule stands and the main theme again after the rule goes.
- TE-10.8: rows = open public screens, Workbench first; per row: tick, theme cycle (first entry = main theme), Take Over Palette tick. Below: All / None / Invert, Default cycle (starts at Workbench Only), then the pattern rule list with Add Rule. A tick or cycle change writes an exact-name rule; exact-name rules written by ticks sort above pattern rules so a pattern never hides a tick.

