#!/usr/bin/env python3
"""dist/keymap -> the key list of the README's AMARCHY section (and the man page's).

One line per shipped row, in file order: "Left Amiga + Shift + T   New tab". A
`super+1..9` row is one line. The README holds the output between the lines
"  [keys]" and "  [end keys]"; tests/test_amarchy_docs.py compares the two, so a
row added to dist/keymap without a README line fails `make dist-check`.
Usage: keymap_doc.py [dist/keymap]"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORDS = {'space': 'Space', 'return': 'Return', 'tab': 'Tab', 'esc': 'Esc', 'help': 'Help', 'del': 'Del',
         'backspace': 'Backspace', 'up': 'Up', 'down': 'Down', 'left': 'Left', 'right': 'Right',
         'comma': ',', 'period': '.', 'minus': '-', 'plus': '+'}
MODS = {'super': 'Left Amiga', 'shift': 'Shift', 'alt': 'Alt', 'rshift': 'Right Shift'}


def key_name(k):
    if k in WORDS:
        return WORDS[k]
    if re.fullmatch(r'f\d+', k):
        return k.upper()
    return k.upper()


def rows(path=os.path.join(ROOT, 'dist', 'keymap')):
    """[(chord text, label)] of the [bind] section's active rows."""
    out = []
    section = ''
    for line in open(path, encoding='latin-1'):
        line = line.strip()
        if not line or line.startswith(';'):
            continue
        if line.startswith('['):
            section = line
            continue
        if section != '[bind]':
            continue
        m = re.match(r'(\S+)\s*=\s*[^;]*;\s*([^|]*?)\s*\|', line)
        if not m:
            continue
        parts = m.group(1).split('+')
        label = m.group(2)
        mods = [MODS[p] for p in parts[:-1]]
        key = parts[-1]
        if key == '1..9':
            key = '1 to 9'
            label = label.replace('%n', 'N')
        else:
            key = key_name(key)
        out.append((' + '.join(mods + [key]), label))
    return out


def text(path=os.path.join(ROOT, 'dist', 'keymap')):
    r = rows(path)
    w = max(len(c) for c, _ in r)
    return ''.join('  %-*s  %s\n' % (w, c, l) for c, l in r)


if __name__ == '__main__':
    sys.stdout.write(text(*sys.argv[1:]))
