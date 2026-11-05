---
date: 2026-11-05
session: vtcon-ee
topic: s57 rig 96 owner bugs: UPDemo exit fixed, drag freeze writer unproven
status: open
---

# vtcon-ee s57: UPDemo exit fix, drag-freeze hunt

## Done (all on origin main)
- ef63c2a9: UPDemo keeps the cursor after a main-screen scene (UPDEMO15-EXIT-PROMPT-TOP, fix a83c6d8e). play() sent ESC[r after main-screen scenes; DECSTBM homed the cursor. Host test the_prompt_after_the_demo_comes_below_the_main_screen_text.
- 86bffc60: host test direct_plane_writes_stay_inside_the_bitmap_at_its_bottom_edge (tests/test_render.c, guard rows in tests/render_host/gfx_fake.c). Handler direct-plane writes are NOT the drag writer.
- 3ef718ec: tests/amiga/copcheck.c and rawdrag.c in CONPROBES with stack cookie; "Hunt attempt 3" in thoughts/shared/research/s57-evidence/rig96-drag-freeze.md.

## Open, in order
1. RIG96-DRAG-COPPER (todos.md): run hunt attempt 4 on a QUIET host only (attempt 3 on rig 131 starved: rigs 96/127/129 + pre-push hook). Recipe in rig96-drag-freeze.md "Hunt attempt 3" / next run: debug fs-uae (`--no-jit --debugger`), send memwatch together with the first "g" at the Kickstart stop, watch the bitmap END (0x2BD40), not LOFlist (its offset differs per rig: 0x168 on rig 96, 0x11A0 on rig 131). Drag NewShell XCON:0/7/640/249 with RAM:rawdrag dy<0, SolidWin running then stopped. Opus agent. Candidates: SolidWin, 3.2 Intuition frame redraw.
2. Owner eye check on rig 96 rebuilt from main (full install, --keep, --idle-minutes 0): UPDemo 15 exit prompt below text. Drag recheck only after item 1 lands a fix (or confirm still reproducible).
3. Item 3 of the 2026-11-04 handoff: wait for "AMARCHY READY <commit>" from vtcon-c1 (3 Amarchy blockers), then re-pin repos.lock, kit build + dist checks; upload needs owner OK.
4. INSTALLER-RIG-REBOOT: close only after a clean fresh-all. Attempt 3 fresh-all on rig 131 (origin/main fdb5e99f) failed check 34 (AMARCHY port: no port) after wasabid did not answer 15 s post-reboot under host load; rig 125 same. Not clean.
5. S57-LOCALE-GADGET-BLACK / XCON title black square: owned by vtcon-c1.

## Constraints
- Never touch rigs 1, 66, 68, 70, 91, 121, 122, 123, 127, 128 without owner/their session. Rig 96 is owner's; logwatch worktree /Users/spot/Code/UP-Term/vtcon-s57-rig96b (branch s57-rig96b) stays while rig 96 is up.
- Do not touch worktrees agent-a902c663e11eb5541, agent-a4a8bc09e147ec27a, MusicPlayer folders 2461f1e9, d530c48f.
- Leave thoughts/shared/research/s31-evidence/installer-check10.md modification alone (not ours).
