#!/usr/bin/env python3
"""icat's diagnostics go to the error stream (R2b). A JPEG with no jpeg datatype class made icat print
"icat: no datatype reads ..." on the OUTPUT handle, so `icat x.jpg 2>file` showed an empty stderr and the
command looked silent. Source-level, as tests/test_qmark_commands.py: icat is Amiga-only code. Pins that the
error stream helper is pr_CES, that every error line of main goes through it, and that the decode failure
path (every non-PNG file no class reads) reaches the "no datatype reads" line with a non-zero return."""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(ROOT, 'pic', 'icat.c')).read()
fails = 0

def check(ok, what):
    global fails
    print('%s %s' % ('[OK]' if ok else '[FAIL]', what))
    if not ok:
        fails += 1

h = re.search(r'static void esay\(const char \*s\)\s*\{(.*?)\n\}', src, re.S)
check(h is not None and 'pr_CES' in h.group(1) and 'Write(' in h.group(1), 'esay writes to the process error stream pr_CES')

body = src[src.find('int main('):]
# every text fragment of an error line, through esay
for frag in ['icat: PLACE is x,y', 'icat: FILE or CLEAR', 'icat: cannot read ', 'icat: no datatype reads ',
             "(picture.datatype and the format's class must be installed"]:
    m = re.search(r'(\w+)\("[^"]*' + re.escape(frag), body)
    check(m is not None and m.group(1) == 'esay', 'error text %r goes through esay' % frag)

# the name in the two "name" error lines is an esay argument too
for lead in ['icat: cannot read ', 'icat: no datatype reads ']:
    m = re.search(re.escape(lead) + r'"\);\s*(\w+)\(name\)', body)
    check(m is not None and m.group(1) == 'esay', 'file name after %r goes through esay' % lead)

# a file no datatype reads (JPEG without its class) ends in that line and return 10, not in the picture path
m = re.search(r'if \(!pix\) \{(.*?)return 10;', body, re.S)
check(m is not None and 'no datatype reads' in m.group(1), 'a failed decode prints "no datatype reads" and returns 10')

print('%d failures' % fails)
sys.exit(1 if fails else 0)
