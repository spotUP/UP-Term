#!/usr/bin/env python3
"""concon_rig.py -- H5.6 of the console.device plan, the REACHABILITY test
of the CON:/RAW: swap: the system's own CON: served by UP-Term through
C:UPConsole, proven from the top (a NewShell CON: window, as a user opens
one) with a sentinel only the vtcon handler answers.

  1. STATUS says ROM; a Shell in a CON: window runs tests/amiga/conwho:
     ACTION_VTCON_GWINSZ is refused (ROM, error 209); romprobe's cases
     (tests/probes/amiga_cases.txt) through the ROM CON: are recorded.
  2. UPConsole CON ON (HANDLER VTC:vtcon-handler); STATUS says UP-Term.
  3. A new Shell in a CON: window: conwho gets the grid (UP-Term rows cols).
  4. romprobe through CON: again: every case must put the cursor where the
     ROM console did.
  5. conwho RAW: a RAW: window hands over one typed key without RETURN.
  6. UPConsole CON OFF: a new CON: window is the ROM's again (error 209).
The rig must be up on a pre-3.2 Kickstart (3.2 is refused, plan DD20);
`make build/amiga/UPConsole build/amiga/conwho build/amiga/romprobe
build/amiga/vtcon-handler` first. Log: build/rig/shots/concon.log."""
import os, pathlib, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"
PROBE_WIN = "CON:0/12/656/216/probe"
lines, failed = [], 0


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def say(s):
    lines.append(s)
    print(s)


def check(ok, what, seen=''):
    global failed
    failed += not ok
    say('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()))


def typeline(s, wait=2.0):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def shell_conwho(title, out):
    """A NewShell CON: window, as a user opens one; conwho run in it."""
    run('Delete %s QUIET' % out)
    run('Run >NIL: NewShell "CON:0/240/640/150/%s"' % title)
    time.sleep(4)
    typeline('VTC:conwho >%s' % out, 3)
    typeline('endcli', 2)
    return run('Type %s' % out)[1].strip()


def romprobe():
    return run('VTC:romprobe VTC:amiga_cases.txt "%s"' % PROBE_WIN, 120)[1]


def main():
    for f in ("UPConsole", "conwho", "romprobe", "vtcon-handler"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    shutil.copyfile(ROOT / "tests/probes/amiga_cases.txt", VTC / "amiga_cases.txt")
    OUT.mkdir(parents=True, exist_ok=True)
    run('VTC:UPConsole CON OFF')  # a rig left switched by an earlier run
    st = run('VTC:UPConsole STATUS')[1]
    check('CON: ROM' in st, 'STATUS before: CON: is the ROM\'s', st)
    who = shell_conwho('rom', 'RAM:who-rom.txt')
    check(who.startswith('ROM error 209'), 'a ROM CON: window refuses GWINSZ (%s)' % who, who)
    rom = romprobe()
    rc, out = run('VTC:UPConsole CON ON HANDLER VTC:vtcon-handler')
    check(rc == 0 and 'UP-Term' in out, 'UPConsole CON ON', out)
    st = run('VTC:UPConsole STATUS')[1]
    check('CON: UP-Term' in st and 'RAW: UP-Term' in st, 'STATUS after: CON: and RAW: are UP-Term', st)
    who = shell_conwho('upterm', 'RAM:who-up.txt')
    check(who.startswith('UP-Term '), 'a NewShell CON: window is served by UP-Term (%s)' % who, who)
    up = romprobe()
    rl = [l for l in rom.splitlines() if l.strip()]
    ul = [l for l in up.splitlines() if l.strip()]
    same = sum(1 for a, b in zip(rl, ul) if a == b)
    check(len(rl) > 1 and len(rl) == len(ul) and same == len(rl),
          'romprobe through CON: equals the ROM on %d of %d lines' % (same, len(rl)),
          '\n'.join('ROM %s | UP %s' % (a, b) for a, b in zip(rl, ul) if a != b)[:1500])
    run('Delete RAM:raw.txt QUIET')
    run('Run >NIL: VTC:conwho >RAM:raw.txt RAW')
    time.sleep(3)
    ami.req(0x08, bytes([4]) + b'x')
    time.sleep(3)
    raw = run('Type RAM:raw.txt')[1].strip()
    check(raw == 'RAW byte 78', 'a RAW: window hands over one key without RETURN (%s)' % raw, raw)
    rc, out = run('VTC:UPConsole CON OFF')
    check(rc == 0, 'UPConsole CON OFF', out)
    who = shell_conwho('back', 'RAM:who-back.txt')
    check(who.startswith('ROM error 209'), 'after CON OFF a new CON: window is the ROM\'s (%s)' % who, who)
    say('passed %d of %d' % (sum(1 for l in lines if l.startswith('ok ')), sum(1 for l in lines if l[:3] in ('ok ', 'FAI'))))
    (OUT / 'concon.log').write_text('\n'.join(lines) + '\n\nROM romprobe:\n' + rom + '\nUP-Term romprobe:\n' + up)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
