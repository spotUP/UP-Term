# S85m gtheme hotkey wedge repro (rig 84), 2026-11-16 03:00

Verdict: NO wedge on current local main build (VTCX:Apps/bin, not rebuilt). 3 presses, daemon answered `Echo ok` every ~2 s for 40 s each (0.0-0.2 s latency).
Setup: ENV:up-term/keymap.user `[bind]` with `super+ctrl+f10 = gtheme:starship`, `super+ctrl+f9 = gtheme:nord`; PATINA RELOAD; keys ami.key(0x59|0x58, qual 0x48 = lamiga+ctrl).
Press 1 (03:00:07, starship): daemon alive all 40 s; Keep requester seen at +23.0 s (late), Keep clicked +25.0 s; WBCLOSES 2, PUT_SKIPS 2, CONFIRM_SHOWN 1/KEEPS 1.
Press 2 (03:00:54, nord): alive; requester +2.3 s, Keep +4.4 s; DESKS 2, CONFIRM_SHOWN 2/KEEPS 2, WBCLOSES 3.
Press 3 (03:01:42 starship, then nord at +3 s as race): alive; requester +0.1 s, Keep +2.3 s; DESKS 4, CONFIRM_SHOWN 3/KEEPS 3 (3 asks for 4 presses: the race press folded in), WBCLOSES 5.
No REVERTS counters, PATINA_THEME stays vantablack (terminal theme env, as in the S85 hunt). Patina bar window present after every press.
Caveat: after each press an IPrefs "System Request" (Retry/Cancel) stayed up, plus one from UP-Term:bin/patina-icons ("UP-Term:" volume not mounted at that moment); these did not stop the daemon. Cancelled at end.
Not reproduced, so the 11-15 wedge lead (IPrefs close hook waiting on clients) is unconfirmed; it may need the unassigned-UP-Term: state or older binaries (pre 530affc2/d68c2f3b/f95d2835).
Log: scratchpad s85m_run.log / s85m_run.py.
End state: rig 84 up, Patina/patina-gui/menu running from VTCX:Apps/bin, Patina bar + Device List windows open, no requesters, keymap.user deleted, PATINA RELOAD sent.
