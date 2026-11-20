#!/usr/bin/env python3
"""test_patina_put_skips_unchanged.py -- dk_put feeds no editor when its bytes equal the ENV:Sys file (S82-SKIP-UNCHANGED);
  the start apply persists its writes to ENVARC: (S82-START-PERSIST).

  python3 -I tests/test_patina_put_skips_unchanged.py

IPrefs resets the Workbench screen on every USE of its watched prefs, identical bytes too, and a foreign window (Directory Opus)
then holds its System Request open. patina/desk_dos.c is Amiga-only code no host binary links, so this checks the structure:
dk_put compares the work copy with ENV:Sys/<same name> through dk_files_equal and returns before dk_editor when they match. Exit 1 otherwise."""
import pathlib, re, sys

SRC = (pathlib.Path(__file__).resolve().parents[1] / 'patina' / 'desk_dos.c').read_text()
m = re.search(r'^static int dk_put\(.*?^\}', SRC, re.S | re.M)
if not m:
    sys.exit('FAIL: desk_dos.c has no dk_put')
body = m.group(0)
eq = re.search(r'if \(dk_files_equal\(work, env\)\) \{[^}]*return 1;', body)
ed = body.find('dk_editor(')
if 'ENV:Sys/' not in body or not eq or ed < 0 or eq.start() > ed:
    sys.exit('FAIL: dk_put feeds the editor without first skipping bytes equal to ENV:Sys')
print('PASS: dk_put skips the editor when the bytes equal ENV:Sys')

# S82-START-PERSIST: the start apply copies what it wrote to ENVARC: (skipping equal bytes), so the next boot plans 0
p = re.search(r'^static void dk_start_persist\(.*?^\}', SRC, re.S | re.M)
a = re.search(r'^int desk_dos_start_apply\(void\).*?^\}', SRC, re.S | re.M)
if not p or not a or 'dk_start_persist(plan & ~DK_START_ARC, flat);' not in a.group(0):
    sys.exit('FAIL: desk_dos_start_apply does not persist its writes to ENVARC: (dk_start_persist)')
pb = p.group(0)
if 'desk_start_persist_mask(' not in pb or 'dk_pending_mask()' not in pb or not (0 <= pb.find('dk_files_equal(') < pb.find('dk_stream_copy(')):
    sys.exit('FAIL: dk_start_persist must skip a pending pick and equal bytes before copying ENV: to ENVARC:')
print('PASS: the start apply persists its writes to ENVARC:, equal bytes skipped')
