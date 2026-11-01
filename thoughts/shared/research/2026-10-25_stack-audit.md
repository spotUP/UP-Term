# Stack audit (rig 6, kit build 83c34d1d)
Method: wasabi ps FREE is current SP headroom only (tc_SPReg/SP vs tc_SPLower, wasabid.c:1352-1379), not a low-water mark.
Measured instead with a scratchpad helper ($S/stk/stkrun.c + runon.s): LoadSeg the program, StackSwap onto a 256 KB buffer filled 0xA5,
call the segment entry with the real args (stdin fed from T:stk.in so ReadArgs works), swap back, scan for the lowest touched byte
(bottom 1024 bytes skipped: exec writes 4 marker bytes there). Peak includes startup + dos.library calls made on the caller stack,
not exception/interrupt frames. All kit programs below use vbcc (`$(VC)`), no program sets `__stack` except upupdate (16384, no cookie).
vbcc: `long __stack = N;` plus `const char x[] = "$STACK: N";` (see amarchy/menu.c:34-35, install/upupdate.c:36).

| program | source | toolchain | stack given | peak used | method | proposed N | notes |
|---|---|---|---|---|---|---|---|
| hl | view/hl_main.c VIEW_HL VIEW_CLI view/vw_plat_amiga.c | vbcc | caller | 10466 | hl t.c (C file 8 KB) | 24576 | measured heavy path |
| mdv | view/md_main.c VIEW_MD VIEW_CLI view/vw_plat_amiga.c | vbcc | caller | 8182 | mdv RULES.md | 20480 | measured heavy path |
| ps | proc/ps_amiga.c tasks_amiga.c PROC_CORE | vbcc | caller | 726 | full task list | 8192 | measured |
| top | proc/top_amiga.c tasks_amiga.c vbsample.s PROC_CORE | vbcc (+vasm) | caller | 846 | top -b -n 2 | 8192 | batch mode only; interactive redraw path not exercised |
| Reboot | install/reboot.c | vbcc | caller | - | none | 8192 | UNMEASURED, would reboot; static: MAXVOL 32 table + packet, estimate under 4 KB |
| rz | zm/zm_amiga.c zmodem.c (-DZM_RZ) | vbcc | caller | 2658 | rz with no sender (init + timeouts) | 8192 | real transfer not run |
| sz | zm/zm_amiga.c zmodem.c (-DZM_SZ) | vbcc | caller | 3186 | sz two files, no receiver | 8192 | real transfer not run |
| UP-Term Prefs | prefs/upprefs.c prefs_*.c kcmode slash upconf | vbcc | caller | 2700 | open, cycle all 4 pages, Cancel | 8192 | colour wheel and ASL font/screenmode requesters not confirmed; use 16384 to be safe |
| UPConsole | device/upconsole.c | vbcc | caller | 1288 | STATUS | 8192 | only the STATUS path |
| UPDemo | demo/updemo.c updemo_amiga.c | vbcc | caller | - | usage only (922) | 8192 | UNMEASURED: scene/window path needs screen driving; usage-path 922 is not a peak |
| upicon | install/upicon.c iconspec.c | vbcc | caller | 1450 | args on a plain file (rc 10) | 8192 | icon write path not reached |
| upsame | install/upsame.c | vbcc | caller | 1146 | file, size, bad CRC (rc 5) | 8192 | measured |
| uptelnet | net/uptelnet.c tn.c upagent.c sha256.c bmsg.c | vbcc | caller | 1214 | localhost 1 (refused, rc 10) | 8192 | UNMEASURED for connected session |
| UPTerm | handler/upterm.c | vbcc | caller | 1162 | echo hi (rc 20) | 8192 | window path not reached |
| upupdate | install/upupdate.c updiff.c | vbcc | own 16384 (`__stack`, no cookie) | - | - | 16384 | UNMEASURED: its startup swaps to its own stack, helper cannot see it; needs a build with __stack removed; add cookie 16384 |
| wasabikey | install/wasabikey.c | vbcc | caller | 880 | run | 8192 | measured |
