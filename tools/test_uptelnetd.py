#!/usr/bin/env python3
"""Tests of tools/uptelnetd.py (ledger A1.2), run by `make test`.

The rules that keep it on the LAN (bind and allowlist), the password's
source and the lockout are tested as functions; one test drives the real
entry point (main, in a subprocess, as the owner starts it) end to end:
refused peers, a wrong password, the right one, then the shell's pty with
the client's TERM and window size. A second sends the same login through
build/tn_host -- uptelnet's own protocol code (net/tn.c) -- so the Amiga's
client and the Mac's server are proved to agree.

The key login (plan 2026-10-14-nas-claude-local.md NL6-NL9) has its units here (the clients
file, the server's AUTH half of Telnet, the vector shared with tests/test_telnet.c, the
configure rules) and ONE reachability test: build/tn_host with KEY= -- the real net/tn.c --
through uptelnetd's main() logs in with no Password: on the line, the server log says "key
login" (the sentinel); with an empty key file it pairs after the password and the next run
needs none; a revoked ID is sent back to the password; a server that fails the proof is
dropped by the client.
"""
import hashlib
import hmac
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
WONT_AUTH = bytes([255, 252, 37])     # a client with no key: today's answer to an option it does not know
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


# the vector tests/test_telnet.c pins in C: same K, sn, cn, ID, same bytes
K_VEC = bytes(range(32))
SN_VEC = bytes([0xff, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0xfe])
CN_VEC = bytes([0x20, 0xff] + list(range(0x22, 0x30)))
MAC_VEC = bytes.fromhex("7dd70132a3e24c22590aba7ee40ac9efae39edb5100d73abaaf37d5895d260d2")
SMAC_VEC = bytes.fromhex("aecac79e686576e2c254b18fb0d1b1865b89ef395872e5e169bc278bf529dbd8")


class KeyLogin(unittest.TestCase):
    def test_the_hmacs_are_the_vector_the_c_client_pins(self):
        self.assertEqual(d.hmac_sha256(K_VEC, b"upt1-client", SN_VEC, CN_VEC, b"amiga-1"), MAC_VEC)
        self.assertEqual(d.hmac_sha256(K_VEC, b"upt1-server", SN_VEC, CN_VEC), SMAC_VEC)

    def test_the_server_asks_for_auth_only_when_it_can_check_one(self):
        t = d.Telnet(auth=True)
        t.start()
        self.assertIn(bytes([255, 253, 37]), bytes(t.out))
        t = d.Telnet()
        t.start()
        self.assertNotIn(bytes([255, 253, 37]), bytes(t.out))

    def test_will_auth_gets_the_challenge_with_the_nonce_doubled(self):
        t = d.Telnet(auth=True)
        t.start()
        t.out.clear()
        t.sn = SN_VEC
        t.feed(bytes([255, 251, 37]))
        self.assertEqual(bytes(t.out), bytes([255, 250, 37, 1, 0x75, 255, 255]) + SN_VEC[1:15] + bytes([0xfe, 255, 240]))

    def test_the_answer_is_parsed_bytewise_with_doubled_255(self):
        t = d.Telnet(auth=True)
        t.start()
        t.feed(bytes([255, 251, 37]))
        msg = bytes([255, 250, 37, 0, 0x75]) + (b"amiga-1\0" + CN_VEC + MAC_VEC).replace(b"\xff", b"\xff\xff") + bytes([255, 240])
        for c in msg:
            t.feed(bytes([c]))
        self.assertEqual(t.auth_msg, (b"amiga-1", CN_VEC, MAC_VEC))

    def test_a_pairing_request_has_no_id_and_no_mac(self):
        t = d.Telnet(auth=True)
        t.start()
        t.feed(bytes([255, 251, 37]))
        t.feed(bytes([255, 250, 37, 0, 0x75, 0]) + CN_VEC.replace(b"\xff", b"\xff\xff") + bytes([255, 240]))
        self.assertEqual(t.auth_msg, (b"", CN_VEC, None))

    def test_a_malformed_answer_becomes_an_id_nobody_has_and_a_second_one_is_ignored(self):
        t = d.Telnet(auth=True)
        t.start()
        t.feed(bytes([255, 251, 37]))
        t.feed(bytes([255, 250, 37, 0, 0x75, 1, 2, 3, 255, 240]))
        self.assertEqual(t.auth_msg[0], b"\0malformed")
        t2 = d.Telnet(auth=True)
        t2.start()
        t2.feed(bytes([255, 251, 37]))
        t2.feed(bytes([255, 250, 37, 0, 0x75]) + b"x\0" + CN_VEC + bytes([255, 240]))
        first = t2.auth_msg
        t2.feed(bytes([255, 250, 37, 0, 0x75]) + b"y\0" + CN_VEC + bytes([255, 240]))
        self.assertEqual(t2.auth_msg, first)

    def test_an_unsolicited_auth_answer_is_ignored_without_the_option(self):
        t = d.Telnet()          # auth off: this server never offered it
        t.start()
        t.feed(bytes([255, 250, 37, 0, 0x75, 0]) + CN_VEC + bytes([255, 240]))
        self.assertIsNone(t.auth_msg)


class Clients(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, "sub", "clients")
        self.c = d.ClientStore(self.path)

    def tearDown(self):
        self.dir.cleanup()

    def test_add_list_revoke_with_the_file_kept_private(self):
        self.assertEqual(self.c.rows(), [])
        cid, key = self.c.add("192.168.0.77")
        self.assertRegex(cid, r"^amiga-[0-9a-f]{6}$")
        self.assertEqual(len(key), 32)
        self.assertEqual(stat.S_IMODE(os.stat(self.path).st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(os.stat(os.path.dirname(self.path)).st_mode), 0o700)
        cid2, _ = self.c.add("192.168.0.78", cid="studio")
        self.assertEqual(self.c.keys()[cid], key)
        rows = self.c.rows()
        self.assertEqual([r[0] for r in rows], [cid, "studio"])
        self.assertEqual(rows[1][3], "192.168.0.78")
        self.assertEqual(open(self.path).read().split()[1], key.hex())
        with self.assertRaises(d.ConfigError):
            self.c.add(cid=cid)                   # an ID is paired once
        with self.assertRaises(d.ConfigError):
            self.c.add(cid="bad id")
        self.assertTrue(self.c.revoke(cid))
        self.assertFalse(self.c.revoke(cid))
        self.assertEqual([r[0] for r in self.c.rows()], ["studio"])
        self.assertEqual(stat.S_IMODE(os.stat(self.path).st_mode), 0o600)

    def test_a_file_others_can_read_gives_no_keys_and_no_pairing(self):
        self.c.add(cid="studio")
        os.chmod(self.path, 0o644)
        self.assertIn("chmod 600", self.c.check())
        self.assertEqual(self.c.rows(), [])
        with self.assertRaises(d.ConfigError):
            self.c.add(cid="other")
        with self.assertRaises(d.ConfigError):
            self.c.revoke("studio")

    def test_a_link_or_someone_elses_file_is_refused(self):
        self.c.add(cid="studio")
        link = os.path.join(self.dir.name, "link")
        os.symlink(self.path, link)
        self.assertIn("not a regular file", d.ClientStore(link).check())
        self.assertIn("someone else", d.ClientStore(self.path, uid=os.getuid() + 1).check())

    def test_bad_lines_are_skipped(self):
        os.makedirs(os.path.dirname(self.path))
        with open(self.path, "w") as f:
            f.write("# comment\nshort 00ff\nbad/id %s\ngood %s 2026-10-14 10.0.0.1\n" % ("00" * 32, "ab" * 32))
        os.chmod(self.path, 0o600)
        self.assertEqual([r[0] for r in self.c.rows()], ["good"])

    def test_the_clients_file_stands_in_for_the_password_but_neither_alone_is_nothing(self):
        env = {}
        args = lambda *extra: d.parse_args(["--bind", "127.0.0.1", "--password-file", os.path.join(self.dir.name, "pw"),
                                            "--clients-file", self.path] + list(extra))
        with self.assertRaises(d.ConfigError) as cm:
            d.configure(args(), env=env)
        self.assertIn("no password", str(cm.exception))
        self.c.add(cid="studio")
        _, _, password, _ = d.configure(args(), env=env)
        self.assertIsNone(password)
        os.chmod(self.path, 0o644)
        with self.assertRaises(d.ConfigError) as cm:
            d.configure(args(), env=env)
        self.assertIn("chmod 600", str(cm.exception))
        os.chmod(self.path, 0o600)
        _, _, password, _ = d.configure(args(), env={d.PASSWORD_ENV: PW})
        self.assertEqual(password, PW)

    def test_a_short_password_is_still_an_error_even_with_clients(self):
        self.c.add(cid="studio")
        with self.assertRaises(d.ConfigError):
            d.configure(d.parse_args(["--bind", "127.0.0.1", "--clients-file", self.path]), env={d.PASSWORD_ENV: "short"})


class PairsTool(unittest.TestCase):
    """tools/nas/pairs (NL12): list, add, revoke against a temp clients file."""
    PAIRS = ROOT / "tools/nas/pairs"

    def pairs(self, path, *args):
        return subprocess.run([sys.executable, str(self.PAIRS), "--file", path] + list(args),
                              capture_output=True, text=True)

    def test_list_add_revoke_and_the_file_modes(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "nas", "clients")
            r = self.pairs(path, "list")
            self.assertEqual(r.returncode, 0)
            self.assertIn("no Amiga is paired", r.stdout)
            r = self.pairs(path, "add")
            self.assertEqual(r.returncode, 0, r.stderr)
            m = re.search(r"^(amiga-[0-9a-f]{6}) ([0-9a-f]{64})$", r.stdout, re.M)
            self.assertIsNotNone(m, r.stdout)
            self.assertEqual(stat.S_IMODE(os.stat(path).st_mode), 0o600)
            self.assertEqual(d.ClientStore(path).keys()[m.group(1)].hex(), m.group(2))
            r = self.pairs(path, "list")
            self.assertIn(m.group(1), r.stdout)
            self.assertNotIn(m.group(2), r.stdout)               # the key is never listed
            r = self.pairs(path, "revoke", "nope")
            self.assertEqual(r.returncode, 1)
            r = self.pairs(path, "revoke", m.group(1))
            self.assertEqual(r.returncode, 0)
            self.assertEqual(d.ClientStore(path).rows(), [])
            os.chmod(path, 0o644)
            r = self.pairs(path, "add")
            self.assertEqual(r.returncode, 2)
            self.assertIn("chmod 600", r.stderr)

    def test_the_container_files_carry_it(self):
        nas = ROOT / "tools/nas"
        self.assertIn("COPY pairs /usr/local/bin/pairs", (nas / "Dockerfile").read_text())
        self.assertIn("image: claude-amiga:5", (nas / "compose.yaml").read_text())
        ep = (nas / "entrypoint.sh").read_text()
        self.assertIn('[ ! -s "$CLIENTS" ]', ep)                 # it starts with a password OR a clients file
        self.assertIn('/home/claude/.config/uptelnetd/clients', ep)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Server:
    """uptelnetd started through its real entry point, as the owner does."""

    def __init__(self, *extra, command=SENTINEL_CMD, clients=None, password=True):
        self.port = free_port()
        self.tmp = tempfile.TemporaryDirectory()
        # never the real ~/.config/uptelnetd/clients: a file of this test's own (absent until paired)
        self.clients = clients or os.path.join(self.tmp.name, "clients")
        env = {k: v for k, v in os.environ.items() if k != d.PASSWORD_ENV}
        if password:
            env[d.PASSWORD_ENV] = PW
        self.p = subprocess.Popen([sys.executable, str(ROOT / "tools/uptelnetd.py"), "--bind", "127.0.0.1",
                                   "--port", str(self.port), "--shell", "/bin/sh", "--command", command,
                                   "--clients-file", self.clients, "--password-file",
                                   os.path.join(self.tmp.name, "no-password")] + list(extra),
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
        self.log = self.p.stderr.read().decode("utf-8", "replace")
        self.p.stderr.close()
        self.tmp.cleanup()


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
            s.sendall(WONT_AUTH + bytes([255, 251, 31, 255, 250, 31, 0, 100, 0, 30, 255, 240, 255, 251, 24]))
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
            s.sendall(WONT_AUTH)
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

    # ---- the key login, through main() with the real net/tn.c ----

    def run_tn_host(self, srv, keyfile, send_password=True, timeout=12):
        """build/tn_host KEY=keyfile against srv: (stdout, stderr, returncode); types the password when asked."""
        tn_host = ROOT / "build/tn_host"
        if not tn_host.exists():
            self.fail("build/tn_host missing: make build/tn_host (make test builds it)")
        c = subprocess.Popen([str(tn_host), "127.0.0.1", str(srv.port), "132", "43", "KEY=" + keyfile],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out = b""
        end = time.time() + timeout
        sent = False
        while time.time() < end:
            r, _, _ = select.select([c.stdout], [], [], 0.1)
            if r:
                chunk = os.read(c.stdout.fileno(), 4096)
                if not chunk:
                    break
                out += chunk
            if send_password and not sent and b"Password: " in out:
                c.stdin.write(PW.encode() + b"\n")
                c.stdin.flush()
                sent = True
            if re.search(rb"SENTINEL[^\n]*\n", out):
                break
            if not send_password and b"Password: " in out:
                break
        try:
            c.stdin.close()
        except OSError:
            pass
        try:
            rc = c.wait(10)
        except subprocess.TimeoutExpired:
            c.kill()
            rc = c.wait()
        err = c.stderr.read()
        c.stdout.close()
        c.stderr.close()
        return out, err, rc

    def test_key_login_through_main_pairs_once_then_needs_no_password(self):
        with tempfile.TemporaryDirectory() as tmp:
            keyfile = os.path.join(tmp, "nas-key")
            open(keyfile, "w").close()                 # an empty key file asks to be paired
            srv = Server()
            try:
                # 1: the password, once: the server hands a key over, the client writes the file
                out, err, _ = self.run_tn_host(srv, keyfile)
                self.assertIn(b"Password: ", out)
                self.assertRegex(out, rb"SENTINEL term=xterm-256color size=43 132")
                line = open(keyfile).read()
                m = re.match(r"^(amiga-[0-9a-f]{6}) ([0-9a-f]{64})\n$", line)
                self.assertIsNotNone(m, line)
                store = d.ClientStore(srv.clients)
                self.assertEqual(store.keys()[m.group(1)].hex(), m.group(2))
                self.assertEqual(stat.S_IMODE(os.stat(srv.clients).st_mode), 0o600)
                # 2: the second run: no Password: on the line at all, the server log says key login
                out, err, _ = self.run_tn_host(srv, keyfile, send_password=False)
                self.assertNotIn(b"Password", out)
                self.assertRegex(out, rb"SENTINEL term=xterm-256color size=43 132")
                self.assertIn(b"auth ok", err)
                self.assertNotIn(PW.encode(), out)
                # 3: revoked on the server: back to the password, and the client says it was rejected
                store.revoke(m.group(1))
                out, err, _ = self.run_tn_host(srv, keyfile)
                self.assertIn(b"Password: ", out)
                self.assertIn(b"auth rejected", err)
            finally:
                srv.stop()
            self.assertIn("paired as amiga-", srv.log)
            self.assertEqual(srv.log.count("key login amiga-"), 1)        # the sentinel: the second run only
            self.assertIn("key rejected (unknown or revoked ID)", srv.log)

    def test_a_wrong_key_is_rejected_and_five_of_them_lock_the_address(self):
        with tempfile.TemporaryDirectory() as tmp:
            store = d.ClientStore(os.path.join(tmp, "clients"))
            cid, key = store.add(cid="studio")
            srv = Server(clients=store.path)
            try:
                for _ in range(d.LOCK_FAILURES):
                    s = socket.create_connection(("127.0.0.1", srv.port))
                    s.sendall(bytes([255, 251, 37]))
                    got = read_until(s, rb"(?s)\xff\xfa\x25\x01\x75.{16,32}\xff\xf0", timeout=5)
                    m = re.search(rb"\xff\xfa\x25\x01\x75(.*?)\xff\xf0", got, re.S)
                    self.assertIsNotNone(m, got)
                    cn = bytes(16)
                    bad = hmac.new(os.urandom(32), b"x", hashlib.sha256).digest()
                    s.sendall(bytes([255, 250, 37, 0, 0x75]) + (cid.encode() + b"\0" + cn + bad).replace(b"\xff", b"\xff\xff")
                              + bytes([255, 240]))
                    got = read_until(s, rb"\xff\xfa\x25\x02\x75\x00\xff\xf0", timeout=5)
                    self.assertIn(bytes([255, 250, 37, 2, 0x75, 0, 255, 240]), got)    # REJECT
                    s.close()
                s = socket.create_connection(("127.0.0.1", srv.port))
                self.assertEqual(read_until(s, rb"Password", timeout=1.5), b"")        # locked out: closed at once
                s.close()
            finally:
                srv.stop()
            self.assertIn("locked out", srv.log)

    def test_a_right_key_by_hand_gets_the_servers_proof_and_no_prompt(self):
        with tempfile.TemporaryDirectory() as tmp:
            store = d.ClientStore(os.path.join(tmp, "clients"))
            cid, key = store.add(cid="studio")
            srv = Server(clients=store.path)
            try:
                s = socket.create_connection(("127.0.0.1", srv.port))
                s.sendall(bytes([255, 251, 37]))
                got = read_until(s, rb"(?s)\xff\xfa\x25\x01\x75.*?\xff\xf0", timeout=5)
                m = re.search(rb"\xff\xfa\x25\x01\x75(.*?)(?<!\xff)\xff\xf0", got, re.S)
                sn = m.group(1).replace(b"\xff\xff", b"\xff")
                self.assertEqual(len(sn), 16)
                cn = os.urandom(16)
                mac = d.hmac_sha256(key, b"upt1-client", sn, cn, cid.encode())
                s.sendall(bytes([255, 250, 37, 0, 0x75]) + (cid.encode() + b"\0" + cn + mac).replace(b"\xff", b"\xff\xff")
                          + bytes([255, 240]))
                got = read_until(s, rb"SENTINEL[^\n]*\n|Password", timeout=8)
                self.assertNotIn(b"Password", got)
                smac = d.hmac_sha256(key, b"upt1-server", sn, cn)
                self.assertIn(bytes([255, 250, 37, 2, 0x75, 1]) + smac.replace(b"\xff", b"\xff\xff") + bytes([255, 240]), got)
                s.close()
            finally:
                srv.stop()

    def test_old_clients_still_get_the_password_prompt(self):
        srv = Server()
        try:
            # answers WONT: at once
            s = socket.create_connection(("127.0.0.1", srv.port))
            t0 = time.time()
            s.sendall(WONT_AUTH)
            self.assertIn(b"Password: ", read_until(s, rb"Password: "))
            self.assertLess(time.time() - t0, d.AUTH_WAIT - 1)
            s.close()
            # never answers (a telnet that ignores unknown options): after AUTH_WAIT
            s = socket.create_connection(("127.0.0.1", srv.port))
            t0 = time.time()
            self.assertIn(b"Password: ", read_until(s, rb"Password: ", timeout=d.AUTH_WAIT + 4))
            self.assertGreaterEqual(time.time() - t0, d.AUTH_WAIT - 0.3)
            s.close()
        finally:
            srv.stop()

    def test_a_server_with_only_keys_closes_on_a_client_without_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            store = d.ClientStore(os.path.join(tmp, "clients"))
            store.add(cid="studio")
            srv = Server(clients=store.path, password=False)
            try:
                s = socket.create_connection(("127.0.0.1", srv.port))
                s.sendall(WONT_AUTH)
                got = read_until(s, rb"key only", timeout=5)
                self.assertIn(b"logs in with a key only", got)
                self.assertNotIn(b"Password", got)
                s.close()
            finally:
                srv.stop()

    def test_the_client_drops_a_server_that_fails_the_proof(self):
        """A fake NAS that sends SEND and then a wrong ACCEPT: tn_host (net/tn.c) ends the line."""
        tn_host = ROOT / "build/tn_host"
        with tempfile.TemporaryDirectory() as tmp:
            keyfile = os.path.join(tmp, "nas-key")
            with open(keyfile, "w") as f:
                f.write("studio %s\n" % ("11" * 32))
            ls = socket.socket()
            ls.bind(("127.0.0.1", 0))
            ls.listen(1)
            port = ls.getsockname()[1]
            c = subprocess.Popen([str(tn_host), "127.0.0.1", str(port), "80", "24", "KEY=" + keyfile],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            conn, _ = ls.accept()
            conn.sendall(bytes([255, 253, 37]))
            read_until(conn, rb"\xff\xfb\x25")
            conn.sendall(bytes([255, 250, 37, 1, 0x75]) + bytes(16) + bytes([255, 240]))
            read_until(conn, rb"\xff\xf0", timeout=3)
            conn.sendall(bytes([255, 250, 37, 2, 0x75, 1]) + bytes(32) + bytes([255, 240]) + b"Password: ")
            rc = c.wait(10)
            out = c.stdout.read()
            err = c.stderr.read()
            c.stdin.close()
            c.stdout.close()
            c.stderr.close()
            conn.close()
            ls.close()
            self.assertEqual(rc, 3)
            self.assertIn(b"auth bad-server", err)
            self.assertNotIn(b"Password", out)             # nothing from that server is shown


if __name__ == "__main__":
    unittest.main(verbosity=1)
