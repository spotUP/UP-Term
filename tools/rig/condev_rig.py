#!/usr/bin/env python3
"""condev_rig.py -- DV1 of the console.device plan, the REACHABILITY test of
UP-Term's console.device: the ROM con-handler (CON OFF) drawing a Shell
window through our device, proven with a sentinel only our device answers.

  1. STATUS: console.device is the ROM's; tests/amiga/devwho says ROM.
  2. UPConsole DEVICE ON FILE VTC:up-console.device; STATUS says UP-Term;
     devwho answers (UPCMD_STATS).
  3. NewShell CON:... (the ROM con-handler opens a console.device unit on
     its window: ours). `echo devhi` typed into it (keys through our input
     handler, the unit, CMD_READ) and run (its output through CMD_WRITE):
     units >= 1, bytes written grew by at least the echo's.
  4. endcli: the window closes, units back to 0.
  5. Retired and back: a window holds a unit, DEVICE OFF leaves the device
     loaded for it ("stays"), DEVICE ON switches that same device back on
     (it used to refuse until the window closed, which failed the kit's
     DEVICE step); the window still works through it.
  6. UPConsole DEVICE OFF: STATUS says ROM, devwho says ROM; the device's
     code is unloaded (no unit open).
Screenshot of step 3: build/rig/shots/condev.png. Log: condev.log.
The rig must be up; `make build/amiga/up-console.device build/amiga/UPConsole
build/amiga/devwho` first. Leaves CON: as the ROM's."""
import os, pathlib, re, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = ROOT / "build/rig/vtc"
OUT = ROOT / "build/rig/shots"
lines, failed = [], 0


def run(cmd, timeout=60):
    b = ami.req(0x02, struct.pack('>H', timeout) + cmd.encode('latin-1'))
    return struct.unpack('>I', b[:4])[0], b[4:].decode('latin-1')


def say(s):
    lines.append(s)
    print(s, flush=True)


def check(ok, what, seen=''):
    global failed
    failed += not ok
    say('%s %s%s' % ('ok' if ok else 'FAIL', what, '' if ok else ': ' + seen.strip()))
    return ok


def typeline(s, wait=2.0):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.4)
    ami.key(0x44)
    time.sleep(wait)


def stats():
    out = run('VTC:devwho')[1].strip()
    m = re.match(r'UP-Term units (\d+) written (\d+) answered (\d+) dropped (\d+)', out)
    return (out, tuple(int(x) for x in m.groups()) if m else None)


def main():
    for f in ("up-console.device", "UPConsole", "devwho"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    OUT.mkdir(parents=True, exist_ok=True)
    run('VTC:UPConsole CON OFF')
    run('VTC:UPConsole DEVICE OFF')
    st = run('VTC:UPConsole STATUS')[1]
    check('console.device: ROM' in st, 'STATUS before: console.device is the ROM\'s', st)
    out, s0 = stats()
    check(out.startswith('ROM'), 'devwho before: ROM (%s)' % out, out)
    rc, out = run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    if not check(rc == 0 and 'UP-Term now' in out, 'UPConsole DEVICE ON', out):
        return finish()
    st = run('VTC:UPConsole STATUS')[1]
    check('console.device: UP-Term' in st, 'STATUS after: console.device is UP-Term', st)
    out, s0 = stats()
    if not check(s0 is not None, 'devwho: our device answers UPCMD_STATS (%s)' % out, out):
        return finish()
    run('Run >NIL: NewShell "CON:0/240/640/150/condev"')
    time.sleep(5)
    typeline('echo devhi', 3)
    ami.main(['shot', str(OUT / 'condev.png')])
    out, s1 = stats()
    check(s1 is not None and s1[0] >= 1, 'a ROM CON: window opened a unit of ours (%s)' % out, out)
    check(s1 is not None and s1[1] >= s0[1] + len('devhi'), 'its output went through our CMD_WRITE (written %d -> %s)' % (s0[1], s1[1] if s1 else '?'), out)
    check(s1 is not None and s1[2] > s0[2], 'the typed line came through our CMD_READ (answered %d -> %s)' % (s0[2], s1[2] if s1 else '?'), out)
    typeline('endcli', 3)
    out, s2 = stats()
    check(s2 is not None and s2[0] == 0, 'endcli closed the unit (%s)' % out, out)
    # retired and back
    run('Run >NIL: NewShell "CON:0/240/640/150/condev2"')
    time.sleep(5)
    rc, out = run('VTC:UPConsole DEVICE OFF')
    check(rc == 0 and 'stays' in out, 'DEVICE OFF with a window open: the device stays for it', out)
    rc, out = run('VTC:UPConsole DEVICE ON FILE VTC:up-console.device')
    check(rc == 0 and 'again' in out, 'DEVICE ON switches the device still in use back on', out)
    out, s3 = stats()
    check(s3 is not None and s3[0] >= 1, 'devwho: ours again, the window\'s unit with it (%s)' % out, out)
    typeline('echo again', 3)
    out, s4 = stats()
    check(s3 is not None and s4 is not None and s4[1] > s3[1], 'the window writes through it (%s)' % out, out)
    typeline('endcli', 3)
    out, s5 = stats()
    check(s5 is not None and s5[0] == 0, 'endcli closed that unit too (%s)' % out, out)
    rc, out = run('VTC:UPConsole DEVICE OFF')
    check(rc == 0 and 'stays' not in out, 'UPConsole DEVICE OFF, code unloaded', out)
    st = run('VTC:UPConsole STATUS')[1]
    check('console.device: ROM' in st, 'STATUS after OFF: the ROM\'s', st)
    out, _ = stats()
    check(out.startswith('ROM'), 'devwho after OFF: ROM (%s)' % out, out)
    return finish()


def finish():
    say('passed %d of %d' % (sum(1 for l in lines if l.startswith('ok ')),
                             sum(1 for l in lines if l[:3] in ('ok ', 'FAI'))))
    (OUT / 'condev.log').write_text('\n'.join(lines) + '\n')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
