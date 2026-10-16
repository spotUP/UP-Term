#!/usr/bin/env python3
"""An ixemul program links with ixemul's own startup object for every CPU flag (-mcrt=ixemul
-m68020 linked newlib's lib/libm020/crt0.o: gcc's startfile search tries the multilib
subdirectory of every prefix before the bare prefixes, ixemul ships crt0.o only in its base
ixemul/lib/, so the newlib crt0.o of the same name won; the link failed on __nocommandline,
DOSBase and ixemulbase).

For each flag set: link a hello that uses printf and sqrt with the installed compiler ($AGCC,
default ~/opt/amiga/bin/m68k-amigaos-gcc) and -Wl,-t; the traced startup object must be
m68k-amigaos/ixemul/lib/<name> and the link must succeed. Skipped when the compiler is absent."""
import os, re, subprocess, sys, tempfile

AGCC = os.environ.get('AGCC') or os.path.expanduser('~/opt/amiga/bin/m68k-amigaos-gcc')
# flags -> the startup object ixemul's startfile spec names for them
CASES = [
    ([], 'crt0.o'),
    (['-m68020'], 'crt0.o'),
    (['-m68020', '-m68881'], 'crt0.o'),
    (['-m68060'], 'crt0.o'),
    (['-fbaserel'], 'bcrt0.o'),
    (['-fbaserel', '-m68020'], 'bcrt0.o'),
    (['-fbaserel32', '-m68020'], 'lcrt0.o'),
]
HELLO = '#include <stdio.h>\n#include <math.h>\nint main(int c, char **v) { printf("%g\\n", sqrt((double)c)); return 0; }\n'


def main():
    if not os.access(AGCC, os.X_OK):
        print('[INFO] %s not installed: ixemul startfile test skipped' % AGCC)
        return 0
    fails = 0
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, 'h.c')
        open(src, 'w').write(HELLO)
        for flags, obj in CASES:
            r = subprocess.run([AGCC, '-mcrt=ixemul'] + flags + [src, '-o', os.path.join(d, 'h'), '-lm', '-Wl,-t'],
                               capture_output=True, text=True)
            out = r.stdout + r.stderr
            crt = [l.strip() for l in out.splitlines() if re.search(r'crt0\.o\)?$', l.strip())]
            ok = r.returncode == 0 and len(crt) == 1 and re.search(r'm68k-amigaos/ixemul/lib/%s$' % re.escape(obj),
                                                                    os.path.normpath(crt[0]))
            name = ' '.join(flags) or '(no CPU flag)'
            if ok:
                print('[OK] -mcrt=ixemul %s links %s' % (name, os.path.normpath(crt[0])))
            else:
                fails += 1
                undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", out)))
                print('[ERROR] -mcrt=ixemul %s: exit %d, startup %s, undefined %s'
                      % (name, r.returncode, [os.path.normpath(c) for c in crt], undef or '-'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
