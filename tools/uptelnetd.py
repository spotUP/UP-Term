#!/usr/bin/env python3
"""uptelnetd -- the Mac end of "Claude from the Amiga" (ledger A1.2).

A small telnet server for the LAN only: a password, then a pty with your
shell, or straight into a command (tmux + claude). The Amiga connects with
C:uptelnet in an UP-Term window.

    UNENCRYPTED. Telnet sends everything in clear, the password too. Run it
    only on a network you trust (your home LAN), never forward its port
    through a router, and stop it when you are done. For an encrypted path
    use ssh (BebboSSH from the kit) to the Mac's Remote Login instead.

Start (nothing is installed; it runs until Ctrl-C):

    (umask 077; mkdir -p ~/.config/uptelnetd; read -rs p; printf '%s\\n' "$p" > ~/.config/uptelnetd/password)
    python3 tools/uptelnetd.py --command 'tmux new -A -s claude claude'

Without --command you get a login shell ($SHELL -l, interactive, your rc
files read), where `claude` or `tmux new -A -s claude` run as at the Mac.
--command runs through `$SHELL -lc`: login files are read but not the
interactive rc, so a shell FUNCTION named claude is not there -- write what
it does (e.g. --command 'tmux new -A -s claude env CLAUDE_CONFIG_DIR=$HOME/.claude-private claude').

Safety, all enforced here:
  * It listens on addresses of a private network (the Mac's own LAN
    address by default, found with `ipconfig getifaddr en0`; --bind again
    for each further one, e.g. 127.0.0.1 where a VPN such as Tailscale hands
    its connections to loopback), never on 0.0.0.0 or ::, and refuses a
    public address outright.
  * Only peers in the allowlist get a prompt: the bind address's own
    subnet by default (`ipconfig getoption <if> subnet_mask`), or the
    --allow networks, which must be private too. Others are closed at once.
  * The password comes from $UPTELNETD_PASSWORD or a file under your home
    (default ~/.config/uptelnetd/password) that only you can read (mode
    0600 or stricter, owned by you); never from the command line, where ps
    would show it. At least 8 characters. Compared in constant time.
  * Failures cost time: 2 s after each wrong password, 3 tries per
    connection, and 5 failures from one address in 10 minutes lock that
    address out for 10 minutes. At most 4 logins at once; 60 s to log in.
  * The session's environment is built from a short list (HOME, USER,
    PATH, LANG...): the password variable never reaches the shell.

Key login (plan 2026-10-14-nas-claude-local.md, NL6/NL8; the client is net/tn.c, whose
header net/tn.h holds the same text). The password crossed the LAN at every start;
with a key per Amiga the reusable secret never does. Challenge-response over the
telnet link, HMAC-SHA256, RFC 2941 framing (option AUTHENTICATION 37, IS 0, SEND 1,
REPLY 2) with one private auth-type 0x75 ("UPT1"); binary fields are plain bytes in
the subnegotiation, a 255 in them doubled like any SB data. K = 32-byte key,
ID = 1 to 16 characters of [A-Za-z0-9._-], sn = 16 server nonce bytes (os.urandom),
cn = 16 client nonce bytes, HMAC(K, m) = HMAC-SHA256.

  1 server  IAC DO AUTH beside its other offers. A client without a key answers
            WONT and gets "Password:" exactly as before (so does a client that
            never answers: after AUTH_WAIT seconds).
  2 client  IAC WILL AUTH.
  3 server  SB AUTH SEND 0x75 sn[16]
  4 client  SB AUTH IS 0x75 ID 0x00 cn[16] mac[32],
            mac = HMAC(K, "upt1-client" | sn | cn | ID).
            A client asking to be paired sends ID empty and no mac.
  5 server  mac good: SB AUTH REPLY 0x75 ACCEPT smac[32],
            smac = HMAC(K, "upt1-server" | sn | cn); no Password prompt; the log
            says "key login ID". The client checks smac and closes the line on a
            bad one (a fake NAS cannot make it). mac bad or ID unknown or revoked:
            SB AUTH REPLY 0x75 REJECT, the failure counts in the rate limiter,
            then "Password:" as before.
  6 server  pairing: after a good password from a client that asked (empty ID) the
            server makes ID and K, appends them to the clients file and sends
            SB AUTH REPLY 0x75 PAIR ID 0x00 K[32]. The client keeps them; its next
            start is step 5.

The clients file (--clients-file, default ~/.config/uptelnetd/clients; 0600, yours, a
regular file, like the password file) has one line per Amiga: "ID hexkey date
first-peer". It is read at every connection, so `pairs add` and `pairs revoke`
(tools/nas/pairs) act at once. The server starts with a password, a clients file or
both; with no password only keys log in. The wire shows one-time values (sn, cn,
mac, smac) and, once at the pairing, ID and K, which is as exposed as the one
password typed to authorise it. It protects the login, not the session: the session
is plain TCP as before.

Python 3.9+ standard library only (macOS's /usr/bin/python3 will do).
"""
import argparse
import asyncio
import fcntl
import hashlib
import hmac
import ipaddress
import os
import pwd
import re
import selectors
import shlex
import stat
import struct
import subprocess
import sys
import termios
import time

PASSWORD_ENV = "UPTELNETD_PASSWORD"
DEFAULT_PASSWORD_FILE = "~/.config/uptelnetd/password"
DEFAULT_PORT = 2323
MIN_PASSWORD = 8

FAIL_DELAY = 2.0          # seconds after each wrong password
TRIES_PER_CONNECTION = 3
LOCK_FAILURES = 5         # failures from one address ...
LOCK_WINDOW = 600.0       # ... within this many seconds ...
LOCK_TIME = 600.0         # ... lock it out this long
MAX_PENDING = 4           # connections not logged in yet
LOGIN_TIMEOUT = 60.0
AUTH_WAIT = 3.0           # a client that answers neither WILL nor WONT to DO AUTH gets the password prompt
DEFAULT_CLIENTS_FILE = "~/.config/uptelnetd/clients"
WRITE_HIGH = 256 * 1024   # stop reading the pty while this much waits for the Amiga

# The networks a LAN address can be on. Loopback is allowed too (it is
# safer still: nothing off the Mac reaches it).
PRIVATE_NETS = [ipaddress.ip_network(n) for n in (
    "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "169.254.0.0/16", "127.0.0.0/8",
    "fc00::/7", "fe80::/10", "::1/128")]


class ConfigError(Exception):
    pass


class NoPassword(ConfigError):
    """There is no password (not: a bad one). A clients file can stand in for it."""


def is_lan(addr):
    a = ipaddress.ip_address(addr)
    return any(a in n for n in PRIVATE_NETS if n.version == a.version)


def check_bind(addr):
    """The address to listen on, or ConfigError: one private address, never
    a wildcard, never a public one."""
    try:
        a = ipaddress.ip_address(addr)
    except ValueError:
        raise ConfigError("--bind %r is not an IP address" % addr)
    if a.is_unspecified:
        raise ConfigError("refusing to listen on %s: that is every interface, the Internet's side too" % a)
    if a.is_multicast or not is_lan(a):
        raise ConfigError("refusing to listen on %s: not a private LAN address" % a)
    return str(a)


def check_allow(cidr):
    try:
        n = ipaddress.ip_network(cidr, strict=False)
    except ValueError:
        raise ConfigError("--allow %r is not a network (like 192.168.1.0/24)" % cidr)
    if not any(n.version == p.version and n.subnet_of(p) for p in PRIVATE_NETS):
        raise ConfigError("refusing --allow %s: it reaches past the private LAN ranges" % n)
    return n


def default_allow(bind, mask=None):
    """The bind address's subnet: the mask the interface has, else a /24."""
    a = ipaddress.ip_address(bind)
    if a.is_loopback:
        return [ipaddress.ip_network("127.0.0.0/8") if a.version == 4 else ipaddress.ip_network("::1/128")]
    if a.version == 4:
        return [check_allow("%s/%s" % (bind, mask or "255.255.255.0"))]
    return [check_allow("%s/64" % bind)]


def allowed(peer, nets):
    try:
        a = ipaddress.ip_address(peer)
    except ValueError:
        return False
    if a.version == 6 and a.ipv4_mapped:
        a = a.ipv4_mapped
    return any(a.version == n.version and a in n for n in nets)


def _run(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=5).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def lan_address():
    """(address, interface) of the Mac's LAN: en0, then en1."""
    for ifname in ("en0", "en1"):
        a = _run(["ipconfig", "getifaddr", ifname])
        if a:
            return a, ifname
    return None, None


def load_password(env=None, path=None, uid=None):
    """The password: $UPTELNETD_PASSWORD, else the file, which must be a
    regular file of yours that nobody else can read or write."""
    env = os.environ if env is None else env
    uid = os.getuid() if uid is None else uid
    if env.get(PASSWORD_ENV):
        pw = env[PASSWORD_ENV]
    else:
        p = os.path.expanduser(path or DEFAULT_PASSWORD_FILE)
        try:
            st = os.lstat(p)
        except FileNotFoundError:
            raise NoPassword("no password: set %s, or write one to %s (mode 0600)" % (PASSWORD_ENV, p))
        if not stat.S_ISREG(st.st_mode):
            raise ConfigError("%s is not a regular file" % p)
        if st.st_uid != uid:
            raise ConfigError("%s belongs to someone else" % p)
        if st.st_mode & 0o077:
            raise ConfigError("%s can be read by others (mode %o): chmod 600 it" % (p, st.st_mode & 0o777))
        with open(p, "r", encoding="utf-8") as f:
            pw = f.readline().rstrip("\r\n")
    if len(pw) < MIN_PASSWORD:
        raise ConfigError("the password is shorter than %d characters" % MIN_PASSWORD)
    return pw


def password_ok(given, want):
    return hmac.compare_digest(given.encode("utf-8", "replace"), want.encode("utf-8", "replace"))


class RateLimiter:
    """Failures per peer address; an address with LOCK_FAILURES within
    LOCK_WINDOW seconds is locked out for LOCK_TIME seconds."""

    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.fails = {}
        self.locked = {}

    def is_locked(self, peer):
        until = self.locked.get(peer)
        if until is None:
            return False
        if self.clock() >= until:
            del self.locked[peer]
            self.fails.pop(peer, None)
            return False
        return True

    def failure(self, peer):
        now = self.clock()
        recent = [t for t in self.fails.get(peer, []) if now - t < LOCK_WINDOW] + [now]
        self.fails[peer] = recent
        if len(recent) >= LOCK_FAILURES:
            self.locked[peer] = now + LOCK_TIME
            return True
        return False

    def success(self, peer):
        self.fails.pop(peer, None)


# ---- the clients file (key login) ---------------------------------------------

AUTH_TYPE = 0x75
ID_RE = re.compile(r"^[A-Za-z0-9._-]{1,16}$")
KEY_RE = re.compile(r"^[0-9a-f]{64}$")


def hmac_sha256(key, *parts):
    return hmac.new(key, b"".join(parts), hashlib.sha256).digest()


def session_key(key, sn, cn):
    """The session daemon's key (wasabid): hex HMAC-SHA256(K, "wasabi-session" | sn | cn), the
    string net/tn.c's tn_session_key makes on the Amiga; the relay authenticates with it."""
    return hmac_sha256(key, b"wasabi-session", sn, cn).hex()


class ClientStore:
    """The clients file: one line per paired Amiga, "ID hexkey date first-peer". Read whole
    at every use, so a `pairs add` or `pairs revoke` (another process) acts at the next
    login. The file must be a regular file of yours that others cannot read or write, the
    checks load_password makes; a file that fails them gives no keys."""

    def __init__(self, path=None, uid=None):
        self.path = os.path.expanduser(path or DEFAULT_CLIENTS_FILE)
        self.uid = os.getuid() if uid is None else uid

    def check(self):
        """None when the file is fine or absent; else the reason it is refused."""
        try:
            st = os.lstat(self.path)
        except FileNotFoundError:
            return None
        if not stat.S_ISREG(st.st_mode):
            return "%s is not a regular file" % self.path
        if st.st_uid != self.uid:
            return "%s belongs to someone else" % self.path
        if st.st_mode & 0o077:
            return "%s can be read by others (mode %o): chmod 600 it" % (self.path, st.st_mode & 0o777)
        return None

    def exists(self):
        return os.path.lexists(self.path)

    def rows(self):
        """[(id, key bytes, date, peer)] of the good lines; [] when the file is absent or refused."""
        if self.check() or not os.path.exists(self.path):
            return []
        out = []
        with open(self.path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                w = line.split()
                if len(w) >= 2 and ID_RE.match(w[0]) and KEY_RE.match(w[1].lower()):
                    out.append((w[0], bytes.fromhex(w[1]), w[2] if len(w) > 2 else "-", w[3] if len(w) > 3 else "-"))
        return out

    def keys(self):
        return {r[0]: r[1] for r in self.rows()}

    def new_id(self):
        have = {r[0] for r in self.rows()}
        while True:
            cid = "amiga-" + os.urandom(3).hex()
            if cid not in have:
                return cid

    def add(self, peer="-", cid=None, key=None):
        """A new pairing appended (the file made 0600): (id, key). Refuses a file that fails check()."""
        why = self.check()
        if why:
            raise ConfigError(why)
        cid = cid or self.new_id()
        if not ID_RE.match(cid):
            raise ConfigError("an ID is 1 to 16 characters of letters, digits . _ -")
        if cid in {r[0] for r in self.rows()}:
            raise ConfigError("%s is paired already" % cid)
        key = key or os.urandom(32)
        d = os.path.dirname(self.path)
        if d:
            os.makedirs(d, mode=0o700, exist_ok=True)
        fd = os.open(self.path, os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o600)
        with os.fdopen(fd, "a", encoding="utf-8") as f:
            f.write("%s %s %s %s\n" % (cid, key.hex(), time.strftime("%Y-%m-%d"), peer))
        return cid, key

    def revoke(self, cid):
        """The line of cid removed: True when there was one."""
        why = self.check()
        if why:
            raise ConfigError(why)
        if not os.path.exists(self.path):
            return False
        with open(self.path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
        keep = [l for l in lines if l.split()[:1] != [cid]]
        if len(keep) == len(lines):
            return False
        tmp = self.path + ".tmp"
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.writelines(keep)
        os.replace(tmp, self.path)
        return True


# ---- telnet (server side) -----------------------------------------------------

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240
BINARY, ECHO, SGA, TTYPE, NAWS, AUTH = 0, 1, 3, 24, 31, 37
AUTH_IS, AUTH_SEND, AUTH_REPLY = 0, 1, 2
AUTH_REJECT, AUTH_ACCEPT, AUTH_PAIR = 0, 1, 2
SB_MAX = 128
TTYPE_IS, TTYPE_SEND = 0, 1
NO, YES, WANTNO, WANTYES = 0, 1, 2, 3
US_OK = {ECHO, SGA, BINARY}           # we will do these
HIM_OK = {BINARY, SGA, NAWS, TTYPE}   # we want the client to do these


class Telnet:
    """The server's half of RFC 854/1143: feed() takes the client's bytes
    and returns the data in them; answers go to self.out (bytes for the
    client). The window size and terminal type arrive as attributes."""

    def __init__(self, auth=False):
        self.auth = auth      # offer the key login (DO AUTH)
        self.sn = os.urandom(16)
        self.pair_wanted = False
        self.auth_msg = None  # (id bytes, cn, mac or None) of the client's IS
        self.us = {}
        self.him = {}
        self.out = bytearray()
        self.st = 0
        self.verb = 0
        self.sb = bytearray()
        self.cr = False
        self.size = None      # (cols, rows)
        self.term = None
        self.size_changed = False

    def start(self):
        for o in (ECHO, SGA, BINARY):
            self.us[o] = WANTYES
            self.out += bytes([IAC, WILL, o])
        for o in (BINARY, NAWS, TTYPE) + ((AUTH,) if self.auth else ()):
            self.him[o] = WANTYES
            self.out += bytes([IAC, DO, o])

    def _cmd(self, verb, opt):
        self.out += bytes([IAC, verb, opt])

    def sb_out(self, *parts):
        """IAC SB <parts> IAC SE with every 255 inside doubled."""
        self.out += bytes([IAC, SB]) + b"".join(parts).replace(b"\xff", b"\xff\xff") + bytes([IAC, SE])

    def _him_on(self, opt):
        if opt == TTYPE:
            self.out += bytes([IAC, SB, TTYPE, TTYPE_SEND, IAC, SE])
        elif opt == AUTH:
            self.sb_out(bytes([AUTH, AUTH_SEND, AUTH_TYPE]), self.sn)

    def _got(self, verb, opt):
        if verb in (WILL, WONT):
            q = self.him.get(opt, NO)
            if verb == WILL:
                if q == NO:
                    if opt in HIM_OK:
                        self.him[opt] = YES
                        self._cmd(DO, opt)
                        self._him_on(opt)
                    else:
                        self._cmd(DONT, opt)
                elif q == WANTNO:
                    self.him[opt] = NO
                elif q == WANTYES:
                    self.him[opt] = YES
                    self._him_on(opt)
            else:
                if q == YES:
                    self.him[opt] = NO
                    self._cmd(DONT, opt)
                elif q in (WANTNO, WANTYES):
                    self.him[opt] = NO
            return
        q = self.us.get(opt, NO)
        if verb == DO:
            if q == NO:
                if opt in US_OK:
                    self.us[opt] = YES
                    self._cmd(WILL, opt)
                else:
                    self._cmd(WONT, opt)
            elif q == WANTNO:
                self.us[opt] = NO
            elif q == WANTYES:
                self.us[opt] = YES
        else:
            if q == YES:
                self.us[opt] = NO
                self._cmd(WONT, opt)
            elif q in (WANTNO, WANTYES):
                self.us[opt] = NO

    def _auth_is(self, sb):
        """SB AUTH IS 0x75 ID 0x00 cn[16] [mac[32]]; a malformed one becomes an ID nobody has."""
        rest = sb[3:]
        i = rest.find(b"\0")
        tail = rest[i + 1:] if i >= 0 else b""
        if i < 0 or len(tail) not in (16, 48) or self.auth_msg is not None:
            if self.auth_msg is None:
                self.auth_msg = (b"\0malformed", b"", None)
            return
        self.auth_msg = (rest[:i], tail[:16], tail[16:] or None)

    def _subneg(self):
        sb = bytes(self.sb)
        if (self.auth and len(sb) >= 3 and sb[0] == AUTH and sb[1] == AUTH_IS and sb[2] == AUTH_TYPE
                and self.him.get(AUTH) == YES):
            self._auth_is(sb)
        elif len(sb) >= 5 and sb[0] == NAWS:
            cols, rows = struct.unpack(">HH", sb[1:5])
            if cols and rows:
                self.size = (cols, rows)
                self.size_changed = True
        elif len(sb) >= 2 and sb[0] == TTYPE and sb[1] == TTYPE_IS:
            name = sb[2:].decode("ascii", "replace").strip()
            if name and all(c.isalnum() or c in "-_.+" for c in name) and len(name) <= 40:
                self.term = name.lower()

    def binary_in(self):
        return self.him.get(BINARY) == YES

    def binary_out(self):
        return self.us.get(BINARY) == YES

    def feed(self, data):
        o = bytearray()
        for c in data:
            st = self.st
            if st == 0:
                if c == IAC:
                    self.st = 1
                elif self.cr and c in (0, 10) and not self.binary_in():
                    self.cr = False   # NVT: CR NUL and CR LF are a CR (Enter)
                else:
                    self.cr = c == 13
                    o.append(c)
            elif st == 1:
                self.st = 0
                if c == IAC:
                    self.cr = False
                    o.append(IAC)
                elif WILL <= c <= DONT:
                    self.verb = c
                    self.st = 2
                elif c == SB:
                    self.sb = bytearray()
                    self.st = 3
            elif st == 2:
                self.st = 0
                self._got(self.verb, c)
            elif st == 3:
                if c == IAC:
                    self.st = 4
                elif len(self.sb) < SB_MAX:
                    self.sb.append(c)
            else:
                if c == SE:
                    self.st = 0
                    self._subneg()
                else:
                    if c == IAC and len(self.sb) < SB_MAX:
                        self.sb.append(c)
                    self.st = 3
        return bytes(o)

    def escape(self, data):
        """Bytes for the client: IAC doubled; in NVT a bare CR is CR NUL."""
        data = data.replace(b"\xff", b"\xff\xff")
        if not self.binary_out():
            data = data.replace(b"\r", b"\r\0").replace(b"\r\0\n", b"\r\n")
        return data


# ---- the session ----------------------------------------------------------------

HELPER = ("import fcntl, os, sys, termios\n"
          "fcntl.ioctl(0, termios.TIOCSCTTY, 0)\n"
          "os.execvp(sys.argv[1], sys.argv[1:])\n")
ENV_KEEP = ("HOME", "USER", "LOGNAME", "SHELL", "PATH", "LANG", "LC_ALL", "LC_CTYPE", "TMPDIR",
            "CLAUDE_CONFIG_DIR")


def session_env(base, term, shell):
    """The shell's environment: a short list from ours, so a password in our
    environment (or anything else of the server's) is never handed on."""
    env = {k: base[k] for k in ENV_KEEP if k in base}
    pw = pwd.getpwuid(os.getuid())
    env.setdefault("HOME", pw.pw_dir)
    env.setdefault("USER", pw.pw_name)
    env.setdefault("LOGNAME", pw.pw_name)
    env.setdefault("PATH", "/usr/bin:/bin:/usr/sbin:/sbin")
    env["SHELL"] = shell
    env["TERM"] = term
    if "LANG" not in env and "LC_ALL" not in env:
        env["LANG"] = "en_US.UTF-8"   # Claude Code draws in UTF-8
    env["UPTELNETD"] = "1"
    return env


def session_argv(shell, command):
    return [shell, "-l"] if not command else [shell, "-lc", command]


def log(msg):
    sys.stderr.write("%s uptelnetd: %s\n" % (time.strftime("%H:%M:%S"), msg))
    sys.stderr.flush()


class Server:
    def __init__(self, password, nets, shell, command, limiter=None, clients=None):
        self.password = password        # None: only keys log in
        self.clients = clients          # a ClientStore, or None: no key login
        self.nets = nets
        self.shell = shell
        self.command = command
        self.limiter = limiter or RateLimiter()
        self.pending = 0

    async def handle(self, reader, writer):
        peer = (writer.get_extra_info("peername") or ("?",))[0]
        if not allowed(peer, self.nets):
            log("%s: not in the allowlist, closed" % peer)
            writer.close()
            return
        if self.limiter.is_locked(peer):
            log("%s: locked out, closed" % peer)
            writer.close()
            return
        if self.pending >= MAX_PENDING:
            log("%s: too many logins at once, closed" % peer)
            writer.close()
            return
        self.pending += 1
        tn = Telnet(auth=self.clients is not None)
        try:
            try:
                ok = await asyncio.wait_for(self.login(reader, writer, tn, peer), LOGIN_TIMEOUT)
            except asyncio.TimeoutError:
                ok = False
                log("%s: login timed out" % peer)
        finally:
            self.pending -= 1
        if not ok:
            writer.close()
            return
        log("%s: logged in" % peer)
        await self.run(reader, writer, tn, peer)

    async def settle(self, reader, tn):
        """A client that agreed to TTYPE answers within a moment: wait for it."""
        for _ in range(10):
            if tn.term or tn.him.get(TTYPE) != YES:
                break
            try:
                chunk = await asyncio.wait_for(reader.read(256), 0.1)
            except asyncio.TimeoutError:
                continue
            if not chunk:
                return False
            tn.feed(chunk)
        return True

    async def key_phase(self, reader, writer, tn, peer, pending):
        """Steps 2-5 of the key login. "ok": the key was good and the client has the proof;
        "password": go on to Password: (tn.pair_wanted says the client asked to be paired);
        "closed": hang up."""
        deadline = asyncio.get_running_loop().time() + AUTH_WAIT
        while tn.auth_msg is None and tn.him.get(AUTH) != NO:
            left = deadline - asyncio.get_running_loop().time()
            if left <= 0:
                return "password"
            try:
                chunk = await asyncio.wait_for(reader.read(256), left)
            except asyncio.TimeoutError:
                return "password"
            if not chunk:
                return "closed"
            pending += tn.feed(chunk)
            if tn.out:
                writer.write(bytes(tn.out))
                tn.out.clear()
        if tn.auth_msg is None:
            return "password"                  # WONT: an Amiga without a key
        cid, cn, mac = tn.auth_msg
        if not cid:
            tn.pair_wanted = True              # empty ID: "pair me after the password"
            return "password"
        key = self.clients.keys().get(cid.decode("ascii", "replace"))
        want = hmac_sha256(key, b"upt1-client", tn.sn, cn, cid) if key else None
        if key and mac and hmac.compare_digest(mac, want):
            tn.sb_out(bytes([AUTH, AUTH_REPLY, AUTH_TYPE, AUTH_ACCEPT]), hmac_sha256(key, b"upt1-server", tn.sn, cn))
            writer.write(bytes(tn.out))
            tn.out.clear()
            await writer.drain()
            self.limiter.success(peer)
            log("%s: key login %s" % (peer, cid.decode("ascii", "replace")))
            return "ok"
        tn.sb_out(bytes([AUTH, AUTH_REPLY, AUTH_TYPE, AUTH_REJECT]))
        writer.write(bytes(tn.out))
        tn.out.clear()
        locked = self.limiter.failure(peer)
        log("%s: key rejected (%s)%s" % (peer, "unknown or revoked ID" if not key else "wrong mac",
                                         ", locked out" if locked else ""))
        if locked:
            await writer.drain()
            return "closed"
        return "password"

    def pair(self, writer, tn, peer):
        """Step 6: a key for the client that asked, after its good password."""
        try:
            cid, key = self.clients.add(peer)
        except (ConfigError, OSError) as e:
            log("%s: pairing failed: %s" % (peer, e))
            return
        tn.sb_out(bytes([AUTH, AUTH_REPLY, AUTH_TYPE, AUTH_PAIR]) + cid.encode("ascii") + b"\0" + key)
        writer.write(bytes(tn.out))
        tn.out.clear()
        log("%s: paired as %s" % (peer, cid))

    async def login(self, reader, writer, tn, peer):
        tn.start()
        tn.pair_wanted = False
        writer.write(bytes(tn.out) + b"uptelnetd (unencrypted telnet, LAN only)\r\n")
        tn.out.clear()
        pending = bytearray()
        if tn.auth:
            verdict = await self.key_phase(reader, writer, tn, peer, pending)
            if verdict == "ok":
                return await self.settle(reader, tn)
            if verdict == "closed":
                await writer.drain()
                return False
        if self.password is None:
            writer.write(b"This server logs in with a key only; this Amiga has none it knows. Ask for a pairing.\r\n")
            await writer.drain()
            return False
        for _ in range(TRIES_PER_CONNECTION):
            writer.write(b"Password: ")
            await writer.drain()
            line = None
            while line is None:
                chunk = await reader.read(256)
                if not chunk:
                    return False
                pending += tn.feed(chunk)
                if len(pending) > 1024:
                    return False  # nobody types a password that long
                if tn.out:
                    writer.write(bytes(tn.out))
                    tn.out.clear()
                for i, c in enumerate(pending):
                    if c in (10, 13):
                        line = bytes(pending[:i])
                        end = i + 2 if pending[i:i + 2] == b"\r\n" else i + 1
                        del pending[:end]
                        break
            writer.write(b"\r\n")
            if password_ok(line.decode("utf-8", "replace"), self.password):
                self.limiter.success(peer)
                if tn.pair_wanted:
                    self.pair(writer, tn, peer)
                return await self.settle(reader, tn)
            locked = self.limiter.failure(peer)
            log("%s: wrong password%s" % (peer, ", locked out" if locked else ""))
            await asyncio.sleep(FAIL_DELAY)
            writer.write(b"Login incorrect\r\n")
            if locked:
                break
        await writer.drain()
        return False

    async def run(self, reader, writer, tn, peer):
        loop = asyncio.get_running_loop()
        master, slave = os.openpty()
        cols, rows = tn.size or (80, 24)
        fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        tn.size_changed = False
        term = tn.term or "xterm-256color"
        proc = subprocess.Popen([sys.executable, "-c", HELPER] + session_argv(self.shell, self.command),
                                stdin=slave, stdout=slave, stderr=slave, start_new_session=True,
                                env=session_env(os.environ, term, self.shell),
                                cwd=os.path.expanduser("~"), close_fds=True)
        os.close(slave)
        os.set_blocking(master, False)
        done = loop.create_future()

        def pty_readable():
            try:
                data = os.read(master, 65536)
            except BlockingIOError:
                return
            except OSError:
                data = b""
            if not data:
                loop.remove_reader(master)
                if not done.done():
                    done.set_result(None)
                return
            writer.write(tn.escape(data))
            if writer.transport.get_write_buffer_size() > WRITE_HIGH:
                loop.remove_reader(master)
                loop.create_task(resume())

        async def resume():
            try:
                await writer.drain()
            except ConnectionError:
                if not done.done():
                    done.set_result(None)
                return
            if not done.done():
                loop.add_reader(master, pty_readable)

        async def from_client():
            while True:
                chunk = await reader.read(4096)
                if not chunk:
                    break
                data = tn.feed(chunk)
                if tn.out:
                    writer.write(bytes(tn.out))
                    tn.out.clear()
                if tn.size_changed:
                    tn.size_changed = False
                    c, r = tn.size
                    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", r, c, 0, 0))
                while data:
                    try:
                        n = os.write(master, data)
                    except BlockingIOError:
                        await asyncio.sleep(0.01)
                        continue
                    data = data[n:]
            if not done.done():
                done.set_result(None)

        loop.add_reader(master, pty_readable)
        client = loop.create_task(from_client())
        try:
            await done
        finally:
            loop.remove_reader(master)
            client.cancel()
            try:
                await writer.drain()
            except ConnectionError:
                pass
            writer.close()
            if proc.poll() is None:
                try:
                    os.killpg(proc.pid, 1)   # SIGHUP: the line dropped
                except ProcessLookupError:
                    pass
            os.close(master)
            await loop.run_in_executor(None, proc.wait)
            log("%s: session ended" % peer)


def parse_args(argv):
    ap = argparse.ArgumentParser(description="LAN-only telnet server for UP-Term's uptelnet (UNENCRYPTED).")
    ap.add_argument("--bind", action="append", default=[],
                    help="a private address to listen on (repeatable; default: the Mac's LAN address, en0/en1)")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT, help="TCP port (default %d)" % DEFAULT_PORT)
    ap.add_argument("--allow", action="append", default=[],
                    help="a private network allowed to connect (repeatable; default: the bind address's subnet)")
    ap.add_argument("--password-file", default=None,
                    help="0600 file holding the password (default %s; %s wins)" % (DEFAULT_PASSWORD_FILE, PASSWORD_ENV))
    ap.add_argument("--clients-file", default=None,
                    help="0600 file of paired Amigas (key login; default %s; tools/nas/pairs edits it)" % DEFAULT_CLIENTS_FILE)
    ap.add_argument("--command", default=None,
                    help="run this instead of a login shell, e.g. 'tmux new -A -s claude claude'")
    ap.add_argument("--shell", default=None, help="the shell (default $SHELL)")
    return ap.parse_args(argv)


def configure(a, env=None):
    """(binds, nets, password, shell) from the arguments, or ConfigError. The password is None
    when only a clients file stands in for it."""
    env = os.environ if env is None else env
    mask = None
    if not a.bind or a.bind == ["auto"]:
        addr, ifname = lan_address()
        if not addr:
            raise ConfigError("no LAN address on en0 or en1: pass --bind <the Mac's LAN address>")
        mask = _run(["ipconfig", "getoption", ifname, "subnet_mask"]) or None
        binds = [check_bind(addr)]
    else:
        binds = []
        for b in a.bind:
            b = check_bind(b)
            if b not in binds:
                binds.append(b)
    if a.allow:
        nets = [check_allow(c) for c in a.allow]
    else:
        nets = []
        for b in binds:
            for n in default_allow(b, mask if b == binds[0] else None):
                if n not in nets:
                    nets.append(n)
    try:
        password = load_password(env, a.password_file)
    except NoPassword:
        # keys alone can log in: a clients file (made by `pairs add`) stands in for the password
        clients = ClientStore(getattr(a, "clients_file", None))
        why = clients.check()
        if why:
            raise ConfigError(why)
        if not clients.exists():
            raise
        password = None
    why = ClientStore(getattr(a, "clients_file", None)).check()
    if why:
        raise ConfigError(why)       # a clients file others can read is refused, as the password file is
    shell = a.shell or env.get("SHELL") or pwd.getpwuid(os.getuid()).pw_shell or "/bin/sh"
    return binds, nets, password, shell


async def serve(a, binds, nets, password, shell):
    srv = Server(password, nets, shell, a.command, clients=ClientStore(a.clients_file))
    server = await asyncio.start_server(srv.handle, host=binds, port=a.port, reuse_address=True)
    port = server.sockets[0].getsockname()[1]
    print("uptelnetd: listening on %s port %d (UNENCRYPTED telnet), allowing %s" %
          (", ".join(binds), port, ", ".join(str(n) for n in nets)), flush=True)
    print("uptelnetd: on the Amiga, in an UP-Term window: uptelnet %s %d" % (binds[0], port), flush=True)
    async with server:
        await server.serve_forever()


def main(argv=None):
    a = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        binds, nets, password, shell = configure(a)
    except ConfigError as e:
        sys.stderr.write("uptelnetd: %s\n" % e)
        return 2
    os.environ.pop(PASSWORD_ENV, None)
    # select(), not kqueue: macOS's kqueue does not report a pty reliably
    loop = asyncio.SelectorEventLoop(selectors.SelectSelector())
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(serve(a, binds, nets, password, shell))
    except KeyboardInterrupt:
        pass
    except OSError as e:
        sys.stderr.write("uptelnetd: cannot listen on %s port %d: %s\n" % (", ".join(binds), a.port, e.strerror or e))
        return 1
    finally:
        loop.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
