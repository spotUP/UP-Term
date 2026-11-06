#!/usr/bin/env python3
"""test_amarchy_rows_buffer.py -- no caller passes a literal-sized local buffer to mn_rows_join (TE-ROWS-255-CUT).

  python3 -I tests/test_amarchy_rows_buffer.py

AMARCHY_ROWS was cut at 255 characters because amarchy/gui.c and amarchy/menu.c each joined into a stack
`char x[256]`. The buffer must be MN_ROWS_MAX (menu_core.h) and static (no big Amiga stack frame). Exit 1 otherwise."""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[1] / 'amarchy'
bad = []
for name in ('gui.c', 'menu.c'):
    lines = (ROOT / name).read_text().splitlines()
    calls = [i for i, l in enumerate(lines) if 'mn_rows_join(' in l]
    if not calls:
        bad.append('%s: no mn_rows_join call' % name)
    for i in calls:
        m = re.search(r'mn_rows_join\([^;]*,\s*(\w+)\s*,\s*sizeof\s+\1\s*\)', lines[i])
        if not m:
            bad.append('%s:%d: buffer/size not "buf, sizeof buf": %s' % (name, i + 1, lines[i].strip()))
            continue
        buf = m.group(1)
        for j in range(max(0, i - 6), i):
            d = re.search(r'\bchar\s+%s\s*\[([^\]]*)\]' % buf, lines[j])
            if d:
                if d.group(1).strip() != 'MN_ROWS_MAX' or not re.search(r'\bstatic\b', lines[j]):
                    bad.append('%s:%d: %s' % (name, j + 1, lines[j].strip()))
                break
        else:
            bad.append('%s:%d: no declaration of %s found above the call' % (name, i + 1, buf))
if bad:
    sys.exit('FAIL: ' + '\nFAIL: '.join(bad))
print('PASS: gui.c and menu.c join into a static char [MN_ROWS_MAX]')
