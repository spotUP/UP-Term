"""zmodem_rig check 4 (host lsz -> Amiga rz): lsz must start once rz's ZRINIT is on the wire.
Started at once it read vsh's echo of "VTC:rz" first, took the 'C' for a YMODEM receiver's
request, sent YMODEM blocks rz never answers and gave up with rc 128 (2026-10-25).
The far end here plays vsh + rz: echo the command line, then ZRINIT, ZRINIT again for
every ZRQINIT; lsz must answer with a ZMODEM ZFILE header, never a YMODEM block 0."""
import os, pathlib, select, shutil, sys, tempfile, threading, time, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import getty_rig
import zmodem_rig


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def hex_header(kind, flags=(0, 0, 0, 0)):
    body = bytes((kind,) + tuple(flags))
    return b'**\x18B' + (body.hex() + '%04x' % crc16(body)).encode() + b'\r\n\x11'


ZRINIT = hex_header(1, (0, 0, 0, 0x03))  # CANFDX | CANOVIO: 16-bit CRC frames
ZRQINIT = b'**\x18B00'
ZFILE16 = b'*\x18A\x04'
YMODEM0 = b'\x00\xffzm-back.bin\x00'


def fake_amiga(fd, seen, stop):
    """vsh echoes the line (late, as over the rig's serial port), rz says ZRINIT and answers each ZRQINIT with it"""
    buf, answered = b'', 0
    while not stop.is_set():
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r:
            continue
        try:
            buf += os.read(fd, 4096)
        except OSError:
            return
        if answered == 0 and b'VTC:rz\r' in buf:
            # an Amiga's echo comes late: after lsz started and flushed its input
            time.sleep(0.3)
            os.write(fd, b'VTC:rz\r\n')
            time.sleep(0.3)
            os.write(fd, ZRINIT)
            buf, answered = b'', 1
            continue
        seen[:] = [buf]
        while answered and buf.count(ZRQINIT) >= answered:
            os.write(fd, ZRINIT)
            answered += 1


@unittest.skipUnless(shutil.which('lsz'), 'lrzsz is not installed')
class Handshake(unittest.TestCase):
    def test_lsz_speaks_zmodem_after_the_echo(self):
        wire = getty_rig.Wire()
        cwd = tempfile.mkdtemp(prefix='zm-hs-')
        pathlib.Path(cwd, 'zm-back.bin').write_bytes(bytes(range(256)) * 8)
        seen, stop = [], threading.Event()
        t = threading.Thread(target=fake_amiga, args=(wire.slave, seen, stop), daemon=True)
        t.start()
        try:
            zmodem_rig.lsz_to_rz(wire, cwd, 'zm-back.bin', secs=4)
        finally:
            stop.set()
            t.join(2)
            shutil.rmtree(cwd, ignore_errors=True)
        out = b''.join(seen)
        self.assertNotIn(YMODEM0, out, 'lsz fell back to YMODEM')
        self.assertIn(ZFILE16, out, 'lsz never sent ZFILE: %r' % out[:200])


if __name__ == '__main__':
    unittest.main()
