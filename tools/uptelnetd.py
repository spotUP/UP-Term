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
  * It listens on ONE address of a private network (the Mac's own LAN
    address by default, found with `ipconfig getifaddr en0`), never on
    0.0.0.0 or ::, and refuses a public address outright.
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

Python 3.9+ standard library only (macOS's /usr/bin/python3 will do).
"""
import argparse
import asyncio
import fcntl
import hmac
import ipaddress
import os
import pwd
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
WRITE_HIGH = 256 * 1024   # stop reading the pty while this much waits for the Amiga

# The networks a LAN address can be on. Loopback is allowed too (it is
# safer still: nothing off the Mac reaches it).
PRIVATE_NETS = [ipaddress.ip_network(n) for n in (
    "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "169.254.0.0/16", "127.0.0.0/8",
    "fc00::/7", "fe80::/10", "::1/128")]


class ConfigError(Exception):
    pass


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
            raise ConfigError("no password: set %s, or write one to %s (mode 0600)" % (PASSWORD_ENV, p))
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


# ---- telnet (server side) -----------------------------------------------------

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240
BINARY, ECHO, SGA, TTYPE, NAWS = 0, 1, 3, 24, 31
TTYPE_IS, TTYPE_SEND = 0, 1
NO, YES, WANTNO, WANTYES = 0, 1, 2, 3
US_OK = {ECHO, SGA, BINARY}           # we will do these
HIM_OK = {BINARY, SGA, NAWS, TTYPE}   # we want the client to do these


class Telnet:
    """The server's half of RFC 854/1143: feed() takes the client's bytes
    and returns the data in them; answers go to self.out (bytes for the
    client). The window size and terminal type arrive as attributes."""

    def __init__(self):
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
        for o in (BINARY, NAWS, TTYPE):
            self.him[o] = WANTYES
            self.out += bytes([IAC, DO, o])

    def _cmd(self, verb, opt):
        self.out += bytes([IAC, verb, opt])

    def _him_on(self, opt):
        if opt == TTYPE:
            self.out += bytes([IAC, SB, TTYPE, TTYPE_SEND, IAC, SE])

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

    def _subneg(self):
        sb = bytes(self.sb)
        if len(sb) >= 5 and sb[0] == NAWS:
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
                elif len(self.sb) < 64:
                    self.sb.append(c)
            else:
                if c == SE:
                    self.st = 0
                    self._subneg()
                else:
                    if c == IAC and len(self.sb) < 64:
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
    def __init__(self, password, nets, shell, command, limiter=None):
        self.password = password
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
        tn = Telnet()
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

    async def login(self, reader, writer, tn, peer):
        tn.start()
        writer.write(bytes(tn.out) + b"uptelnetd (unencrypted telnet, LAN only)\r\n")
        tn.out.clear()
        pending = bytearray()
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
                # a client that agreed to TTYPE answers within a moment
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
    ap.add_argument("--bind", default="auto",
                    help="the private address to listen on (default: the Mac's LAN address, en0/en1)")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT, help="TCP port (default %d)" % DEFAULT_PORT)
    ap.add_argument("--allow", action="append", default=[],
                    help="a private network allowed to connect (repeatable; default: the bind address's subnet)")
    ap.add_argument("--password-file", default=None,
                    help="0600 file holding the password (default %s; %s wins)" % (DEFAULT_PASSWORD_FILE, PASSWORD_ENV))
    ap.add_argument("--command", default=None,
                    help="run this instead of a login shell, e.g. 'tmux new -A -s claude claude'")
    ap.add_argument("--shell", default=None, help="the shell (default $SHELL)")
    return ap.parse_args(argv)


def configure(a, env=None):
    """(bind, nets, password, shell) from the arguments, or ConfigError."""
    env = os.environ if env is None else env
    mask = None
    if a.bind == "auto":
        addr, ifname = lan_address()
        if not addr:
            raise ConfigError("no LAN address on en0 or en1: pass --bind <the Mac's LAN address>")
        mask = _run(["ipconfig", "getoption", ifname, "subnet_mask"]) or None
        bind = check_bind(addr)
    else:
        bind = check_bind(a.bind)
    nets = [check_allow(c) for c in a.allow] if a.allow else default_allow(bind, mask)
    password = load_password(env, a.password_file)
    shell = a.shell or env.get("SHELL") or pwd.getpwuid(os.getuid()).pw_shell or "/bin/sh"
    return bind, nets, password, shell


async def serve(a, bind, nets, password, shell):
    srv = Server(password, nets, shell, a.command)
    server = await asyncio.start_server(srv.handle, host=bind, port=a.port, reuse_address=True)
    port = server.sockets[0].getsockname()[1]
    print("uptelnetd: listening on %s port %d (UNENCRYPTED telnet), allowing %s" %
          (bind, port, ", ".join(str(n) for n in nets)), flush=True)
    print("uptelnetd: on the Amiga, in an UP-Term window: uptelnet %s %d" % (bind, port), flush=True)
    async with server:
        await server.serve_forever()


def main(argv=None):
    a = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        bind, nets, password, shell = configure(a)
    except ConfigError as e:
        sys.stderr.write("uptelnetd: %s\n" % e)
        return 2
    os.environ.pop(PASSWORD_ENV, None)
    # select(), not kqueue: macOS's kqueue does not report a pty reliably
    loop = asyncio.SelectorEventLoop(selectors.SelectSelector())
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(serve(a, bind, nets, password, shell))
    except KeyboardInterrupt:
        pass
    except OSError as e:
        sys.stderr.write("uptelnetd: cannot listen on %s port %d: %s\n" % (bind, a.port, e.strerror or e))
        return 1
    finally:
        loop.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
