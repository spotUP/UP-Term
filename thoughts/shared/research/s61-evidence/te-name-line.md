---
date: 2026-11-06
topic: Duplicate Theme / Save As wrote the source name in the name line
status: fixed on branch te-name-line (local commit), host gthemes green, m68k rc=0
---
Cause: amarchy/gthemes_core.c gt_save called gt_model_save_text(m,...), whose writer emits m->name. gt_duplicate loads the source into g_smodel (name = source), so the file under the new name held the source name. Save As likewise carries whatever name the model had.
Invariant: theme identity is the file name.
Fix shape: the writer body became static model_save_named(m, name, out, cap); gt_model_save_text (public, unchanged signature) calls it with m->name; gt_save calls it with its target `name`. One place, no cast, model untouched.
Test: tests/test_gthemes.c near Probe Copy: copy file holds name = "Probe Copy", not "Probe"; Save As of a model named Probe Copy to Other Copy holds Other Copy.
Before fix: FAIL test_gthemes.c:2833/2834/2838/2839 -> [FAIL] gthemes: 6953 checks, 4 failed
After fix: [OK] gthemes: 6953 checks, 0 failed
Pin change: te3_save round-trip compared the saved bytes with gt_model_save_text of a model named from the parsed file; it encoded the bug, so the test now sets mod.name = "Mine" first.
Other paths: gthemes_dos.c:197 gt_dos_preview_model uses gt_model_save_text for a live preview only (not stored), uses m->name, fine. Export/Import go through gt_save's put_text/theme_text copy of raw file text (~1634-1657), not the writer; Export copies the file bytes, so it carries whatever the file holds (files already saved with the stale name stay stale until re-saved).
