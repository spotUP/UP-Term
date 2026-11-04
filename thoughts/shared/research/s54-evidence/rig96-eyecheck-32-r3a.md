# Rig 96 eye-check on AmigaOS 3.2, run 3a (s54, vtcon-ee R3a)

Rig build: main checkout ae859c52 == origin/main (verified). Kit: `UPTERM_PORTS=.../kit-main make dist` rc=0.
Install: `installer_rig.py ONLY fresh-all` with --os 32 --max, INSTALLER_KEEP=1: [PASS], menu window after reboot True. Version: Kickstart 47.115, Workbench 47.2. Rig 96 left RUNNING.

| Row | Verdict | Evidence |
|---|---|---|
| j strut_prebar | PASS | `ok strut_prebar: opened [20, 40, 600, 216] bar top 246 ends [246] clamped 1 -> 2; after BAR OFF [(20, 40, 600, 216)] (want 20 40 600 216)` (amarchy_bar_rig.py --only strut_prebar, passed 1 of 1). Harness bug (dec2222f) was the cause of the R2 FAIL; product clamps and BAR OFF restores. |
| b window knob colour | FAIL | XCON FillA (30,40,400x150) with scrolling content (text area changes after 3 down-arrow clicks): prop gadget 23312 box (416,52,10x105) = 1050 px, all 1A1B26, in the active window; no 7AA2F7 knob pixels. Shots r96r3a-2-fillA-overflow.png, r96r3a-3a/3b.png. Same as run 1 (S32-KNOB-NONE-FRESH still open on main ae859c52). Inactive state not re-measured. |
| c FLAT-SCROLLER | PARTIAL | Arrow ring PASS: after 3 clicks on the down arrow, right edge (x428-429), bottom (y189), left (x30-31) have 0 pixels other than 7AA2F7 (298/400/298 px). Arrow gadget area has only 1A1B26 + 7AA2F7 (88 px). Knob part = row b FAIL. |
| d AMARCHY-WB-BACKDROP / RL3 | PASS | Boot shot r96r3a-0-boot.png: backdrop 0E0E14 at (5,226),(300,236),(630,128),(610,100); 37373 px 0E0E14; window interior 1A1B26 (darker backdrop distinct); counts of EE9900, 6688BB, 000000, FFFFFF, 55DD55 are all 0. Status shows Amarchy, amarchy-flat, amarchy-gui, amarchy-menu running. WB backdrop window is 640x245 frameless. |
| e BAR-STRIP-RESERVE | PARTIAL | Windows: PASS via strut_prebar (row j). Backdrop icons above the strip (Clean Up / snapshot): NOT RUN (budget). |

Notes: no crash or Guru seen. Windows drawn by shots at ~/Desktop/upterm-shots/r96r3a-*.png. FillA closed (EndCLI) and RAM:fill deleted.
