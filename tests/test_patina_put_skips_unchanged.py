#!/usr/bin/env python3
"""test_patina_put_skips_unchanged.py -- dk_put feeds no editor when its bytes equal the ENV:Sys file (S82-SKIP-UNCHANGED).

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
