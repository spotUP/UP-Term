#!/usr/bin/env python3
"""ans2utf8.py IN OUT -- an Amiga ANSI (Topaz: Latin-1 bytes) for the xterm
personality (UTF-8): drops the SAUCE record (from the ^Z on), converts the
bytes from Latin-1, rewrites the art's SGR 1 as a bright foreground, and ends
with an SGR reset and a new line so the prompt after it starts clean.

ANSI art uses SGR 1 to pick the bright colours 8-15 (ESC[1;31m is bright red),
never a heavier glyph. The xterm personality draws SGR 1 as bold AND bright,
so a copied ESC[1m drew the banner's bright letters thick. Each SGR is
rewritten with the bold state folded into the foreground (90-97) and the 1/22
dropped; bold on the default foreground is bright white (97), as ANSI.SYS
draws it."""
import re
import sys

SGR = re.compile(r'\x1b\[([0-9;]*)m')

def unbold(text):
    """Rewrite every SGR in `text` so bold becomes a bright foreground."""
    bold = False
    fg = None  # 0-7, or None for the default foreground

    def rewrite(m):
        nonlocal bold, fg
        params = [int(p) if p else 0 for p in m.group(1).split(';')]
        out = []
        colour = False  # this SGR changed bold or the foreground
        i = 0
        while i < len(params):
            p = params[i]
            if p == 0:
                bold, fg, out, colour = False, None, ['0'], False
            elif p == 1:
                bold, colour = True, True
            elif p == 22:
                bold, colour = False, True
            elif 30 <= p <= 37:
                fg, colour = p - 30, True
            elif p == 39:
                fg, colour = None, True
            elif p in (38, 48) and i + 1 < len(params):
                # 38;5;N and 38;2;R;G;B: copy the colour's own numbers whole
                n = 3 if params[i + 1] == 5 else 5 if params[i + 1] == 2 else 1
                out.extend(str(q) for q in params[i:i + n])
                i += n
                continue
            else:
                out.append(str(p))
            i += 1
        if colour:
            if bold:
                out.append(str(90 + (7 if fg is None else fg)))
            elif fg is not None:
                out.append(str(30 + fg))
            else:
                out.append('39')
        return '\x1b[' + ';'.join(out) + 'm' if out else ''

    return SGR.sub(rewrite, text)

def main(src, dst):
    b = open(src, 'rb').read()
    cut = b.find(b'\x1aSAUCE')
    if cut >= 0:
        b = b[:cut]
    text = unbold(b.decode('latin-1').rstrip('\r\n')) + '\x1b[0m\r\n'
    open(dst, 'wb').write(text.encode('utf-8'))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
