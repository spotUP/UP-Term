#!/usr/bin/env python3
"""ans2utf8.py IN OUT -- an Amiga ANSI (Topaz: Latin-1 bytes) for the xterm
personality (UTF-8): drops the SAUCE record (from the ^Z on), converts the
bytes from Latin-1, and ends with an SGR reset and a new line so the prompt
after it starts clean."""
import sys

def main(src, dst):
    b = open(src, 'rb').read()
    cut = b.find(b'\x1aSAUCE')
    if cut >= 0:
        b = b[:cut]
    text = b.decode('latin-1').rstrip('\r\n') + '\x1b[0m\r\n'
    open(dst, 'wb').write(text.encode('utf-8'))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
