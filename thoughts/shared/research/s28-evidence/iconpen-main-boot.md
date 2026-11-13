# ICONPEN-MAIN-BOOT (s28 ICONPEN-REOPEN-GAP)

Patina main holds the 16 icon pens (32-47) while the patina-icons manifest exists
(SYS:Storage/Icons/Patina/manifest). Owner bit IP_OWNER_MAIN (4). Decision table in
`ip_main_step` (patina/iconpen_core.c); wiring in `iconpens_follow` (patina/patina.c),
called once at start, on every 1 s tick hook, and `iconpens_stop` at exit.

Test: suite `iconpen`, `test_main_takes_pens_at_boot` (tests/test_iconpen.c).
- With fix: `[OK] iconpen: 2069 checks, 0 failed`.
- Fix reverted (ip_main_step returns NONE when applied): `[FAIL] iconpen: 2069 checks, 14 failed`.

Rig 16 (68020 max, OS 3.2, build/amiga/Patina md5 68e68869 = VTC:Patina):
- APPLY (needs UP-Term: assigned to a dir with Patina/Icons/lucide.dat; rig has none by default).
- Reboot: STATUS `icons=applied`, `pens-held 0 refs=0` (no holder).
- Start Patina alone: `pens-held 16 refs=1`.
- RESTORE: `pens-held 0 refs=0`; Patina exit also drops.
- Workbench reopen: NOT exercised on the rig (CloseWorkBench returns rc 10 with windows open).
  Covered by the unit test only (new screen + palette, pens re-held).

Gap (out of scope): the holder only re-obtains on a changed screen or depth
(`ip_sync_decide`); a theme switch with only MAIN owning does not recolour the held pens.
