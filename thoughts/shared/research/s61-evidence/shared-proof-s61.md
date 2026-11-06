# s61 shared proof: 1bd9220b, 9b2caff8, d5294c47 on rig 164

Build: origin/main d5294c47, fresh full install (Installer all ticked, [PASS] fresh-all), rig 164 os32 060 --max, JIT, no --mmu. Date 2026-11-06.

## A 1bd9220b: PASS
20 x `echo hi >XCON:0/30/640/200/t/CLOSE`, 20/20 rc 0, no reset (reset count 7 -> 7 by the same host count as the prior run), RAM:t0.txt survived.

## B 9b2caff8: PASS (colours); font not measured
Profile `[profile default]` with distinct colours plus theme-active (tokyo-night). Pixels via PIL: background (26,27,38) = 1A1B26 before; after `Delete ENV:up-term/theme-active` background (32,64,32) = 204020, the profile colour. No profile = colours stay by design (only font reset runs).
Found, unverified: after deleting theme-active and re-creating it with Copy, new windows and the open one did not take the theme until reboot. Cause not investigated.

## C d5294c47: PASS for new directory and no hang; completion does not insert
vsh window thm5: `CD SYS:C`, `CD RAM:`, `Type tabprobe_z`, Tab. Screenshots c5/c6: Tab shows a highlighted completion candidate (`tabprobe_z` + highlighted `z`); tabprobe_zzz exists only in RAM:, so the candidate came from the new directory. Prompt showed "RAM Disk:".
10 rounds of CD SYS:C / CD RAM: / Type tabprobe_z / Tab / Return: no hang, no crash, window answered afterwards (`exec Echo alive`).
Caveat: Return after Tab ran the typed text (`TYPE can't open tabprobe_z`), so the candidate was not accepted by Return. Not judged a failure (accept key unknown); owner to confirm intended behaviour. Screenshots were read, not pixel-measured (candidate evidence is text on screen).
Host reset count by grep -ci on fs-uae.log.txt: 15 at end (different method than A's 7; not comparable, no visible reset).
