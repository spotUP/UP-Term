#!/usr/bin/env python3
"""Tests of tools/uptelnetd.py (ledger A1.2), run by `make test`.

The rules that keep it on the LAN (bind and allowlist), the password's
source and the lockout are tested as functions; one test drives the real
entry point (main, in a subprocess, as the owner starts it) end to end:
refused peers, a wrong password, the right one, then the shell's pty with
the client's TERM and window size. A second sends the same login through
build/tn_host -- uptelnet's own protocol code (net/tn.c) -- so the Amiga's
client and the Mac's server are proved to agree.
"""
import os
import pathlib
import re
import select
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import uptelnetd as d  # noqa: E402

PW = "correct horse battery"
# the session proves it ran, with what it was given
SENTINEL_CMD = "printf 'SENTINEL term=%s size=%s pw=%s\\n' \"$TERM\" \"$(stty size)\" \"${UPTELNETD_PASSWORD:-none}\""


class Bind(unittest.TestCase):
    def test_wildcards_are_refused(self):
        for a in ("0.0.0.0", "::"):
            with self.assertRaises(d.ConfigError):
                d.check_bind(a)

    def test_public_addresses_are_refused(self):
        for a in ("8.8.8.8", "172.32.0.1", "100.64.0.1", "2001:db8::1", "224.0.0.1"):
            with self.assertRaises(d.ConfigError, msg=a):
                d.check_bind(a)

    def test_lan_and_loopback_are_accepted(self):
        for a in ("192.168.0.58", "10.1.2.3", "172.16.5.4", "169.254.1.1", "127.0.0.1", "fe80::1", "fd12::3"):
            self.assertEqual(d.check_bind(a), a)

    def test_not_an_address(self):
        with self.assertRaises(d.ConfigError):
            d.check_bind("example.com")


class Binds(unittest.TestCase):
    """--bind repeats: the NAS listens on its LAN address and on loopback,
    where Tailscale's userspace mode hands over the connections it accepts."""
    def args(self, *argv):
        return d.parse_args(list(argv) + ["--command", "true"])

    def test_two_binds_each_with_its_own_default_allowlist(self):
        binds, nets, _, _ = d.configure(self.args("--bind", "192.168.0.198", "--bind", "127.0.0.1"),
                                        env={d.PASSWORD_ENV: "a-long-enough-one", "SHELL": "/bin/sh"})
        self.assertEqual(binds, ["192.168.0.198", "127.0.0.1"])
        self.assertEqual([str(n) for n in nets], ["192.168.0.0/24", "127.0.0.0/8"])

    def test_every_bind_is_checked(self):
        with self.assertRaises(d.ConfigError):
            d.configure(self.args("--bind", "192.168.0.198", "--bind", "0.0.0.0"),
                        env={d.PASSWORD_ENV: "a-long-enough-one", "SHELL": "/bin/sh"})

    def test_a_repeated_bind_is_listened_on_once(self):
        binds, _, _, _ = d.configure(self.args("--bind", "127.0.0.1", "--bind", "127.0.0.1"),
                                     env={d.PASSWORD_ENV: "a-long-enough-one", "SHELL": "/bin/sh"})
        self.assertEqual(binds, ["127.0.0.1"])


class Allow(unittest.TestCase):
    def test_default_is_the_interfaces_subnet(self):
        nets = d.default_allow("192.168.0.58", "255.255.252.0")
        self.assertEqual([str(n) for n in nets], ["192.168.0.0/22"])
        self.assertTrue(d.allowed("192.168.3.9", nets))
        self.assertFalse(d.allowed("192.168.4.1", nets))
        self.assertFalse(d.allowed("8.8.8.8", nets))

    def test_default_without_a_mask_is_a_24(self):
        self.assertEqual([str(n) for n in d.default_allow("10.0.7.2")], ["10.0.7.0/24"])

    def test_networks_past_the_lan_are_refused(self):
        for c in ("0.0.0.0/0", "8.8.8.0/24", "192.0.0.0/2", "::/0"):
            with self.assertRaises(d.ConfigError, msg=c):
                d.check_allow(c)
        self.assertEqual(str(d.check_allow("192.168.1.7/24")), "192.168.1.0/24")

    def test_ipv4_mapped_peer(self):
        self.assertTrue(d.allowed("::ffff:192.168.1.5", [d.check_allow("192.168.1.0/24")]))
        self.assertFalse(d.allowed("garbage", [d.check_allow("192.168.1.0/24")]))


class Password(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, "password")

    def tearDown(self):
        self.dir.cleanup()

    def write(self, text, mode):
        with open(self.path, "w") as f:
            f.write(text)
        os.chmod(self.path, mode)

    def test_environment_wins(self):
        self.write("from the file 123\n", 0o600)
        self.assertEqual(d.load_password({d.PASSWORD_ENV: PW}, self.path), PW)

    def test_private_file_is_read_without_its_newline(self):
        self.write(PW + "\n", 0o600)
        self.assertEqual(d.load_password({}, self.path), PW)

    def test_file_others_can_read_is_refused(self):
        for mode in (0o644, 0o640, 0o604, 0o620):
            self.write(PW + "\n", mode)
            with self.assertRaises(d.ConfigError, msg=oct(mode)):
                d.load_password({}, self.path)

    def test_someone_elses_file_is_refused(self):
        self.write(PW + "\n", 0o600)
        with self.assertRaises(d.ConfigError):
            d.load_password({}, self.path, uid=os.getuid() + 1)

    def test_a_link_is_refused(self):
        self.write(PW + "\n", 0o600)
        link = self.path + ".link"
        os.symlink(self.path, link)
        with self.assertRaises(d.ConfigError):
            d.load_password({}, link)

    def test_missing_or_short_is_refused(self):
        with self.assertRaises(d.ConfigError):
            d.load_password({}, self.path)
        with self.assertRaises(d.ConfigError):
            d.load_password({d.PASSWORD_ENV: "short"}, self.path)

    def test_compare(self):
        self.assertTrue(d.password_ok(PW, PW))
        self.assertFalse(d.password_ok(PW + "x", PW))
        self.assertFalse(d.password_ok("", PW))

    def test_session_env_never_carries_the_password(self):
        env = d.session_env({d.PASSWORD_ENV: PW, "HOME": "/h", "PATH": "/bin", "SECRET_TOKEN": "x",
                             "COLORTERM": "truecolor"}, "xterm-256color", "/bin/sh")
        self.assertNotIn(d.PASSWORD_ENV, env)
        self.assertNotIn("SECRET_TOKEN", env)
        self.assertNotIn("COLORTERM", env)
        self.assertEqual(env["TERM"], "xterm-256color")
        self.assertEqual(env["LANG"], "en_US.UTF-8")


class Limiter(unittest.TestCase):
    def test_five_failures_lock_the_address_for_ten_minutes(self):
        now = [1000.0]
        r = d.RateLimiter(clock=lambda: now[0])
        for i in range(d.LOCK_FAILURES - 1):
            self.assertFalse(r.failure("10.0.0.9"))
            now[0] += 1
        self.assertFalse(r.is_locked("10.0.0.9"))
        self.assertTrue(r.failure("10.0.0.9"))
        self.assertTrue(r.is_locked("10.0.0.9"))
        self.assertFalse(r.is_locked("10.0.0.10"))  # per address
        now[0] += d.LOCK_TIME - 1
        self.assertTrue(r.is_locked("10.0.0.9"))
        now[0] += 2
        self.assertFalse(r.is_locked("10.0.0.9"))
        self.assertFalse(r.failure("10.0.0.9"))  # the count started again

    def test_old_failures_age_out_and_success_clears(self):
        now = [0.0]
        r = d.RateLimiter(clock=lambda: now[0])
        for _ in range(d.LOCK_FAILURES - 1):
            r.failure("p")
        now[0] += d.LOCK_WINDOW + 1
        self.assertFalse(r.failure("p"))
        for _ in range(d.LOCK_FAILURES - 2):
            r.failure("p")
        r.success("p")
        self.assertFalse(r.failure("p"))


class TelnetServer(unittest.TestCase):
    def test_naws_with_doubled_255_split_bytewise(self):
        t = d.Telnet()
        t.start()
        msg = bytes([255, 251, 31, 255, 250, 31, 0, 255, 255, 1, 255, 255, 255, 240])
        out = b"".join(t.feed(bytes([c])) for c in msg)
        self.assertEqual(out, b"")
        self.assertEqual(t.size, (255, 511))

    def test_ttype_is_asked_for_and_taken(self):
        t = d.Telnet()
        t.start()
        t.out.clear()
        t.feed(bytes([255, 251, 24]))
        self.assertEqual(bytes(t.out), bytes([255, 250, 24, 1, 255, 240]))  # agreed: SEND at once
        t.feed(bytes([255, 250, 24, 0]) + b"XTERM-256color" + bytes([255, 240]))
        self.assertEqual(t.term, "xterm-256color")
        t.feed(bytes([255, 250, 24, 0]) + b"bad;name" + bytes([255, 240]))
        self.assertEqual(t.term, "xterm-256color")  # nothing odd reaches $TERM

    def test_data_rules(self):
        t = d.Telnet()
        self.assertEqual(t.feed(b"a\r\0b\r\nc\xff\xffd"), b"a\rb\rc\xffd")
        t.feed(bytes([255, 251, 0]))  # client sends BINARY: bytes as they are
        self.assertEqual(t.feed(b"\r\n"), b"\r\n")
        self.assertEqual(t.escape(b"\xff\r"), b"\xff\xff\r\0")
        t.feed(bytes([255, 253, 0]))
        self.assertEqual(t.escape(b"\xff\r"), b"\xff\xff\r")

    def test_no_loop_on_repeated_requests(self):
        t = d.Telnet()
        t.feed(bytes([255, 253, 1]))
        self.assertEqual(bytes(t.out), bytes([255, 251, 1]))
        t.out.clear()
        t.feed(bytes([255, 253, 1, 255, 253, 1]))
        self.assertEqual(bytes(t.out), b"")
        t.feed(bytes([255, 253, 39]))  # NEW-ENVIRON: not ours
        self.assertEqual(bytes(t.out), bytes([255, 252, 39]))


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Server:
    """uptelnetd started through its real entry point, as the owner does."""

    def __init__(self, *extra, command=SENTINEL_CMD):
        self.port = free_port()
        env = dict(os.environ, **{d.PASSWORD_ENV: PW})
        self.p = subprocess.Popen([sys.executable, str(ROOT / "tools/uptelnetd.py"), "--bind", "127.0.0.1",
                                   "--port", str(self.port), "--shell", "/bin/sh", "--command", command] + list(extra),
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        line = self.p.stdout.readline().decode()
        if "listening on 127.0.0.1" not in line:
            self.stop()
            raise AssertionError("uptelnetd did not start: %r %r" % (line, self.p.stderr.read()))

    def stop(self):
        self.p.terminate()
        try:
            self.p.wait(5)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.p.stdout.close()
        self.p.stderr.close()


def read_until(sock, pattern, timeout=10.0):
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([sock], [], [], 0.1)
        if r:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
            if re.search(pattern, buf):
                return buf
    return buf


class Reach(unittest.TestCase):
    def test_login_and_session_through_main(self):
        srv = Server()
        try:
            s = socket.create_connection(("127.0.0.1", srv.port))
            # a client that agrees to NAWS and TTYPE, 100 x 30
            s.sendall(bytes([255, 251, 31, 255, 250, 31, 0, 100, 0, 30, 255, 240, 255, 251, 24]))
            got = read_until(s, rb"Password: ")
            self.assertIn(b"unencrypted", got)
            self.assertIn(bytes([255, 251, 1]), got)  # WILL ECHO: the password is not echoed
            s.sendall(bytes([255, 250, 24, 0]) + b"xterm-256color" + bytes([255, 240]))
            s.sendall(b"wrong password\r\0")
            t0 = time.time()
            got = read_until(s, rb"Password: ")
            self.assertIn(b"Login incorrect", got)
            self.assertGreaterEqual(time.time() - t0, d.FAIL_DELAY - 0.2)
            self.assertNotIn(b"wrong password", got)  # never echoed
            s.sendall(PW.encode() + b"\r\0")
            got = read_until(s, rb"SENTINEL[^\n]*\n")
            m = re.search(rb"SENTINEL term=(\S+) size=(\d+ \d+) pw=(\S+)", got)
            self.assertIsNotNone(m, got)
            self.assertEqual(m.group(1), b"xterm-256color")
            self.assertEqual(m.group(2), b"30 100")
            self.assertEqual(m.group(3), b"none")  # the password stays in the server
            s.close()
        finally:
            srv.stop()

    def test_ctrl_c_reaches_the_session_as_a_signal(self):
        """The pty is the session's controlling terminal: Ctrl-C from the
        Amiga interrupts what runs (Claude Code's Esc/Ctrl-C need this)."""
        srv = Server(command="echo READY; sleep 20; echo SURVIVED")
        try:
            s = socket.create_connection(("127.0.0.1", srv.port))
            read_until(s, rb"Password: ")
            s.sendall(PW.encode() + b"\r\0")
            self.assertIn(b"READY", read_until(s, rb"READY"))
            t0 = time.time()
            s.sendall(b"\x03")
            rest = read_until(s, rb"SURVIVED", timeout=5)
            self.assertNotIn(b"SURVIVED", rest)
            self.assertLess(time.time() - t0, 4.5)  # the session ended and closed the line
            s.close()
        finally:
            srv.stop()

    def test_peer_outside_the_allowlist_gets_nothing(self):
        srv = Server("--allow", "10.0.0.0/8")
        try:
            s = socket.create_connection(("127.0.0.1", srv.port))
            self.assertEqual(read_until(s, rb"Password", timeout=2), b"")
            s.close()
        finally:
            srv.stop()

    def test_refuses_to_start_on_a_wildcard_or_without_a_private_password_file(self):
        env = {k: v for k, v in os.environ.items() if k != d.PASSWORD_ENV}
        r = subprocess.run([sys.executable, str(ROOT / "tools/uptelnetd.py"), "--bind", "0.0.0.0"],
                           capture_output=True, text=True, env=env)
        self.assertEqual(r.returncode, 2)
        self.assertIn("every interface", r.stderr)
        with tempfile.TemporaryDirectory() as tmp:
            p = os.path.join(tmp, "pw")
            with open(p, "w") as f:
                f.write(PW + "\n")
            os.chmod(p, 0o644)
            r = subprocess.run([sys.executable, str(ROOT / "tools/uptelnetd.py"), "--bind", "127.0.0.1",
                                "--password-file", p], capture_output=True, text=True, env=env)
            self.assertEqual(r.returncode, 2)
            self.assertIn("chmod 600", r.stderr)

    def test_uptelnet_protocol_code_logs_in(self):
        """build/tn_host is net/tn.c -- uptelnet's protocol -- on a POSIX socket."""
        tn_host = ROOT / "build/tn_host"
        if not tn_host.exists():
            self.fail("build/tn_host missing: make build/tn_host (make test builds it)")
        srv = Server()
        try:
            c = subprocess.Popen([str(tn_host), "127.0.0.1", str(srv.port), "132", "43"],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            out = b""
            end = time.time() + 10
            sent = False
            while time.time() < end and b"SENTINEL" not in out.split(b"\n")[-1] + out:
                r, _, _ = select.select([c.stdout], [], [], 0.1)
                if r:
                    chunk = os.read(c.stdout.fileno(), 4096)
                    if not chunk:
                        break
                    out += chunk
                if not sent and b"Password: " in out:
                    c.stdin.write(PW.encode() + b"\n")
                    c.stdin.flush()
                    sent = True
                if re.search(rb"SENTINEL[^\n]*\n", out):
                    break
            c.stdin.close()
            c.wait(10)
            c.stdout.close()
            m = re.search(rb"SENTINEL term=(\S+) size=(\d+ \d+)", out)
            self.assertIsNotNone(m, out)
            self.assertEqual(m.group(1), b"xterm-256color")
            self.assertEqual(m.group(2), b"43 132")
            self.assertNotIn(PW.encode(), out)  # the server echoes nothing of it, the client neither
        finally:
            srv.stop()


if __name__ == "__main__":
    unittest.main(verbosity=1)
