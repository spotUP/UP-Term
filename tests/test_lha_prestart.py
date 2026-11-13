"""The packing lha must write archives 7-Zip decodes: lha 1.14i-ac wrote a match into its pre-file ' ' window.

usage: tests/test_lha_prestart.py LHA   (LHA: the binary under test; make test builds build/host/lha)
The input is the v1.1 kit's Files/ixkill (8052 bytes, an Amiga hunk file starting 00 00 03 F3 whose
relocation table is a hash chain past the encoder's limit; byte 6639 is ' ' 00 00 03, which the broken
search matches against the preset window before the file). Tried at -lh5-, -lh6-, -lh7-; skips when 7zz is absent.
"""
import os, random, shutil, subprocess, sys, tempfile

lha = sys.argv[1]
if not shutil.which('7zz'):
    print('[SKIP] lha_prestart: no 7zz')
    sys.exit(0)
data = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures', 'lha_prestart_ixkill.bin'), 'rb').read()
bad = []
with tempfile.TemporaryDirectory() as d:
    open(os.path.join(d, 'f'), 'wb').write(data)
    for m in ('5', '6', '7'):
        a = os.path.join(d, 'a%s.lha' % m)
        subprocess.run([lha, '-aqo' + m, a, 'f'], cwd=d, check=True)
        r = subprocess.run(['7zz', 't', a], capture_output=True, text=True)
        print('lh%s: 7zz t rc %d' % (m, r.returncode))
        if r.returncode:
            bad.append(m)
if bad:
    print('[ERROR] lha_prestart: 7zz t fails for -lh%s-: %s is not the patched lha' % (',-lh'.join(bad), lha))
    sys.exit(1)
print('[OK] lha_prestart')
