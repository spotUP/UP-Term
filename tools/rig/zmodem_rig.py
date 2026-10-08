#!/usr/bin/env python3
"""zmodem_rig.py -- ledger T4 G2/G3: files over the serial login by ZMODEM,
with our own sz / rz on the Amiga (zm/, no ixemul) and the host's lrzsz at
the far end of the cable, as a user's terminal program would.

  1. upgetty on the rig's serial port (rig.py --serial: a host pty), vsh's
     prompt over the wire.
  2. Amiga -> host: `VTC:sz VTC:zm-send.bin` at the prompt, the host's lrz
     on the wire receives; the file arrives byte for byte (every byte value,
     70 KB).
  3. host -> Amiga: `VTC:rz` in RAM:, the host's lsz sends; the file in RAM:
     is copied to VTC: and compared on the host.
  4. The prompt is back after each.
Needs lrzsz on the host (lsz / lrz). The rig is stopped before and after."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import os, pathlib, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import getty_rig as g

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"
results = []


def check(ok, what, seen=''):
    results.append(ok)
    print('%s %d %s%s' % ('ok' if ok else 'FAIL', len(results), what,
                          (': ' + repr(seen)[-300:]) if seen and not ok else ''), flush=True)


def payload():
    return bytes(((i * 7 + i // 256) & 0xFF) for i in range(70000))


def host(cmd, wire, cwd, secs=120):
    """a host program on the far end of the cable (the wire's master)"""
    p = subprocess.Popen(cmd, stdin=wire.master, stdout=wire.master, stderr=subprocess.DEVNULL, cwd=cwd)
    try:
        return p.wait(timeout=secs)
    except subprocess.TimeoutExpired:
        p.kill()
        return -1


def main():
    if not shutil.which('lrz') or not shutil.which('lsz'):
        print('zmodem_rig: lrzsz is not installed on the host')
        return 2
    # pty-handler before the boot: the rig mounts PTY: from VTC: as it starts,
    # and SetMode's raw (cfmakeraw) is the handler's
    for f in ("upgetty", "sz", "rz", "vsh", "pty-handler"):
        shutil.copyfile(ROOT / "build/amiga" / f, VTC / f)
    data = payload()
    (VTC / "zm-send.bin").write_bytes(data)
    wire = g.Wire()
    subprocess.run(g.RIG + ["stop"], capture_output=True)
    subprocess.run(g.RIG + ["start", "--serial", wire.path], check=True)
    import ixpty_rig
    ixpty_rig.use_ixemul()
    if g.run('Assign >NIL: PTY: EXISTS DEVICES')[0] != 0:
        g.run('Mount PTY: FROM VTC:ptymount')
    g.run('Delete RAM:zm-send.bin QUIET')
    g.run('Run >NIL: VTC:upgetty SHELL VTC:vsh')
    prompt = rb'[%#$>] $'
    check(wire.wait_for(prompt, 30), 'vsh\'s prompt arrives over serial', wire.text())
    wire.seen = b''
    wire.send('cd RAM:\r')
    wire.wait_for(prompt, 10)

    # Amiga -> host
    got = pathlib.Path(tempfile.mkdtemp(prefix='zm-rig-'))
    wire.seen = b''
    # the far end takes over the line at once, as a terminal program's
    # zmodem auto-start does: reading the echo here could swallow the
    # protocol's first bytes (the test was flaky while it did)
    wire.send('VTC:sz VTC:zm-send.bin\r')
    t0 = time.time()
    rc = host(['lrz', '-y', '-q'], wire, str(got))
    secs = time.time() - t0
    out = got / 'zm-send.bin'
    check(rc == 0 and out.exists() and out.read_bytes() == data,
          'sz on the Amiga -> lrz on the host: 70 KB byte for byte (%.1f s)' % secs,
          'rc %s, %s bytes' % (rc, out.stat().st_size if out.exists() else 'no file'))
    wire.seen = b''
    wire.send('\r')
    check(wire.wait_for(prompt, 15), 'the prompt is back after sz', wire.text())

    # host -> Amiga
    src = got / 'zm-back.bin'
    src.write_bytes(data[::-1])
    wire.seen = b''
    wire.send('VTC:rz\r')
    t0 = time.time()
    rc = host(['lsz', '-q', 'zm-back.bin'], wire, str(got))
    secs = time.time() - t0
    wire.seen = b''
    wire.send('\r')
    back = wire.wait_for(prompt, 15)
    (VTC / 'zm-back.bin').unlink(missing_ok=True)
    g.run('Copy RAM:zm-back.bin VTC:zm-back.bin')
    arrived = VTC / 'zm-back.bin'
    check(rc == 0 and arrived.exists() and arrived.read_bytes() == data[::-1],
          'lsz on the host -> rz on the Amiga: 70 KB byte for byte (%.1f s)' % secs,
          'rc %s, %s bytes' % (rc, arrived.stat().st_size if arrived.exists() else 'no file'))
    check(back, 'the prompt is back after rz', wire.text())

    wire.send('exit\r')
    time.sleep(2)
    shutil.rmtree(got, ignore_errors=True)
    for f in ('zm-send.bin', 'zm-back.bin'):
        (VTC / f).unlink(missing_ok=True)
    subprocess.run(g.RIG + ["stop"], capture_output=True)
    print('zmodem_rig: passed %d of %d' % (sum(results), len(results)))
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
