# aminet util/sys/layers (Layers V45) vs rounded window corners

Verdict: NO. No shape call; it is the V45 layers that 3.2 already carries. Does not shrink the 6-10 week estimate meaningfully.

## Evidence (archive layers.lha 210610 B, readme 7553 B, aminet.net/util/sys/layers)
- Author Thomas Richter (thor@math.tu-berlin.de). "Short: Layers Library V45 for Os 3.9", Version 45.27, "Requires: Os 3.1", m68k. strings: `layers.library 45.27 (12.12.2014)`. Binary 13748 B. Readme dated Dec 2014.
- Load: Workbench/LoadModule (own ROM-module loader) line `LoadModule LIBS:layers.library` above SetPatch in Startup-Sequence. Readme: "Layers V45 is based on the V40 ROM version of layers" -> classic ClipRect layout, ROM graphics. Needs rtg.library (40.4029) shipped for P96 users; "LayersAntiHack" for CGX.
- NDK/fd/layers_lib.fd (bias 30, 35 public + 2 private AllocClipRect/FreeClipRect). Counted from the fd order: InstallClipRegion -150, InstallLayerHook -174, InstallLayerInfoHook -180, SortLayerCR -186, DoHookClipRects -192, LayerOccluded -222, HideLayer -228, ShowLayer -234, SetLayerInfoBounds -240. Nothing beyond -240.
- New in V45 (readme): LayerOccluded, HideLayer/ShowLayer (hide whole layer), SetLayerInfoBounds (Layer_Info clip rect, "clippy" demo). grep shape/region in autodocs: only InstallClipRegion (draw clipping, transparent user region; no visibility change).
- Slots: the four V45 slots -222..-240 ARE the same four functions 3.2 V45+ layers/Intuition use (same author line, same names). No clash because identical; 3.2 layers is the Richter line (3.2 contents not read here; from the earlier spike: 3.2 Intuition calls these four). So 3.2 already has this; installing it on 3.2 would downgrade.
- Source: NOT shipped for the library. Only Examples (AmiSnap.c, clippy.c) and NDK includes/autodocs. NOTES/layers-internals.readme documents ClipRect/bounds (`floor(MinX/16)*16` offset for obscured ClipRects) - useful as documentation of the convention ROM graphics reads.
- License: closed binary; "publication agreed upon by Hyperion; free for owners of the classic AmigaOs"; thanks to Ben Hermans for permission to redistribute. Not an open licence: do not commit the binary to the public repo; link to Aminet instead.

## Answers
1. See above: Richter, 45.27, needs 3.1+, LoadModule, classic ClipRects on ROM graphics.
2. No shape / visibility-shaping call. Only whole-layer HideLayer(-228). 3.2 already has it.
3. No source for library; redistribution restricted to classic OS owners.
4. Rounded corners: not obtainable from it on 3.1 or 3.2. Its only value: on 3.1, installing it gives the 4 V45 slots (-222..-240), which makes a hyperlayers-style base's slot layout match 3.2 (so one shape call at -246 could serve both). Also its internals notes confirm the ROM-graphics ClipRect convention that was the "biggest unknown" in the spike, partly de-risking the cheapest test. No code to build on, so the 6-10 week estimate stands (maybe trim days, not weeks).
