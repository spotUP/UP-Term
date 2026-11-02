---
date: 2026-11-03
topic: ledger verification of listed plans (s52)
---
Counted from plan checkbox lists (grep -c on "- [ ]" / "- [x]") plus git log; builds and rigs not rerun.

| Item | Verdict | Proving commit | Open |
|---|---|---|---|
| 1 kitty K37-K41 | DONE | K37 a14fe089/5ff4b661, K38 ceacf685/41acbb56, K39 eaa21b90/b0f5c9da, K40 c1518f39..85c5c1f7, K41 b0a0a42a + AGA half cbd9e471 | K36 AGA/RTG rig proof, K19-K35 planned; plan boxes K37-K39 were unticked, now ticked |
| 2 amiga-pi | PARTIAL | 233 of 235 ticked | O6 rig step 14 + one real login per provider (owner) |
| 3 shell tutorial | PARTIAL | b6cd4eb3, 0ffc7d42, be8228c1 | L-C1 (facts on vsh/rig), L-R rig check; plan status line stale |
| 4 window lesson | DONE | P1 92528f6f, P3 e48bf6a9, P4 fd788ed8, P5 3916391d, P6/P7 680fc49a | none (status line fixed) |
| 5a upkg browse dialogs | PARTIAL | 07a8199b (Phase 3), 8206576, 38c8d21c | plan has no checklist; ledger rows BROWSE-DIALOGS, BROWSE-SNAPPY, P3d hang hunt unresolved |
| 5b rolling feed | NOT DONE | none | .github/workflows has only nas-image.yml; F1-F13 all open |
| 5c UHC repo | PARTIAL | up-uhc 1.5 (up-uhc d6a2a75b), U8 rig note | plan has no ticks; U2-U7 upkg-side (libupkg UHC source, CLI, browse) not found in upterm-ports log |
| 5d P10.5 | DONE (code), plan status stale | f5a8fe44, b7ede5fd (P10.5-6 7 of 7 on rig 3) | plan status line still "not started"; not edited |
| 5e upkg speed | PARTIAL | step 1 profile only | optimisation steps open; plan 2026-10-18-upkg P5.1/P5.2/P7.4/P8.0-P8.4 open |
| 6 handler rasterizer R3+ | PARTIAL | R3-R5 ticked; R6 b829462f, 184fdbb8, 1a87b4a4 | R6.1d and R6.2b A1200, R6.B1-B4 |
| 7 Selecta AA | PARTIAL | b829462f (own dither blend, not Selecta code) | ledger SELECTA-AA-BLEND / -REUSE open; owner picked AGA-dither fix only, no RTG readback |
| 8 installer finds addons | DONE | 2a67fa6e, 733fe571 (7 of 7) | none (status line fixed) |
| 9 native picture tools | PARTIAL | P1 kittyout.c, P2 rig 32 | P3 fetch, P4 nnn preview, P5 chafa (later by owner) |
| 10 screens + dctelnet | PARTIAL | P1-P2, P4-P10 ticked | P3 cycle-exact direct path proof |
| 11 Amarchy plans | see below | | |

Amarchy: status line set to done (all boxes ticked, majority carry hashes): menu-minimal, app-index, group-drawer-icon.
Not changed, ticked but few or no hashes: commodity (3 of 5 hashed), gui-aa-font (2 of 6; status says not started), menu-aspect (0 of 8; status open).
Plans with open boxes (real work left): dopus-integration 13, own-prefs 25, p1a 17, p1b 31, lx-logo 14, p3 8, p4 8, menu-in-terminal 7, flat-ui-patch 4, p2 3.
Already correct: gui-warm done, autotile done.
