#!/usr/bin/env python3
"""prof68k.py -- instructions per function on the 68020, counted under vamos
(ledger S1; creep's method: "count instructions per function under vamos
before timing anything"). On a stock A1200 every instruction outside the
68020's 256-byte cache is a chip-RAM fetch, so the counts and each
function's code footprint are what the speed work is steered by.

  tools/prof68k.py build                 engbench with line debug info -> build/prof/engbench
  tools/prof68k.py run ONLY w PERS p     count one engbench workload (w 0-5, p 0 xterm / 1 amiga)
        [--bytes N]                      divide by N input bytes as well (instructions a byte)

The program runs once under `vamos -C 68020 -I` (an instruction trace);
each traced PC is mapped to file:line through the binary's LINE debug
hunks and the line to its C function (or asm label). Output: per
function the instructions executed, their share, the distinct code bytes
it executed (its footprint: over 256 means it cannot sit in the cache)
and its hottest lines.
"""
import collections, pathlib, re, struct, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/prof'
BIN = OUT / 'engbench'


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    cfg = (ROOT / 'tools/vbcc-aos68k.cfg').read_text()
    cfg = cfg.replace(' -x ', ' ')  # keep the symbols
    cfg = cfg.replace('-ld=vlink ', '-ld=vlink -M%s ' % (OUT / 'link.map'), 1)
    (OUT / 'vbcc-prof.cfg').write_text(cfg)
    cmd = ['vc', '+%s' % (OUT / 'vbcc-prof.cfg'), '-g', '-I%s' % (ROOT / 'vendor/ndk-3.2r4-Include_H'),
           '-cpu=68020', '-O2', '-warn=-1', '-dontwarn=163,166,167,168,170,306,307,81,153,65', '-DVT_ASM', '-DVP_ASM',
           '-o', str(BIN), 'tests/amiga/engbench.c', 'engine/vtengine.c', 'engine/vtengine_68k.s',
           'render/amiga_render_68k.s', 'render/painter.c', 'render/painter_68k.s']
    subprocess.run(cmd, cwd=ROOT, check=True)
    print('built', BIN)


def line_table(path):
    """[(offset, file, line)] of the first code segment, from its LINE hunks."""
    d = path.read_bytes()
    pos = 0

    def L():
        nonlocal pos
        v = struct.unpack('>I', d[pos:pos + 4])[0]
        pos += 4
        return v
    assert L() == 0x3F3
    while L():  # resident library names (none)
        pass
    nseg, first, last = L(), L(), L()
    for _ in range(last - first + 1):
        L()
    seg = -1
    out = []
    while pos < len(d):
        h = L() & 0x3FFFFFFF
        if h in (0x3E9, 0x3EA):      # CODE / DATA
            seg += 1
            size = L() & 0x3FFFFFFF  # (not `pos += L()...`: L moves pos)
            pos += size * 4
        elif h == 0x3EB:             # BSS
            seg += 1
            L()
        elif h in (0x3EC, 0x3F7):    # RELOC32 / RELOC32SHORT, ABSRELOC32
            short = h == 0x3F7
            while True:
                if short:
                    n = struct.unpack('>H', d[pos:pos + 2])[0]; pos += 2
                    if not n:
                        break
                    pos += 2 + n * 2
                else:
                    n = L()
                    if not n:
                        break
                    pos += 4 + n * 4
            if short and pos % 4:
                pos += 2
        elif h == 0x3F0:             # SYMBOL
            while True:
                n = L()
                if not n:
                    break
                pos += (n & 0xFFFFFF) * 4 + 4
        elif h == 0x3F1:             # DEBUG
            n = L()
            end = pos + n * 4
            base = L()
            tag = d[pos:pos + 4]; pos += 4
            if tag == b'LINE' and seg == 0:
                nl = L()
                name = d[pos:pos + nl * 4].rstrip(b'\0').decode('latin-1'); pos += nl * 4
                while pos < end:
                    ln, off = L(), L()
                    out.append((base + off, name, ln & 0xFFFFFF))
            pos = end
        elif h == 0x3F2:             # END
            pass
        else:
            raise SystemExit('prof68k: hunk %x not handled' % h)
    out.sort()
    return out


def functions_of(src):
    """line -> function name for a C or asm file."""
    names = {}
    cur = '?'
    lines = (ROOT / src).read_text(errors='replace').splitlines()
    for i, l in enumerate(lines, 1):
        if src.endswith('.s'):
            m = re.match(r'^(_?[A-Za-z][\w]*):', l)
            if m:
                cur = m.group(1)
        else:
            m = re.match(r'^[A-Za-z_][\w \*]*?\b([A-Za-z_]\w*)\s*\([^;]*$', l)
            if m and not l.startswith(('if', 'for', 'while', 'switch', 'return')) and m.group(1) not in ('if', 'while'):
                cur = m.group(1)
        names[i] = cur
    return names


def asm_ranges():
    """(start, end, name) of the asm routines (no LINE info): global code
    symbols from the link map whose name says asm."""
    syms = []
    sec = None
    for l in (OUT / 'link.map').read_text(errors='replace').splitlines():
        if l.startswith('Symbols of '):
            sec = l[len('Symbols of '):].rstrip(':')
        m = re.match(r'\s+0x([0-9a-f]+) (\S+): global', l)
        if m and sec == 'CODE':
            syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    out = []
    for i, (a, n) in enumerate(syms):
        if '_asm_' in n:
            out.append((a, syms[i + 1][0] if i + 1 < len(syms) else a + 4096, n.lstrip('_')))
    return out


def run(args, nbytes):
    table = line_table(BIN)
    asms = asm_ranges()
    offs = [t[0] for t in table]
    import bisect
    funcs = {}
    count = collections.Counter()
    bytes_hit = collections.defaultdict(set)
    linecount = collections.Counter()
    import os
    home = OUT / 'vamos-home'  # its own RAM: volume, not shared with other vamos runs
    home.mkdir(exist_ok=True)
    import shutil
    shutil.rmtree(home / '.vamos/volumes/ram', ignore_errors=True)  # left by a killed run
    proc = subprocess.Popen(['vamos', '-C', '68020', '-I', '-B', str(BIN)] + args, cwd=OUT,
                            env=dict(os.environ, HOME=str(home)),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1 << 20)
    pat = re.compile(r'\+([0-9a-f]{6}) engbench_0:code')
    total = 0
    other = 0
    for line in proc.stdout:
        m = pat.search(line)
        if not m:
            if ' instr:' in line:
                other += 1
            elif 'instr:' not in line:
                sys.stderr.write(line)
            continue
        off = int(m.group(1), 16)
        total += 1
        i = bisect.bisect_right(offs, off) - 1
        asm = next((n for a, e, n in asms if a <= off < e), None)
        if asm:
            fn, key = 'asm:' + asm, ('asm', asm)
        elif i < 0:
            fn, key = '?', ('?', 0)
        else:
            _, f, ln = table[i]
            if f not in funcs:
                funcs[f] = functions_of(f) if (ROOT / f).exists() else {}
            fn = '%s:%s' % (pathlib.Path(f).stem, funcs[f].get(ln, '?'))
            key = (f, ln)
        count[fn] += 1
        bytes_hit[fn].add(off & ~1)
        linecount[key] += 1
    proc.wait()
    print('%d instructions in the program, %d in libraries (traps)' % (total, other))
    if nbytes:
        print('%.1f instructions an input byte' % (total / nbytes))
    print('\n%-40s %10s %6s %8s' % ('function', 'instr', '%', 'footprt'))
    for fn, c in count.most_common(30):
        print('%-40s %10d %5.1f%% %7dB' % (fn, c, 100.0 * c / total, len(bytes_hit[fn]) * 2))
    print('\nhottest lines:')
    for (f, ln), c in linecount.most_common(25):
        src = ''
        p = ROOT / f if f != 'asm' else None
        if p and p.exists():
            ls = p.read_text(errors='replace').splitlines()
            src = ls[ln - 1].strip()[:70] if 0 < ln <= len(ls) else ''
        print('%10d  %s:%s  %s' % (c, f, ln, src))


if __name__ == '__main__':
    a = sys.argv[1:]
    if not a or a[0] == 'build':
        build()
    else:
        nb = 0
        if '--bytes' in a:
            nb = int(a[a.index('--bytes') + 1])
            del a[a.index('--bytes'):a.index('--bytes') + 2]
        run(a[1:] if a[0] == 'run' else a, nb)
