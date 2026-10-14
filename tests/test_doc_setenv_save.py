#!/usr/bin/env python3
"""Docs and installer texts never tell the user `SetEnv SAVE name value` without saying it is OS 3.2 and later.
The SAVE keyword exists from dos.library 47 (OS 3.2); on OS 3.1 the same line sets a variable called SAVE
(rig-proven 2026-10-14). The 3.1 and 3.2 form is `SetEnv NAME value` then `Copy ENV:NAME ENVARC:`.
Scans this checkout's user-facing text (dist/, tools/nas/skills, top-level *.md) and, when present, amiga-pi's
(*.md, skills/); the thoughts/ ledger is history and is left out."""
import os, pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
PI = pathlib.Path(os.environ.get('AMIGA_PI_DIR') or (pathlib.Path(os.environ.get('UPTERM_ROOT', ROOT.parent)) / 'amiga-pi'))
BAD = re.compile(r'set\s*env\s+(?:-\w+\s+)?save\b', re.I)
LABEL = re.compile(r'3\.2')


def text_files():
    roots = [(ROOT, ['dist', 'tools/nas/skills']), (PI, ['skills'])]
    for base, subs in roots:
        for s in subs:
            for p in sorted((base / s).rglob('*')) if (base / s).is_dir() else []:
                if p.is_file() and p.suffix not in ('.png', '.lha', '.info', '.bin', '.pcf', '.pfs'):
                    yield p
        for p in sorted(base.glob('*.md')):
            yield p


def offenders(files):
    out = []
    for p in files:
        try:
            lines = p.read_text(encoding='latin-1').splitlines()
        except OSError:
            continue
        for n, l in enumerate(lines, 1):
            if BAD.search(l) and not LABEL.search(l):
                out.append(f'{p}:{n}: {l.strip()[:100]}')
    return out


class SetEnvSave(unittest.TestCase):
    def test_lint_flags_unlabelled_save(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / 'x.txt'
            f.write_text('SetEnv SAVE KEY v\nSetenv save KEY v (OS 3.2 and later)\nSetEnv KEY v\n')
            self.assertEqual(len(offenders([f])), 1)

    def test_no_unlabelled_setenv_save_in_docs(self):
        self.assertEqual(offenders(text_files()), [], 'SetEnv SAVE on OS 3.1 sets a variable called SAVE; '
                         'use SetEnv NAME value + Copy ENV:NAME ENVARC:, or label the line OS 3.2 and later')


if __name__ == '__main__':
    unittest.main()
