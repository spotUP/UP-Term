# FLAT-31-GEOMETRY: routes to thinner title bars, borders, screen bar (read only, no rig)

Evidence tags: (V) read in a repo file, cited; (U) from memory, unverified.

## Facts in the repo
- (V) NDK vendor/ndk-3.2r4-Include_H/intuition/screens.h:156-164: Screen has BarHeight, BarVBorder, BarHBorder, MenuVBorder, MenuHBorder, WBorTop, WBorLeft, WBorRight, WBorBottom. "The title bar height of a window is calculated from the screen's WBorTop field, plus the font height, plus one." BarHeight is one less than the real bar height.
- (V) Research 2026-10-29-amarchy-flat-ui-patch.md section 9 (FLAT-R1, e77add71): 3.2 (intuition 47.53) stock: bar 10, BorderLeft 4, Top 11, Right 18, Bottom 2. ic_GUIGeometry byte 1 = -1 gives BorderTop 10 (only gain, 1 px); byte 0 negative breaks LockPubScreen; left/right/bottom never move; byte 3 only scales right border/size gadget (18/20/22). On 3.1 (intuition 40.85) all four bytes: no effect at all.
- (V) Section 10 (FLAT-R3): sysiclass Hook is swappable on 3.1 and 3.2 (swap h_SubEntry; 3.1 takes C stack args, 3.2 registers). sysiclass draws close/depth/zoom/size on both.
- (V) Section 7/earlier: replacing window frame code by patching OpenWindow with own border gadgets + WFLG_BORDERLESS was rejected (breaks apps reading BorderTop/Left).
- (V) Birdie/VisualPrefs note: VisualPrefs 1.5k patches Intuition/GadTools/frameiclass/sysiclass/OpenWindow and has a GUI chunk (1672 bytes, layout undocumented); 1.5n "titlebar height via system mechanism" on 3.2 (so VP itself defers to IControl there). Birdie only fills border pixels, no geometry. No local doc says VP 1.5k can shrink borders on 3.1; whether it does is (U).

## Table
| route | works 3.1 | works 3.2 | px title | px border | cost | risk |
|---|---|---|---|---|---|---|
| R1 ic_GUIGeometry (IControl) | no (V, measured) | yes, byte 1 only (V) | 0 / 1 | 0 | low, already measured | low; byte 0 negative breaks the WB screen |
| 1 sysiclass hook | yes (V hook swap) | yes | 0 | 0 | medium | low; hook only repaints inside a box Intuition already sized, cannot change BorderTop (U: size is fixed before the hook sees it) |
| 2a OpenWindow taglist | no tag exists for border size (V: no WA_ border-size tag in intuition.h) | same | 0 | 0 | - | - |
| 2b borderless + own frame | yes | yes | any | any | very high | rejected (V): apps read BorderTop/Left |
| 3 patch OpenScreen(TagList) / rewrite Screen->WBorTop, WBorLeft..., BarHeight after open | probably (U) | partly (U) | up to WBorTop (U: stock about 2, so up to 2) | WBorLeft/Bottom already 4/2: 1-2 | medium: needs a rig probe first | high: bar layer height and window layers are built at open; a post-open edit may desync redraw and clipping; apps copy WBor* |
| 4 smaller screen font (Font prefs, Screen) | yes (U) | yes (U) | 1 per font px (title = WBorTop + YSize + 1, V); topaz 8 is the smallest ROM font, a 7 or 6 px font must be shipped (U) | 0 | low | medium: every app that uses the screen font re-lays out; icon text is a separate font |
| 5 VisualPrefs / Birdie style | VP yes on 3.1 (U, designed for 3.0/3.1) | VP yields to IControl (V) | unknown | unknown | high: reverse the GUI chunk, bundle third-party code | high: patch of five Intuition surfaces, no 3.2 guarantee |

## Recommendation
Fastest honest route: ship 3.2 byte 1 = -1 (1 px, measured) and, for 3.1, run one cheap rig probe of route 3 (own screen, open, poke Screen->WBorTop, open a window, read BorderTop and redraw) before any build; if it desyncs, a 7 px screen font (route 4) is the only 3.1 title gain and the screen bar stays; borders (left 4, right 18, bottom 2) are already near minimal and nothing thins them.
Probe to add: FLAT-31-G1 (rig `--os 31`: print Screen->WBorTop/WBorLeft/WBorRight/WBorBottom/BarHeight of Workbench, then poke on an own screen and open a window).
Not done: nothing measured this session; no binaries downloaded; screen field stock values on 3.1 not read.
