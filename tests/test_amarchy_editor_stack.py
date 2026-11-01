#!/usr/bin/env python3
"""test_amarchy_editor_stack.py -- dk_editor starts the prefs editors with a stack from pl_stack, floor >= 32768.

  python3 -I tests/test_amarchy_editor_stack.py

Under WBStartup a SystemTags child without NP_StackSize gets 4096 bytes. amarchy/desk_dos.c is Amiga-only code no host
binary links, so this checks the structure: the SystemTags call in dk_editor passes NP_StackSize taken from
pl_stack(prog, floor) with floor >= 32768. Exit 1 otherwise."""
import pathlib, re, sys

SRC = (pathlib.Path(__file__).resolve().parents[1] / 'amarchy' / 'desk_dos.c').read_text()
m = re.search(r'^static void dk_editor\(.*?^\}', SRC, re.S | re.M)
if not m:
    sys.exit('FAIL: desk_dos.c has no dk_editor')
c = re.search(r'SystemTags\([^;]*NP_StackSize,\s*pl_stack\(\s*\w+\s*,\s*(\d+)\s*\)', m.group(0))
if not c:
    sys.exit('FAIL: dk_editor SystemTags passes no NP_StackSize from pl_stack')
if int(c.group(1)) < 32768:
    sys.exit('FAIL: dk_editor stack floor %s < 32768' % c.group(1))
print('PASS: dk_editor stack floor %s via pl_stack' % c.group(1))
