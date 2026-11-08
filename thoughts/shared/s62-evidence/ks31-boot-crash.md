
## s63 part 8 (private stack fix)

Fix: the c2 clone path (c2_clone -> flat_clone_draw_in) now runs on a private 16384-byte
MEMF_PUBLIC stack through the asm helper flat_on_stack (amarchy/flat_stk.s, exec StackSwap),
while cl_ss is held. The stack is allocated in main before the hooks install and freed after
hook/rf/tx removal under cl_ss. If allocation failed, the clone falls back to the block glyph
(cnostk counter); the clone never runs on the caller's stack. The refuse guard is removed.

Binary: build/amiga/amarchy-flat from branch s63-ks31-stack, md5 6097ecbd8cacd282308edfab5fa210e0
(checked on the rig disk each boot). Rig 97: KS 3.1, 060 --max, DOpus installed.

| boot | result | md5 on rig | clone draws (S2) | fallback | cnostk | cmin (IPrefs) |
|------|--------|------------|------------------|----------|--------|---------------|
| 1 | Workbench + Amarchy | 6097ecbd | 63 | 0 | 0 | 1180 |
| 2 | Workbench + Amarchy | 6097ecbd | 57 | 0 | 0 | 1180 |
| 3 | Workbench + Amarchy | 6097ecbd | see sp-a log | 0 | 0 | 1180 |
| 4 | Workbench + Amarchy | 6097ecbd | see sp-a log | 0 | 0 | 1180 |
| 5 | Workbench + Amarchy | 6097ecbd | see sp-a log | 0 | 0 | 1180 |

Boot 1 S2 tail: `lskip=2 cmin=1180 ctask=<IPrefs> chw=16384 cstk=16384 cnostk=0 clone draws=63 fallback=0`
Boot 2 S2 tail: `lskip=2 cmin=1180 ctask=<IPrefs> chw=16384 cstk=16384 cnostk=0 clone draws=57 fallback=0`
(boots 3-5 STATUS lines show the same cmin/chw/cstk/cnostk with draws 53/57/67 across S1/S2 pairs.)
cmin=1180 on IPrefs confirms the cause: the caller stack had about 1.2 KB free, now unused by the clone.

OPEN: chw=16384 equals the whole stack on every boot. The high-water scan walks up from the
stack bottom until the first byte that is not 0xA5, so chw=16384 means byte 0 was already
changed. Either the clone really used the whole 16 KB (no crash seen, but unproven), or the
scan or fill is wrong. Not resolved here (boot-fail rule: no more patches). Next step: dump
the stack bytes, or fill with a second pattern and scan from the top.

KS 3.2 (rig 98, --os 32, same pinned binary started by hand): flat=on, `clone draws=14448`
after opening Palette and Font prefs, so amarchy-flat still draws clones on 3.2. cmin/chw not captured.

Tests: tests/test_flat_clone_stack.py (make test ONLY=flat_clone_stack): fails 3 of 4 on main's
flat.c, passes 4 of 4 on this branch. flat_core, flat_role_core, flat_single_instance pass.
make flat-budget PASS.
