#!/usr/bin/env python3
"""help_man_sync: the GETTING STARTED section of man/amarchy.1 is exactly what tools/gen_amarchy_help.py makes from
dist/amarchy.help, and the generated amhelp.inc carries both sections. A hand edit of either side fails."""
import os
import subprocess
import sys
import tempfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
gen = os.path.join(root, "tools", "gen_amarchy_help.py")
help_, kmap, man = (os.path.join(root, p) for p in ("dist/amarchy.help", "dist/keymap", "man/amarchy.1"))

r = subprocess.run([sys.executable, "-I", gen, "--check", help_, kmap, man], capture_output=True, text=True)
print(r.stdout.strip() or r.stderr.strip())
if r.returncode:
    sys.exit(1)

with tempfile.TemporaryDirectory() as d:
    inc = os.path.join(d, "amhelp.inc")
    subprocess.run([sys.executable, "-I", gen, help_, kmap, inc], check=True)
    text = open(inc).read()
    for name in ("amhelp_welcome", "amhelp_help"):
        if "static const char %s[]" % name not in text:
            print("[FAIL] amhelp.inc lacks %s" % name)
            sys.exit(1)
    if "{key:menu:root}" not in text:
        print("[FAIL] amhelp.inc lost the placeholders")
        sys.exit(1)
print("[OK] amhelp.inc has both sections with placeholders")
