# TE-10.5b1: the per-screen pen holder keeps the plan pens

Signature: `int ip_plan_sync(ip_holder *h, const ip_ops *o, const gt_plan_pen *plan, int n)` (amarchy/iconpen_core.[ch]). Returns the plan pens held after the call.

| plan state | action |
|---|---|
| exclusive, not held | o->obtain(pen, rgb); -1 = skipped, retried next sync |
| held, colour changed | o->set_rgb (same op as the role pens), no release/obtain |
| held, absent or exclusive = 0 | o->release |
| exclusive = 0 | nothing |
| n = 0 | all plan pens released |
| pen held as icon pen or role pen | skipped |

Holder: `pcount`, `ppen[GTE_COUNT]`, `prgb[GTE_COUNT]` (only held entries; capacity = GTE_COUNT, the element count the plan returns at most). `ip_holder_release` releases them (counted in its return); `ip_holder_init` (identity change path) clears them without release.
Reused: the rcount/rpen/rrgb pattern, ip_recolour's set_rgb op, existing release/obtain ops.
Semaphore name bumped to "Amarchy.IconPens.3" (iconpen_dos.h), as TE-10.4 did for ".2"; holder size changed.

RED (body stubbed to return 0): `RUNTEST iconpen rc=2 [FAIL] iconpen: 2416 checks, 29 failed`
GREEN: `RUNTEST iconpen rc=0 [OK] iconpen: 2416 checks, 0 failed`
m68k build (6 Amarchy targets + upprefs): rc=0; tests/test_make_link_lists.py OK.

Open points for b2: caller clears icon/role pen bits from the free mask; ip_plan_sync must be called under the same lock as ip_sync; tests/amiga/iconpens.c reads ipd_rec (layout unchanged there, ip_holder is embedded: re-check offsets on a rig).
