# FLAT-TITLE-ACCENT + inactive title dim: hook route (s28)

Chosen: the existing graphics.library Text patch (flat_tx_c in amarchy/flat.c). Intuition draws a window title with one
Text call on the window's border rastport. flat_title_decide (flat_core.c, pure) recognises it: flat on and not held,
rastport layer belongs to a window, the string is a prefix of Window->Title, pen y (cp_y) inside BorderTop. Then:
- active (WFLG_WINDOWACTIVE): paint the band (0,0)-(Width-1,BorderTop-1) in pen_outline (the theme accent pen the
  existing pen reader already puts in flat_state) with the saved RectFill, then draw the title with the saved Text in
  pen_ptext (dark on accent, same pen as a pressed button label). Side borders are untouched (below BorderTop).
- inactive: saved Text in flat_dimtext_pen (pen_dimtext = DK_MUTED = theme dark_foreground).
Pens are reused from flat_state (pen_outline, pen_ptext, pen_dimtext): no third theme reader, no new ObtainPen, FILLPEN
unchanged. Every saved call keeps the __reg("a6") GfxBase argument (s28 FLAT-ON crash rule).

Rejected:
- RectFill hook on the ROM title fill: the active fill covers title bar AND side borders in rects not documented for
  3.1/3.2; matching them is a guess, and an untitled-window fill would also need the same text redraw anyway.
- Changing FILLPEN: drives GadTools selection fills (owner constraint).
- Changing TEXTPEN/FILLTEXTPEN: dims or recolours all body text.
- Frame class (frameiclass/sysiclass) hook: window title is not an image, sysiclass draws only gadget images.

Known limit: a window without a title string draws no Text, so it gets no band. Order assumption (border, title text,
then gadgets) is checked by the rig pixel read.
