"""wbtext.py -- error lines in the text of a window a Workbench icon opened (the UP-Term icon, wbrun).

The A1200's SYS:System/UP-Term icon opened its window with "wrong number of arguments" (AmigaDOS 118) in it
and install_rig.py passed: it only asked the window list for a window titled UP-Term. install_rig.py now reads
the window's text (typewin.screen_text) and fails on any line this finds."""
import re

ERROR_RE = re.compile(
    r"wrong number of arguments|required argument missing|unknown command|object not found|"
    r"can't open|cannot open|\bnot found\b|\bfailed\b|\b(?:error|rc|return code) ?[:=]? ?\d{2,3}\b",
    re.I)


def error_lines(text):
    """The lines of `text` that read as an error, stripped."""
    return [l.strip() for l in text.splitlines() if ERROR_RE.search(l)]
