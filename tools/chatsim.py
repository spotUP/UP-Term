#!/usr/bin/env python3
"""An irssi-shaped client for capture: a topic bar, a scrolling log window, a
status bar and an input line, drawn by ncurses (so the bytes are what curses
programs really send: scroll regions, IL/DL, attribute changes, partial line
updates). Deterministic: the same run every time.

  chatsim.py [lines]   -- print that many channel lines, then type a message
"""
import curses, sys, time

NICKS = ["spot", "blueberry", "flubba", "lars", "mr_x", "a_very_long_nickname"]
TEXT = [
    "hello there",
    "anyone tried the new console on 3.2?",
    "it scrolls with a region above the input line, like irssi",
    "åäö and é should survive",
    "a long line that has to wrap because it is much wider than the log window "
    "and keeps going past the right edge of the terminal",
    "ok",
    "/me waves",
]

def main(scr, lines):
    curses.start_color()
    curses.use_default_colors()
    for i, c in enumerate([curses.COLOR_CYAN, curses.COLOR_YELLOW, curses.COLOR_GREEN,
                           curses.COLOR_MAGENTA, curses.COLOR_RED, curses.COLOR_BLUE]):
        curses.init_pair(i + 1, c, -1)
    curses.init_pair(10, curses.COLOR_WHITE, curses.COLOR_BLUE)
    h, w = scr.getmaxyx()
    topic = curses.newwin(1, w, 0, 0)
    log = curses.newwin(h - 3, w, 1, 0)
    status = curses.newwin(1, w, h - 2, 0)
    inp = curses.newwin(1, w, h - 1, 0)
    log.scrollok(True)
    log.idlok(True)  # let curses scroll with the terminal's scroll region
    topic.bkgd(" ", curses.color_pair(10))
    status.bkgd(" ", curses.color_pair(10))
    topic.addstr(0, 0, " #amiga: vtcon test channel "[: w - 1])
    status.addstr(0, 0, " [12:00] [spot(+i)] [2:#amiga] [Act: 3,4] "[: w - 1])
    for win in (topic, status):
        win.noutrefresh()
    inp.addstr(0, 0, "[#amiga] ")
    inp.noutrefresh()
    curses.doupdate()
    for i in range(lines):
        nick = NICKS[i % len(NICKS)]
        text = TEXT[i % len(TEXT)]
        log.addstr("\n" if i else "")
        log.addstr("12:%02d " % (i % 60), curses.A_BOLD)
        log.addstr("<", curses.A_DIM)
        log.addstr(nick, curses.color_pair(1 + i % 6))
        log.addstr("> ", curses.A_DIM)
        try:
            log.addstr(text)
        except curses.error:
            pass
        log.noutrefresh()
        if i % 5 == 4:  # the activity counter changes, as irssi's does
            status.addstr(0, 40, "[Act: %d]" % i)
            status.noutrefresh()
        curses.doupdate()
    for ch in "typing a reply":
        inp.addstr(ch)
        inp.refresh()
    inp.move(0, 9)
    inp.clrtoeol()
    inp.refresh()
    time.sleep(30)  # stay on screen; the capture kills us

if __name__ == "__main__":
    curses.wrapper(main, int(sys.argv[1]) if len(sys.argv) > 1 else 40)
