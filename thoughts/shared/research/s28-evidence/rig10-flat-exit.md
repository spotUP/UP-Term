---
date: 2026-11-01
topic: why patina-flat does not stay running on rig 10 (AmigaOS 3.2); FLAT-PREFS-NOT-NOTIFIED
rig: 10 (060 max, --os 32)
build: patina-flat md5 18d0667922958fbae7265ee377007c20 (matches build)
---

# Rig 10: patina-flat exit and FLAT-PREFS-NOT-NOTIFIED

## 1. Why patina-flat exits

Cause: `patina/flat.c:924-925`

    if (is_boot(args) && !boot_wanted()) return 0;

`boot_wanted()` (flat.c:851-856) reads ENV:Patina/prefs via amconf_load /
amconf_flat_index and returns false when neither `flat-system-look` nor
`flat-window-gadgets` is on. BOOT then returns rc 0 with no output. By design,
not a flat.c bug.

Proof on rig 10:

- Prefs with `[profile default]`, `flat-system-look = on`, `flat-window-gadgets = on`:
  `Run >RAM:flat-boot.out RAM:patina-flat BOOT` stays resident. STATUS:
  `flat=on calls=0 chained=0`, `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`,
  `dopus=absent`, `rectfill installed=yes`, `text installed=yes`, `frame=hooked`.
  Snoop: `Open("ENV:Patina/prefs", read) = ok`, `FindSemaphore("Patina.Flat") = fail`
  (expected on first start).
- Prefs as the desk rig script leaves them (`patina-gui-theme = white`,
  `desktop-theme = on`, no flat keys): `RAM:patina-flat BOOT` prints nothing,
  rc 0, STATUS `flat=absent`.

The earlier run 3 failure is this second case: tools/rig/patina_desk_rig.py
(step 3, about lines 381-391) rewrites ENV:Patina/prefs from the pick's file
and appends desktop-theme, so the flat keys are gone before BOOT.

On a fresh rig 10 boot: no Patina process, no ENV:Patina/prefs, no
VTCX:Apps/bin/patina-flat.

## 2. FLAT-PREFS-NOT-NOTIFIED

Patina's StartNotify on ENV:Patina/prefs (patina.c notify_start, about
334-342; handler about 551) calls `reload()` (about 326), which only ran
style_load and the keymap reload. The flat start/sync `dk_flat_sync()`
(patina/desk_dos.c about 437-458, pl_start of `UP-Term:bin/patina-flat BOOT`)
ran only from `desk_dos_apply()`. A direct prefs write therefore never started,
stopped or updated patina-flat.

Fix: `desk_dos_flat_sync()` (changed-only variant of dk_flat_sync) called from
patina.c reload(). Test: tests/test_flat_prefs_notify.py (fails with the
patina.c call reverted).

Rig result with the fixed RAM:Patina (started by plain `Run >NIL:`, stack 16384):

- ON: direct write of flat-on prefs. Within 6 s Patina started it:
  `Process 2: Loaded as command: UP-Term:bin/patina-flat`; snoop
  `LoadSeg("UP-Term:bin/patina-flat") = ok`; STATUS `flat=on`. PASS.
- OFF: write of no-flat prefs (wasabi put after Delete, and DOS Copy). STATUS
  stayed `flat=on calls=0 chained=0`. FAIL.
  `wasabi ps`: `Background CLI (RAM:Patina)` state `ready`, FREE `-`;
  `Patina bar idle` ready 4096/3998; patina-flat wait 32768/28698.
  `wasabi rexx PATINA RELOAD` got no answer in 120 s.
  Windows: only "Patina bar" (task RAM:Patina) and "Workbench"; no requester.

Open (unverified): Patina stops answering after the first flat sync. Suspects:
win_refresh_frames with flat on (the s28 flat-on hang), or stack 16384 because
plain Run ignores the $STACK cookie. Next probe: rerun under `Stack 262144`,
then read ThisTask and tc_SPLower/tc_SPUpper against USP. The theme-pick hang
test (desktop-theme=on + flat-system-look=on) was not run.
