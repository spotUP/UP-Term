#!/usr/bin/env python3
"""test_amarchy_edit_applies_sentinel.py -- the Theme Editor publishes gt_dos_edit_applies as the AmigaDOS global
AMARCHY_EDIT_APPLIES after Save and after Apply to Desktop (leaf.c theme_action), so a rig case (TE-9.1) can read it.

  python3 -I tests/test_amarchy_edit_applies_sentinel.py

leaf.c is Amiga-only code, so this checks the text of theme_action: GTA_SAVE and GTA_APPLY each reach am_sentinel with the
counter. Exit 1 on a missing link."""
import pathlib, re, sys

src = (pathlib.Path(__file__).resolve().parents[1] / 'amarchy' / 'leaf.c').read_text()
m = re.search(r'^static int theme_action\(.*?^\}', src, re.S | re.M)
if not m:
    sys.exit('FAIL: leaf.c has no theme_action')
body = m.group(0)
bad = 0
for case in ('GTA_SAVE', 'GTA_APPLY'):
    cm = re.search(r'case ' + case + r':(.*?)break;', body, re.S)
    if not cm or not re.search(r'am_sentinel\("AMARCHY_EDIT_APPLIES",\s*\(long\)gt_dos_edit_applies\)', cm.group(1)):
        print('FAIL: theme_action %s does not publish AMARCHY_EDIT_APPLIES' % case)
        bad = 1
if not bad:
    print('[OK] theme_action publishes AMARCHY_EDIT_APPLIES after Save and Apply')
sys.exit(bad)
