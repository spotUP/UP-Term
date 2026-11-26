#!/usr/bin/env python3
"""WASABI-NO-SCREENS: ami.py talked to whatever daemon held the rig's port and got "there are no screens open" from
an Amiga that was not the rig asked for. wasabi.check_identity asks the port's UDP discovery for the daemon's name
and refuses anything but rig<N>. Host-only: a fake wasabid answers discovery on a loopback UDP port."""
import os, pathlib, socket, sys, threading, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
os.environ.setdefault('UPTERM_RIG', '7')
import wasabi


class FakeDaemon:
    """Answers WASABI?1 on a free UDP port the way wasabid's answer_probe does (reply None: stays silent)."""
    def __init__(self, reply):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(('127.0.0.1', 0))
        self.port = self.s.getsockname()[1]
        self.s.settimeout(2)
        self.reply = reply
        self.t = threading.Thread(target=self.serve, daemon=True)
        self.t.start()

    def serve(self):
        try:
            while True:
                data, addr = self.s.recvfrom(64)
                if data.startswith(b'WASABI?1') and self.reply:
                    self.s.sendto((self.reply % {'port': self.port, 'own': 'rig%d' % (self.port - wasabi.BASE_PORT + 1)}).encode(), addr)
        except OSError:
            pass

    def close(self):
        self.s.close()


class RigIdentity(unittest.TestCase):
    def check(self, reply):
        d = FakeDaemon(reply)   # %(own)s: rig<N> of the fake's port (paths.wasabi_port)
        wasabi._VERIFIED.clear()
        try:
            with mock.patch.object(wasabi.paths, 'port_listeners', lambda port: ['4242']):
                wasabi.check_identity('127.0.0.1', d.port, 'wasabid 0.11b2', tries=1, timeout=0.3)
        finally:
            d.close()

    def test_match(self):
        self.check('WASABI!1 %(own)s %(port)d wasabid 0.11b2\n')   # no SystemExit

    def test_mismatch(self):
        with self.assertRaises(SystemExit) as cm:
            self.check('WASABI!1 rig2 %(port)d wasabid 0.11b2\n')
        msg = str(cm.exception)
        for part in ('rig identity', 'calls itself rig2', 'wasabid 0.11b2', '127.0.0.1:', 'pid 4242'):
            self.assertIn(part, msg)

    def test_kit_daemon_without_name(self):
        with self.assertRaises(SystemExit) as cm:
            self.check('WASABI!1 amiga %(port)d wasabid 0.11b2\n')
        self.assertIn('calls itself amiga', str(cm.exception))

    def test_no_discovery_answer(self):
        with self.assertRaises(SystemExit) as cm:
            self.check(None)
        self.assertIn('gave no discovery name', str(cm.exception))


if __name__ == '__main__':
    unittest.main()
