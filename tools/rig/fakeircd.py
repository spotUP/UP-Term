#!/usr/bin/env python3
"""A scripted IRC server for testing terminal IRC clients in the rig.

  fakeircd.py [port] [lines]

Accepts one client, welcomes it, joins it to #amiga with a topic and a
names list, then plays channel traffic: normal lines, long lines that
wrap, colours (mIRC codes), actions, joins/parts, Latin-1 and UTF-8 text,
one line every 0.3 s. Deterministic, so screenshots are comparable.
"""
import socket, sys, time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 6667
LINES = int(sys.argv[2]) if len(sys.argv) > 2 else 60
NICKS = ["blueberry", "flubba", "lars", "mr_x", "a_very_long_nickname_indeed"]
TEXT = [
    "hello there",
    "anyone tried the new console on 3.2?",
    "it scrolls with a region above the input line",
    "\x02bold\x02 and \x0304red\x03 and \x0312blue\x03 and \x1funderlined\x1f",
    "a long line that has to wrap because it is much wider than the window and keeps "
    "going past the right edge of the terminal to see how the client breaks it",
    "\x01ACTION waves\x01",
    "latin-1 letters: \xe5\xe4\xf6",
]

def send(c, line):
    c.sendall((line + "\r\n").encode("latin-1"))

def main():
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", PORT))
    srv.listen(1)
    print("listening on", PORT, flush=True)
    c, _ = srv.accept()
    c.settimeout(0.2)
    nick = "spot"
    buf = b""
    t0 = time.time()
    while time.time() - t0 < 3:
        try:
            buf += c.recv(4096)
        except socket.timeout:
            pass
        for line in buf.split(b"\r\n"):
            if line.startswith(b"NICK "):
                nick = line[5:].decode("latin-1").strip()
        if b"USER" in buf:
            break
    s = ":fake.irc"
    send(c, "%s 001 %s :Welcome to the vtcon test network" % (s, nick))
    send(c, "%s 002 %s :Your host is fake.irc" % (s, nick))
    send(c, "%s 375 %s :- fake.irc message of the day -" % (s, nick))
    send(c, "%s 372 %s :- testing terminal IRC clients" % (s, nick))
    send(c, "%s 376 %s :End of MOTD" % (s, nick))
    send(c, ":%s!u@h JOIN :#amiga" % nick)
    send(c, "%s 332 %s #amiga :vtcon test channel: scroll regions, colours, wrapping" % (s, nick))
    send(c, "%s 353 %s = #amiga :@%s %s" % (s, nick, nick, " ".join(NICKS)))
    send(c, "%s 366 %s #amiga :End of NAMES" % (s, nick))
    for i in range(LINES):
        who = NICKS[i % len(NICKS)]
        if i % 17 == 16:
            send(c, ":newbie%d!u@h JOIN :#amiga" % i)
        elif i % 19 == 18:
            send(c, ":%s!u@h PART #amiga :bye" % who)
        else:
            send(c, ":%s!u@h PRIVMSG #amiga :%s" % (who, TEXT[i % len(TEXT)]))
        time.sleep(0.3)
        try:
            data = c.recv(4096)
            for line in data.split(b"\r\n"):
                if line.startswith(b"PING"):
                    send(c, "PONG" + line[4:].decode("latin-1"))
        except socket.timeout:
            pass
    time.sleep(600)

if __name__ == "__main__":
    main()
