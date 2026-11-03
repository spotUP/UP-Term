---
date: 2026-11-03
topic: UP-Term demo tour versus features landed since October 2026 (UPDEMO-NEW-FEATURES)
tags: [updemo, demo, kitty, sixel, themes, amarchy, gap]
status: final
---

# UP-Term demo tour: feature gap

Method: scene table read from `demo/updemo.c` (`scenes[]`, line 1944); features from
`git log --since=2026-10-01`, plus the kitty plan checklist
(`thoughts/shared/plans/2026-10-19-kitty-graphics.md`, status line). Not verified on a rig; this is a source read.

## Current scenes (25)

text (styles, double-size lines, box drawing); styles; colours (16/256/24-bit); character sets;
unicode; scroll regions; tmux; splits (Window menu keys); vsh; completion (Unix and KingCON);
themes (12 themes from `demo/tour_themes.inc`, live via OSC 4/10/11; the repo ships 112 theme
.conf files); mouse; links (OSC 8, OSC 9); sixel (a drawn Amiga ball, generated in the demo
code, no media file); synchronized output; resize (DECCOLM); reflow; copper bars; plasma; fire;
rotozoomer; vector cubes; sine scroller; palette cycling; the end.

The demo has no kitty graphics code at all (grep for kitty, icat, `ESC _ G`: nothing) and ships
no image or animation file. Its only picture scene is the generated sixel ball.

## Gap table

| Feature | Landed | In demo | How a scene could show it | Media |
|---|---|---|---|---|
| Kitty graphics, PNG/RGB/RGBA via APC G (K1-K18) | 2026-10-19 plan, K1-K18 done | no | Scene "pictures": demo emits a kitty RGBA image generated in code (gradient, checker) at the cursor | no (generated) |
| Kitty animation, frame clock (K34-K36) | 55abb007, f9ca3bee, 92d580cf, c63dcb4f (2026-11-02/03) | no | Generated multi-frame animation (spinning ball) with a=a control; profile key image-animation | no (generated frames) or small PNG set |
| Kitty truecolor RGB pictures on deep screens, alpha blend (K37-K41) | c1518f39, ceacf685, 2db445e0, b0a0a42a (2026-11-02) | no | Translucent RGBA picture over coloured text; shows blend on RTG, 8-bit fallback on AGA | no |
| Kitty z-index, negative z under text, z>=0 keeps cells (K40d) | 6c4acd94, 85c5c1f7 | no | Text drawn over and under the same picture; label on top of a gradient | no |
| Kitty placement controls: X/Y pixel offset, relative placements P/Q/H/V, c/r sizing | 9d69f8d2 (K23), c0a4a712 (K29) | no | Several pictures positioned relative to a parent image | no |
| Kitty virtual placements and unicode placeholder cells, under tmux (K31-K33) | 4f9aa380, be550040, fe4af6c9 | no | Placeholder image inside a tmux-style pane region (ties to the tmux scene) | no |
| Kitty file transfer t=f / t=t via host file reader (K25-K28) | 75a8c755 | no | Needs a file on disk; better shown by a manual icat step, not a scripted scene | yes (a file) |
| icat (PNG f=100 true colour on RTG; IFF ILBM; JPEG with V43 picture.datatype) | 1ed14b5f, a375f996, 71ce8edc | no | Final "pictures" scene runs `icat` on a bundled PNG and an IFF; demo would ship the file | yes (PNG, IFF) |
| Antialiased glyph edges blended with dithered tile pen; 4/8-shade ramp (R6) | b829462f, 2eadbc5d (2026-11-01) | partly (outline fonts named on text scene) | "Text quality" scene: large outline text over dithered colour background | no |
| Grid-fit of box, block, powerline and Nerd Font icon glyphs on tall pixels | 94075481, c376883e | no | Powerline prompt and Nerd icons row next to box drawing | no |
| Unifont fallback for all BMP characters; emoji boxes, outline glyphs beyond BMP | 5b702c67, 6de5d18c (10-04) | partly (unicode scene; Unifont mentioned in scroller text) | Add CJK/Arabic/symbol rows | no |
| Tabs: several sessions in one window, each its own profile | 3ec0da48 (10-03) | text only (named in a row, no scene) | Scene that opens a second tab via the menu escape | no |
| Native splits (Window menu, snap/focus/float/swap keys) | 10-19 plan, 2026-10-29 keybinds | yes (splits scene is text; swap/float/focus keys not shown) | Extend the splits scene with the new keys | no |
| Settings menu, live Font/Theme/Profile/Scrollback; slash commands for every setting | 768a037c .. 2cd46020 (10-02/03) | no (themes scene uses OSC only) | Scene that types `/theme NAME` and `/font-fallback` at the prompt | no |
| Themes from 112 shipped .conf files and live truecolour theme switch (Amarchy) | 2026-10-28 | partly (12 themes) | Grow the cycle toward the shipped set; show Catppuccin, Dracula, Everforest | no |
| Amarchy desktop: flat UI, theme palette/pointer/font at start, autotile, desktop theme default on | 295ef667, e7e321e2, cf0d408e, 177b000e | no | Cannot run inside a terminal scene; belongs in a separate screenshot or video, or a caption slide | yes (screenshots) |
| Amarchy menu with deep search; screen capture to ILBM | 2026-10-29 | no | Same as above: slide with screenshot | yes |
| mdv, Markdown in the window; syntax highlight | 62909ee1 (10-04) | no | Scene piping a short Markdown text through mdv | no |
| Claude Code TUI client (C:Claude) in the window | ee1d5af8 (10-05) | no | Static recorded screen or a caption; needs network | yes (recording) |
| Reflow on resize, synchronized output, hyperlinks, notifications | earlier | yes | none | no |
| Mouse reporting, tmux, completion, Unix userland (vsh) | earlier | yes | none | no |

## Top gaps by wow factor

1. Kitty animation (K34-K36): a moving picture inside a text window.
2. Kitty truecolour pictures with alpha blend on deep screens (K37-K41).
3. Kitty z-index: text drawn over and under a picture.
4. icat with a bundled PNG/IFF (needs media files shipped with the demo).
5. Kitty placeholders and pictures inside tmux panes (K31-K33).
6. Crisp glyphs: antialias blend over dither, grid-fit powerline/Nerd icons.
7. Live `/theme` and the Settings menu, with more of the 112 themes.
8. Amarchy desktop (flat UI, themes, autotile) as screenshot slides.

## Notes for the implementer

- The demo core is portable C89 with no OS calls (`demo/updemo.c` header); kitty scenes can emit
  APC G through the existing `ps()` output path. Raw RGBA (`f=32`) needs no decoder, so
  generated pictures need no media file; only icat and file transfer need bundled files.
- Machine limits: truecolour pictures need a deep (RTG) screen; on AGA the 8-bit fallback shows
  instead. A caption should say which, as the sixel scene does.
- Scene count drives the caption "n of N" and `UPDemo BENCH`; adding scenes must keep
  `updemo_rig.py` in step (not read here).
