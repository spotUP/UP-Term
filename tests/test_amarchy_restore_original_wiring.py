"""TE-10.9 wiring guard: the leaf dispatch must run Restore Original as the pure reset (gt_se_action with GTA_SCR_RESTORE)
and then the existing save (gt_dos_screens_save), in one action, so the rule files, the on/off key and ipd_rules_changed follow."""
import os, re, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
leaf = open(os.path.join(root, 'amarchy', 'leaf.c'), encoding='latin-1').read()
fails = []
m = re.search(r'case\s+GTA_SCR_RESTORE\s*:(.*?)(?:\n\s*case\s|\n\s*\}\s*\n\s*if\s*\(!ok\))', leaf, re.S)
if not m:
    fails.append('leaf.c has no case GTA_SCR_RESTORE')
else:
    body = m.group(1)
    a = body.find('gt_se_action(GTA_SCR_RESTORE')
    b = body.find('gt_dos_screens_save(')
    if a < 0:
        fails.append('GTA_SCR_RESTORE does not call gt_se_action(GTA_SCR_RESTORE, ...)')
    if b < 0:
        fails.append('GTA_SCR_RESTORE does not call gt_dos_screens_save')
    if a >= 0 and b >= 0 and a > b:
        fails.append('gt_dos_screens_save runs before the reset')
core = open(os.path.join(root, 'amarchy', 'gthemes_core.c'), encoding='latin-1').read()
if '"Restore Original"' not in core or 'GTA_SCR_RESTORE' not in core:
    fails.append('gthemes_core.c lacks the Restore Original row')
if fails:
    print('FAIL: ' + '; '.join(fails))
    sys.exit(1)
print('ok')
