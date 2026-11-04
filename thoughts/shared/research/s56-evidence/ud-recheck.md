# s56 UPDemo recheck (rig 104, main 8ccdbb7f, fresh-all install PASS, window XCON 760x440, UPDemo started with `UPDemo 15`)
## UD2 fjord photo scene (Demo 15/35 "Kitty graphics: an animation the terminal plays"): FAIL (first look)
Shot ~/Desktop/upterm-shots/s56-104/ud2-a.png, taken ~14 s after Return, status line reads "Demo 15/35", heading
"a globe turns over a fjord photo", text "a photo (z=-1), 44 frames, their gaps, and one loop command." and "The Earth turns over the fjord:".
Earth drawn at left (~x 65-185, y 85-200) on black; canvas right of the earth (x 250-740, y 160-360 crop) has 720 non-dark px of 96400 = text only.
No fjord photo anywhere around or behind the earth. UP-Term:share/updemo has fjord.png 32970, earth.anim 983803, photo.png, kanim-stream.bin.
Demo auto-advances (scene 17 by ~17 s).
## UD3 kitty placement scene (Demo 19, `UPDemo 19`): PASS
Shots ud3-1.png .. ud3-8.png (sheet ud3-sheet.png). ud3-1 shows all five step lines done (p3 deleted at step 4, p2 moved at step 3).
Coloured-pixel map (max-min channel > 50, y 88-190, x 24-170): exactly three circles remain (p1 at x24-72 y88-136, its child x84-108,
p2 moved to x108-150 y136-185); p2's old spot (same row as p1, x ~104-152, y 88-136) has 0 coloured px, and p3's old spot (x ~ 200) has 0.
No leftover pixels where the images were. Not seen: the transient before-move frame (only the end state measured).
## UD6 glyphs scene (Demo 21, auto-advanced from 19/20): PASS
Shot ud3-6.png (crop ud6-crop6.png, 2x). Powerline row: blue "user", green " main", green "ok" with arrow separators drawn, branch glyph drawn.
Nerd Font row: 20 icons in two rows (folder, house, star, code, bug, lock, gear, bolt, database, clock, book / branch, git-branch, V, github, terminal, plus, undo, prompt, nodes) all
as glyph shapes; zero "?" boxes. Emoji row 4 coloured emoji. Box row: two outlined rectangles + one block glyph (box drawing, as intended). Judged from 2x crop pixels.
## UD5 splits scene (Demo 8/35, `UPDemo 8`): FAIL (keys end up in the new pane)
Shots ud5-1..5.png (sheet ud5-sheet.png), ud5-6pre.png, ud5-7post.png, crop ud5-prompt.png (3x).
The split happened (ud5-1: left pane "Demo 8/35 Split panes", "Sent /split right: a new pane with its own shell opens beside this one."; right pane = a new vsh with the
banner and prompt). ud5-2: "Sent /pane zoom ... again: both panes are back." and "Type exit in the new pane to close it." Then the left (demo) pane shows
"UP-Term demo: make the window bigger / 76 x 20 needed. Q quits." (the split pane is narrower than 76 columns).
Key test: sent `q` through ami.py type. Expected: demo pane quits. Measured (ud5-prompt.png, right pane prompt row, 3x crop): before "SYS: % [cursor]",
after "SYS: % q[u]it" = the `q` was typed into the NEW shell pane (autosuggest "quit" in grey), the demo pane was unchanged (still shows the "make the window bigger" text).
So after the scene's /split right + /pane focus (+ zoom/unzoom + splits_leave) the keys go to the new pane, not the demo pane. Not isolated: I did not send a key in the
~0.8-2.4 s window between /pane focus and /pane zoom (scene runs 6 s), so whether /pane focus itself works but splits_leave/unzoom moves focus back to the new pane is UNVERIFIED.
Secondary finding: the demo pane is too narrow for the demo (76x20) after the split, so the demo shows the "make the window bigger" message instead of continuing.
