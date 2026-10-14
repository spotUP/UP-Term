#!/usr/bin/env python3
"""standalone_rig.py -- K2-1: what each add-on does on a stock Workbench with NO UP-Term kit
(thoughts/shared/plans/2026-10-13-kit-split.md, phase K2-1; the answer is
thoughts/shared/research/2026-10-13_standalone-addons.md).

The programs are copied to VTC:sa/ (nothing goes into C:, L:, LIBS: except the KingCON handler and
its mountlist, which the cleanup takes out again) and run in a ROM CON: window and in a KingCON
(KCON:) window of a clean installed AmigaOS 3.2 (rig.py ... --os 32). The owner's definition: an
add-on is stand-alone when, with no base, it starts, finishes a conversation with a tool call, and
its keys work in both consoles (line mode counts, a garbled screen does not).

  claude   C:Claude   against tools/claude_fixture.py (plain http, no key)
  pi       amiga-pi   against the same fixture (openai-completions, models.json in ENVARC:amiga-pi)
  nvim     Neovim     -u NONE RAM:t.txt, typed "ihello<Esc>:wq", RAM:t.txt must say hello
  python   Python 3   -c print(6*7), then the REPL: a typed line runs, Ctrl-C at the prompt and a typed
                      line after it runs (ixemul without the Y1 fix put the SIGINT frame below address 0
                      and the machine reset), Up recalls it
  each of nvim and python under three LIBS: states (none: no ixemul; stock: GG-Lite's ixemul 48.x;
  kit: the kit's patched 80.1 + ixnet) and TERM unset / TERM=vtcon with TERMINFO at a copy of the kit's terminfo.

Evidence: the fixture's request bodies (--dump), files on the Amiga, and the window's text. The text is
read back from the screen's pixels, not by OCR: the glyphs of the console font are learned from a
calibration window (all printable ASCII, written by Type) and every 8x8 cell of the test window is
looked up. A cell it does not know prints as "?". Escape sequences a console did not take show as
text (like "[2J") and count as garbled.

Verdicts: build/<rig>/standalone/<program>-<console>-<check>.json (verdict pass/fail/observed, detail,
fingerprint of the binaries it copied); the window's text in <case>.txt next to it, screenshots in
shots/. Resumable: a recorded verdict with the same fingerprint is skipped.

  python3 -I tools/rig/standalone_rig.py                 the sessions not yet recorded
  python3 -I tools/rig/standalone_rig.py --only nvim-rom the sessions whose name starts so (claude-rom, claude-king,
                                         pi-rom, pi-king, nvim-rom-none-unset, python-king-kit-vtcon, ...)
  python3 -I tools/rig/standalone_rig.py --failed        only the sessions with a failed verdict
  python3 -I tools/rig/standalone_rig.py --force         all, from zero
  python3 -I tools/rig/standalone_rig.py --list          the sessions and their recorded verdicts
  python3 -I tools/rig/standalone_rig.py --cleanup       take KingCON and the LIBS: states out again (also at the end of a run)

Rig: UPTERM_RIG=<n> python3 -I tools/rig/rig.py setup --os 32 --max, then start --os 32 --max (a clean
installed 3.2: no UP-Term:, no ixemul in SYS:Libs). Build first: `make build/amiga/Claude` here,
`make -C $UPTERM_ROOT/amiga-pi amiga`; nvim, python3 and its ixemul come from the staged archives
(build/dist/UP-Term-nvim, Python3, terminfo from UP-Term; STANDALONE_DIST= one Files drawer with all). Needs xdftool only once, to read the owner's KingCON handler out of
UPTERM_SYSTEM_HDF (read only); stock ixemul from UPTERM_IXSTOCK (default GG-Lite's Libs drawer).
Not run by `make test`: it needs the emulator."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import time
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402
import ami  # noqa: E402
import condev_rig as c  # noqa: E402
import typewin as tw  # noqa: E402

ROOT = paths.VTCON
RIG = paths.RIG
VTC = RIG / 'vtc'
SA = VTC / 'sa'
OUT = RIG / 'standalone'
SHOTS = OUT / 'shots'
RET, ESC, UP, BS = tw.RET, tw.ESC, 0x4C, tw.BS
CTRL = tw.CTRL
KEY_C = 0x33
PORT = 8110 + paths.RIG_N

CLAUDE_BIN = pathlib.Path(os.environ.get('CLAUDE_BIN') or ROOT / 'build/amiga/Claude')
PI_BIN = pathlib.Path(os.environ.get('AMIGA_PI_BIN') or paths.repo('amiga-pi') / 'build/amiga/amiga-pi')
DIST_ARCHIVE = {'nvim': 'UP-Term-nvim', 'python3': 'Python3', 'libs': 'Python3'}   # since the K2 split


def dist(rel):
    """A file of the staged kit: under STANDALONE_DIST (one Files drawer holding nvim, python3, libs and
    terminfo), else in the archive that carries it since the K2 split (build/dist/<archive>/Files; libs:
    Python3's own ixemul, terminfo: the base's), else in the single kit of before the split."""
    if os.environ.get('STANDALONE_DIST'):
        return pathlib.Path(os.environ['STANDALONE_DIST']) / rel
    archive = DIST_ARCHIVE.get(rel.split('/')[0], 'UP-Term')
    for top in (ROOT / 'build/dist', paths.RIG_HOME / 'dist'):
        for a in (archive, 'UP-Term'):
            if (top / a / 'Files' / rel).exists():
                return top / a / 'Files' / rel
    return paths.RIG_HOME / 'dist' / archive / 'Files' / rel


IXSTOCK = pathlib.Path(os.environ.get('UPTERM_IXSTOCK') or
                       pathlib.Path.home() / 'Downloads/gg-lite/x/GG-Lite-1.0-bin-m68k/Libs')
KING_HANDLER = SA / 'kc/KingCON-handler'
KING_MOUNTLIST = SA / 'kc/KingCON-mountlist'
CONSOLES = {'rom': ('CON', 'ROM CON:'), 'king': ('KCON', 'KingCON (KCON:)')}
LIBS = ('none', 'stock', 'kit')
TERMS = ('unset', 'vtcon')
GO_LIBS = ['Assign >NIL: LIBS: VTC:ixp6', 'Assign >NIL: LIBS: SYS:Libs ADD', 'Assign >NIL: LIBS: SYS:Classes ADD',
           'Assign >NIL: LIBS: VTC:pkgs/ncurses-5.5-1-p-bin-m68k/ixlibrary/sys/libs ADD']   # BOOTX:go's LIBS:
GO_ENV = ['Delete >NIL: ENV:TERM QUIET', 'SetEnv TERMINFO /VTC/terminfo', 'SetEnv TERMCAP /etc/termcap']

JUNK = re.compile(r'\[(?:\?\d+|\d+(?:;\d+)*)[A-Za-z]')   # an escape sequence a console printed as text
TITLE = 'sa-'

SESSIONS = ['claude-rom', 'claude-king', 'pi-rom', 'pi-king'] + \
    ['%s-%s-%s-%s' % (p, k, l, t) for p in ('nvim', 'python') for k in CONSOLES for l in LIBS for t in TERMS]
CHECKS = {'claude': ['start', 'conversation', 'keys', 'history', 'bash', 'screen', 'data'],
          'pi': ['start', 'conversation', 'keys', 'history', 'bash', 'screen', 'data'],
          'nvim': ['start', 'edit', 'screen', 'terminal'],
          'python': ['start', 'repl', 'ctrlc', 'history', 'afterup', 'screen']}


def say(s):
    print(s, flush=True)


LAST_STEP = ['start']


def step(s):
    LAST_STEP[0] = s
    print('[%s] %s' % (time.strftime('%H:%M:%S'), s), flush=True)


# ---------------------------------------------------------------- verdicts

def sha(*paths_):
    h = hashlib.sha256()
    for p in paths_:
        p = pathlib.Path(p)
        if p.is_dir():
            for f in sorted(p.rglob('*')):
                if f.is_file():
                    h.update(str(f.relative_to(p)).encode() + f.stat().st_size.to_bytes(8, 'big'))
        elif p.exists():
            h.update(p.read_bytes())
        else:
            h.update(b'missing ' + str(p).encode())
    return h.hexdigest()[:12]


def fingerprint(session):
    prog, parts = session.split('-')[0], session.split('-')
    base = [KING_HANDLER] if parts[1] == 'king' else []
    if prog == 'claude':
        return sha(CLAUDE_BIN, *base)
    if prog == 'pi':
        return sha(PI_BIN, *base)
    exe = dist('nvim/bin/nvim' if prog == 'nvim' else 'python3/bin/python3')
    libs = {'none': [], 'stock': [IXSTOCK / 'ixemul.library', IXSTOCK / 'ixnet.library'],
            'kit': [dist('libs/ixemul.library'), dist('libs/ixnet.library')]}[parts[2]]
    return sha(exe, *libs, *base)


def vpath(session, check):
    prog, kind, rest = (session.split('-', 2) + [''])[:3]
    name = '%s-%s-%s' % (prog, kind, check) + ('-' + rest if rest else '')
    return OUT / (name + '.json')


def record(session, check, verdict, detail):
    """verdict: pass | fail | observed (a measurement that is not a gate)."""
    OUT.mkdir(parents=True, exist_ok=True)
    detail = str(detail)[:900]
    vpath(session, check).write_text(json.dumps(
        {'session': session, 'check': check, 'verdict': verdict, 'detail': detail,
         'fingerprint': fingerprint(session), 'time': time.strftime('%Y-%m-%d %H:%M:%S')}, indent=1))
    say('[%s] %s %s%s' % (verdict.upper(), session, check, '' if verdict == 'pass' else ': ' + detail[:300]))


def load(session, check):
    p = vpath(session, check)
    try:
        return json.loads(p.read_text())
    except (OSError, ValueError):
        return None


def session_state(session):
    """(recorded, failed): every check has a verdict with this fingerprint; any failed."""
    fp = fingerprint(session)
    got = [load(session, ck) for ck in CHECKS[session.split('-')[0]]]
    ok = all(g and g['fingerprint'] == fp for g in got)
    return ok, any(g and g['verdict'] == 'fail' for g in got)


# ---------------------------------------------------------------- the Amiga

def run(cmd, timeout=60):
    return c.run(cmd, timeout)


def amiga_text(path):
    rc, out = run('Type "%s"' % path)
    return out.strip() if rc == 0 else None


def put(path, text):
    tmp = OUT / '_put.tmp'
    tmp.write_text(text)
    ami.put(str(tmp), path)


def tree_windows():
    """[(title, (x, y, w, h), active, [(label, kind, (x, y, w, h))])] of the front screen."""
    out = []
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        try:
            f = shlex.split(line)
        except ValueError:
            continue
        if line.startswith('W ') and len(f) >= 6:
            w, h = map(int, f[4].split('x'))
            out.append([f[-1], (int(f[2]), int(f[3]), w, h), f[5] == 'active', []])
        elif line.startswith('G ') and out and len(f) >= 6:
            w, h = map(int, f[4].split('x'))
            out[-1][3].append((f[-1], f[5], (int(f[2]), int(f[3]), w, h)))
    return out


def foreign(own):
    """The windows that are neither the Workbench nor the test window (requesters)."""
    return [w for w in tree_windows() if w[0] not in ('Workbench', own) and not w[0].startswith(TITLE)]


def read_window_text(win):
    """Tesseract's reading of a requester (a few sentences; only for the log, not a verdict)."""
    try:
        wd, h, rows = ami.grab(win[1])
        k = 3
        px = [r[i:i + 3] for r in rows for i in range(0, len(r), 3)]
        bg = Counter(px).most_common(1)[0][0]
        ppm = OUT / '_req.ppm'
        with open(ppm, 'wb') as f:
            f.write(b'P6 %d %d 255\n' % (wd * k, h * k))
            for r in rows:
                big = b''.join((b'\xff\xff\xff' if r[i:i + 3] == bg else b'\0\0\0') * k for i in range(0, len(r), 3))
                for _ in range(k):
                    f.write(big)
        out = subprocess.run(['tesseract', str(ppm), '-', '--psm', '6'], capture_output=True, text=True).stdout
        return ' / '.join(l.strip() for l in out.splitlines() if l.strip())
    except (OSError, SystemExit):
        return '(unreadable)'


def dismiss(win):
    """Click the requester's last labelled gadget (Abort, Cancel, OK ...); the text it showed."""
    text = read_window_text(win)
    labelled = [g for g in win[3] if g[0] and not g[1].startswith('sys:')]
    if labelled:
        x, y, w, h = labelled[-1][2]
        ami.click_px(x + w // 2, y + h // 2)
    time.sleep(1.5)
    return '%s: %s [%s]' % (win[0], text, labelled[-1][0] if labelled else 'no gadget')


def stray_windows():
    return [w for w in tree_windows() if w[0] != 'Workbench']


def close_window(title):
    """End the Shell in the window (EndCLI), or click its close gadget; True when it is gone."""
    for attempt in range(3):
        w = ami.window(title)
        if not w:
            return True
        tw._last_title = title
        tw.click_in(title)
        if attempt == 0:
            tw.typ('EndCLI')
            tw.key(RET, wait=2)
        elif attempt == 1:
            x, y, ww, hh = w['sys:close']
            ami.click_px(x + ww // 2, y + hh // 2)
            time.sleep(2)
        else:
            tw.key(KEY_C, CTRL, wait=2)
    return ami.window(title) is None


def close_strays():
    for w in stray_windows():
        if w[0].startswith(TITLE) or w[0] in ('kc1', 'probe1'):
            close_window(w[0])
        else:
            dismiss(w)


def kill(name):
    """Break (Ctrl+C) every process whose command contains name."""
    for line in run('Status')[1].splitlines():
        m = re.match(r'\s*Process\s+(\d+):\s+Loaded as command: (.*)', line)
        if m and name in m.group(2):
            run('Break %s C' % m.group(1))


def alive(name):
    return name in run('Status')[1]


# ---------------------------------------------------------------- the emulator itself (a guru, a hung machine)

SWIFT_WINDOWS = (
    'import CoreGraphics\nimport Foundation\n'
    'let l = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]\n'
    'for w in l { let b = w["kCGWindowBounds"] as! [String: Any]\n'
    '  print(w["kCGWindowNumber"]!, w["kCGWindowOwnerPID"]!, b["Width"]!, b["Height"]!) }\n')


def emulator_text():
    """What the emulator's own window shows, read by Tesseract (macOS screencapture of this rig's FS-UAE
    window, found by the config path in its command line): an Exec alert ("Software Failure", "Guru
    Meditation #xxxxxxxx.xxxxxxxx") is drawn outside the Workbench screen, where amiagent cannot see it.
    Returns the non-empty lines of the top fifth of the window, or a note why it could not be read."""
    try:
        cfg = str(RIG / 'vtcon-rig.fs-uae')
        pid = subprocess.run(['pgrep', '-f', 'fs-uae.*' + re.escape(cfg)], capture_output=True, text=True).stdout.split()
        if not pid:
            return ['(no FS-UAE process for this rig)']
        swift = OUT / '_windows.swift'
        swift.write_text(SWIFT_WINDOWS)
        wins = [l.split() for l in subprocess.run(['swift', str(swift)], capture_output=True, text=True).stdout.splitlines()]
        big = [w for w in wins if w[1] == pid[0] and int(w[2]) > 600 and int(w[3]) > 400]
        if not big:
            return ['(no FS-UAE window found)']
        png = OUT / '_emulator.png'
        subprocess.run(['screencapture', '-x', '-o', '-l' + big[0][0], str(png)], check=True)
        from PIL import Image
        im = Image.open(png).convert('RGB')
        w, h = im.size
        shots = OUT / 'shots'
        shots.mkdir(parents=True, exist_ok=True)
        im.save(shots / ('emulator-%s.png' % time.strftime('%H%M%S')))
        top = OUT / '_emulator-top.png'
        im.crop((0, 0, w, h // 5)).resize((w * 2, h * 2 // 5)).save(top)
        out = subprocess.run(['tesseract', str(top), '-', '--psm', '6'], capture_output=True, text=True).stdout
        return [l.strip() for l in out.splitlines() if l.strip()]
    except (OSError, subprocess.SubprocessError, ImportError) as e:
        return ['(emulator window unreadable: %s)' % e]


def restart_rig():
    """FS-UAE itself can die (a host-side SIGSEGV, 2026-10-13, python3 on the stock ixemul): start the rig again
    with the flags it was started with (STANDALONE_RIG_FLAGS, default the clean 3.2 on the --max machine)."""
    cfg = str(RIG / 'vtcon-rig.fs-uae')
    if subprocess.run(['pgrep', '-f', 'fs-uae.*' + re.escape(cfg)], capture_output=True).returncode == 0:
        return
    flags = os.environ.get('STANDALONE_RIG_FLAGS', '--os 32 --max').split()
    say('[INFO] FS-UAE is gone: rig.py start %s' % ' '.join(flags))
    subprocess.run([sys.executable, '-I', str(pathlib.Path(__file__).with_name('rig.py')), 'start'] + flags, check=False)


def wait_rig(secs=480):
    """Wait for the rig to answer again after a crash (the guest resets itself a few minutes after one)."""
    restart_rig()
    end = time.time() + secs
    while time.time() < end:
        try:
            if ami.req(0x01):
                time.sleep(20)      # the boot's own scripts are still running
                return True
        except (SystemExit, OSError):
            pass
        time.sleep(10)
    return False


# ---------------------------------------------------------------- reading the screen

class Reader:
    """The text of a console window from its pixels (see the module docstring)."""
    X0, Y0 = 4, 11      # the window's inner left edge and the title bar's bottom (topaz 8 windows)
    CELL = 8

    def __init__(self):
        self.glyphs = {}
        self.org = {}

    def _ink(self, title):
        w = ami.window(title)
        wd, h, rows = ami.grab(w['box'])
        flat = [r[i:i + 3] for r in rows for i in range(0, len(r), 3)]
        bg = Counter(flat).most_common(1)[0][0]
        right = w['sys:size'][0] - w['box'][0] - 1      # the right border starts at the size gadget
        ink = [bytes(0 if r[i:i + 3] == bg else 1 for i in range(0, len(r), 3)) for r in rows]
        return ink, wd, h, right

    def _grid(self, ink, wd, h, right, ox, oy):
        cols = max((right - ox) // self.CELL, 0)
        nrows = max((h - 2 - oy) // self.CELL, 0)
        grid = []
        for r in range(nrows):
            y = oy + r * self.CELL
            grid.append([b''.join(ink[y + k][ox + c * self.CELL: ox + (c + 1) * self.CELL] for k in range(self.CELL))
                         for c in range(cols)])
        return grid

    def calibrate(self, console_dev):
        """Open a window of this console, Type all printable ASCII, learn the glyphs and the grid origin."""
        lines = []
        chars = [chr(i) for i in range(0x21, 0x7F)]
        for i in range(0, len(chars), 32):
            lines.append(''.join(chars[i:i + 32]))
        put('RAM:sa-cal.txt', '\n'.join(lines) + '\n')
        title = TITLE + 'cal-' + console_dev
        open_window(console_dev, title, [], settle=3)
        tw.typ('Type RAM:sa-cal.txt')
        tw.key(RET, wait=2)
        ink, wd, h, right = self._ink(title)
        best = None
        for ox in range(self.X0 - 3, self.X0 + 8):
            for oy in range(self.Y0 - 3, self.Y0 + 8):
                grid = self._grid(ink, wd, h, right, ox, oy)
                blank = bytes(self.CELL * self.CELL)
                pats = {p for row in grid for p in row if p != blank}
                if best is None or len(pats) < best[0]:
                    best = (len(pats), ox, oy, grid)
        _, ox, oy, grid = best
        blank = bytes(self.CELL * self.CELL)
        found = None
        for i in range(len(grid) - 2):
            if all(len(grid[i + k]) >= len(lines[k]) and all(p != blank for p in grid[i + k][:len(lines[k])])
                   for k in range(3)):
                found = grid[i:i + 3]
                break
        if not found:
            raise SystemExit('[ERROR] calibration of %s: the three rows of printable characters were not found' % console_dev)
        glyphs = {}
        for row, text in zip(found, lines):
            for p, ch in zip(row, text):
                glyphs[p] = ch
        # the styles a program may ask for: bold (the console smears the glyph one pixel right), underline
        # (the bottom row inked), both; italic is not drawn by the ROM console
        for pat, ch in list(glyphs.items()):
            rows8 = [pat[r * 8:(r + 1) * 8] for r in range(8)]
            bold = b''.join(bytes(a | b for a, b in zip(r_, b'\0' + r_[:-1])) for r_ in rows8)
            for var in (bold, pat[:56] + bytes([1]) * 8, bold[:56] + bytes([1]) * 8):
                glyphs.setdefault(var, ch)
        self.glyphs[console_dev] = glyphs
        self.org[console_dev] = (ox, oy)
        close_window(title)
        say('[INFO] %s glyphs: %d patterns for %d characters, grid origin %d,%d' % (console_dev, len(glyphs), len(chars), ox, oy))

    def text(self, title, console_dev):
        """(rows of text, unknown-cell count) of the window."""
        ink, wd, h, right = self._ink(title)
        ox, oy = self.org[console_dev]
        grid = self._grid(ink, wd, h, right, ox, oy)
        glyphs = self.glyphs[console_dev]
        blank = bytes(self.CELL * self.CELL)
        full = bytes([1]) * (self.CELL * self.CELL)
        unknown = 0
        rows = []
        for row in grid:
            s = []
            for p in row:
                if p == blank:
                    s.append(' ')
                elif p in glyphs:
                    s.append(glyphs[p])
                else:
                    inv = bytes(1 - b for b in p)
                    if inv in glyphs:
                        s.append(glyphs[inv])      # reverse video: the cursor, a selection
                    elif p == full:
                        s.append(' ')
                    else:
                        s.append('?')
                        unknown += 1
            rows.append(''.join(s).rstrip())
        while rows and not rows[-1]:
            rows.pop()
        return rows, unknown


READER = Reader()


def screen(title, dev, name=None):
    """The window's text as one string; saved next to the verdicts as <name>.txt, a screenshot as <name>.png."""
    rows, unknown = READER.text(title, dev)
    text = '\n'.join(rows)
    if name:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / (name + '.txt')).write_text(text + '\n')
        SHOTS.mkdir(parents=True, exist_ok=True)
        w = ami.window(title)
        if w:
            wd, h, px = ami.grab(w['box'])
            ami.png(str(SHOTS / (name + '.png')), wd, h, px)
    return text, unknown


def garbled(text, unknown):
    """Why the text looks garbled, or ''."""
    why = []
    junk = JUNK.findall(text)
    if junk:
        why.append('escape sequences shown as text: %s' % ', '.join(sorted(set(junk))[:5]))
    chars = sum(len(l.strip()) for l in text.splitlines())
    if chars and unknown * 10 > chars:
        why.append('%d of %d cells are unknown glyphs' % (unknown, chars))
    return '; '.join(why)


def wait_text(title, dev, pattern, secs, name=None):
    """Poll the window's text for the regex; (text, unknown, found)."""
    end = time.time() + secs
    while True:
        text, unknown = screen(title, dev)
        if re.search(pattern, text, re.M):
            return text, unknown, True
        if time.time() >= end:
            return text, unknown, False
        time.sleep(1.5)


# ---------------------------------------------------------------- windows

def open_window(dev, title, prelude, settle=3):
    """A titled window of CON: or KCON: whose Shell first runs the prelude lines, then waits at its prompt."""
    name = 'p%d' % int(time.time() * 1000)
    (SA / 'run').mkdir(parents=True, exist_ok=True)
    (SA / 'run' / (name + '.dos')).write_text('\n'.join(prelude + ['']))
    run('Run >NIL: NewShell "%s:0/12/640/230/%s/CLOSE" FROM VTC:sa/run/%s.dos' % (dev, title, name))
    for _ in range(40):
        if ami.window(title):
            break
        time.sleep(0.5)
    else:
        raise SystemExit('[ERROR] window %s never appeared' % title)
    time.sleep(settle)
    tw._last_title = title
    tw.click_in(title)
    time.sleep(1)
    tw._checked_at = 0.0


def prelude_for(libs, term):
    """The Shell script that makes a window's state: LIBS:, TERM, the current directory."""
    lines = ['Delete >NIL: ENV:TERM ENV:TERMINFO ENV:TERMCAP QUIET', 'Delete >NIL: ENVARC:TERM ENVARC:TERMINFO QUIET']
    if libs == 'none':
        first = 'SYS:Libs'
    else:
        first = 'VTC:sa/ix' + libs
    lines += ['Assign >NIL: LIBS: ' + first, 'Assign >NIL: LIBS: SYS:Libs ADD', 'Assign >NIL: LIBS: SYS:Classes ADD']
    if term == 'vtcon':
        lines += ['SetEnv TERM vtcon', 'SetEnv TERMINFO /VTC/sa/terminfo']
    lines += ['Avail >NIL: FLUSH', 'CD RAM:']
    return lines


def typeline(s, wait=1.0):
    tw.typ(s)
    tw.key(RET, wait=wait)


def type_slowly(s):
    tw.typ(s)


# ---------------------------------------------------------------- fixture

class Fixture:
    def __init__(self, delay=0.05):
        self.dump = RIG / 'standalone_dump'
        shutil.rmtree(self.dump, ignore_errors=True)
        self.dump.mkdir(parents=True)
        self.log = OUT / 'fixture.log'
        self.proc = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools/claude_fixture.py'), '--port', str(PORT),
                                      '--delay', str(delay), '--dump', str(self.dump)],
                                     stdout=open(self.log, 'w'), stderr=subprocess.STDOUT)
        time.sleep(1.5)

    def bodies(self):
        out = []
        for p in sorted(self.dump.glob('*.json')):
            try:
                out.append(p.read_text(errors='replace'))
            except OSError:
                pass
        return out

    def count(self):
        return len(self.bodies())

    def wait_count(self, n, secs):
        end = time.time() + secs
        while time.time() < end:
            if self.count() >= n:
                return True
            time.sleep(0.5)
        return self.count() >= n

    def stop(self):
        self.proc.terminate()
        self.proc.wait()


def tool_result_text(body):
    """The text of the last tool result in a request body (Claude's or an OpenAI chat's), '' when none."""
    try:
        j = json.loads(body)
    except ValueError:
        return ''
    for m in reversed(j.get('messages', [])):
        cont = m.get('content')
        if m.get('role') == 'tool':
            return cont if isinstance(cont, str) else json.dumps(cont)
        if isinstance(cont, list):
            for b in reversed(cont):
                if isinstance(b, dict) and b.get('type') == 'tool_result':
                    t = b.get('content')
                    return t if isinstance(t, str) else json.dumps(t)
    return ''


def system_text(body):
    try:
        j = json.loads(body)
    except ValueError:
        return body
    s = j.get('system') or ''
    if not s:
        s = ' '.join(m.get('content', '') for m in j.get('messages', []) if m.get('role') == 'system' and isinstance(m.get('content'), str))
    return s if isinstance(s, str) else json.dumps(s)


# ---------------------------------------------------------------- staging

def stage():
    """Copy the programs under test to VTC:sa/ (a host drawer) when they changed."""
    SA.mkdir(parents=True, exist_ok=True)
    for src, dst in ((CLAUDE_BIN, 'Claude'), (PI_BIN, 'amiga-pi')):
        if not src.exists():
            sys.exit('[ERROR] %s missing: make build/amiga/Claude / make -C $UPTERM_ROOT/amiga-pi amiga' % src)
        if not (SA / dst).exists() or (SA / dst).read_bytes() != src.read_bytes():
            shutil.copyfile(src, SA / dst)
    for sub in ('nvim', 'python3'):
        if not dist(sub).exists():
            sys.exit('[ERROR] %s missing: make dist (or STANDALONE_DIST=)' % dist(sub))
        if not (SA / sub).exists():
            shutil.copytree(dist(sub), SA / sub, symlinks=True)
    if not (SA / 'terminfo').exists():
        shutil.copytree(dist('terminfo'), SA / 'terminfo')
    for lib, src in (('ixkit', dist('libs')), ('ixstock', IXSTOCK)):
        (SA / lib).mkdir(exist_ok=True)
        for f in ('ixemul.library', 'ixnet.library'):
            if not src.joinpath(f).exists():
                sys.exit('[ERROR] %s missing (UPTERM_IXSTOCK= for the stock ixemul)' % (src / f))
            if not (SA / lib / f).exists() or (SA / lib / f).read_bytes() != (src / f).read_bytes():
                shutil.copyfile(src / f, SA / lib / f)


def kingcon_ready():
    """The owner's KingCON handler on the clean image and KCON: mounted; (True, note) or (False, why)."""
    if not KING_HANDLER.exists():
        (SA / 'kc').mkdir(parents=True, exist_ok=True)
        if not paths.SYSTEM_HDF.exists() or not shutil.which('xdftool'):
            return False, 'KingCON handler not available (no %s or no xdftool to read it)' % paths.SYSTEM_HDF
        for src, dst in (('L/KingCON-handler', KING_HANDLER), ('DEVS/KingCON-mountlist', KING_MOUNTLIST)):
            r = subprocess.run(['xdftool', '-r', str(paths.SYSTEM_HDF), 'open', 'part=DH0', '+', 'read', src, str(dst)],
                               capture_output=True, text=True)
            if not dst.exists():
                return False, 'KingCON handler not found in %s DH0:%s (%s)' % (paths.SYSTEM_HDF.name, src, r.stdout[-100:])
    if 'KCON' not in run('Assign LIST')[1].upper().split():
        run('Copy VTC:sa/kc/KingCON-handler L:')
        run('Copy VTC:sa/kc/KingCON-mountlist DEVS:')
        rc, out = run('Mount KCON: FROM DEVS:KingCON-mountlist')
        if rc or 'KCON' not in run('Assign LIST')[1].upper().split():
            return False, 'KCON: did not mount (rc %d %s)' % (rc, out.strip()[:100])
    return True, 'KingCON handler %d bytes from the owner\'s system disk (read only), KCON: mounted' % KING_HANDLER.stat().st_size


def cleanup():
    """KingCON out of L: and DEVS:, KCON: dismounted, the rig's own LIBS: and TERM back."""
    run('Delete >NIL: L:KingCON-handler DEVS:KingCON-mountlist QUIET')
    run('Assign >NIL: KCON: DISMOUNT')
    for cmd in GO_LIBS + GO_ENV:
        run(cmd)
    if rig_upterm_removed:
        run('Assign >NIL: UP-Term: ' + RIG_UPTERM_DRAWER)
    run('Delete >NIL: RAM:sa-cal.txt RAM:t.txt RAM:p.txt RAM:q.txt QUIET')
    run('Delete >NIL: ENVARC:Claude ENVARC:amiga-pi RAM:S RAM:.claude ALL QUIET')


RIG_UPTERM_DRAWER = 'VTCX:' + paths.UPTERM_DRAWER   # what rig.py's boot assigns UP-Term: to when no kit is installed (UT1)
rig_upterm_removed = False


def rig_upterm_assign(assign_list):
    """True when UP-Term: is in the `Assign LIST` text and points at the rig's own drawer (not a kit's)."""
    for line in assign_list.splitlines():
        f = line.split()
        if len(f) >= 2 and f[0].upper() == 'UP-TERM' and f[1].lower() == RIG_UPTERM_DRAWER.lower():
            return True
    return False


def check_stock():
    """The clean image: no UP-Term: assign and no ixemul in SYS:Libs; the stock ixemul's version.
    The rig's own UP-Term: (the glyph pages and the icon's tool, R9) is taken away for the run and
    put back by cleanup(); a kit's UP-Term: stays an error."""
    global rig_upterm_removed
    listing = run('Assign LIST')[1]
    if rig_upterm_assign(listing):
        run('Assign >NIL: UP-Term: REMOVE')
        rig_upterm_removed = True
    assigns = run('Assign LIST')[1].upper()
    if 'UP-TERM' in assigns:
        sys.exit('[ERROR] UP-Term: is assigned: this is not a stock Workbench (rig.py setup --os 32, not the old rig)')
    rc, out = run('Version SYS:Libs/ixemul.library')
    if rc == 0:
        sys.exit('[ERROR] SYS:Libs/ixemul.library exists (%s): not a stock Workbench' % out.strip())
    return run('Version VTC:sa/ixstock/ixemul.library')[1].strip() + ' / ' + run('Version VTC:sa/ixkit/ixemul.library')[1].strip()


# ---------------------------------------------------------------- the agents

def agent_prepare(prog):
    """The fixture-side files of the agent: trust for RAM:, the permissions, the files the prompts read."""
    run('Delete >NIL: ENVARC:Claude ENVARC:amiga-pi RAM:S RAM:.claude ALL QUIET')
    run('MakeDir >NIL: RAM:S RAM:.claude ENVARC:Claude ENVARC:amiga-pi')
    run('Copy SYS:S/Startup-Sequence RAM:S/Startup-Sequence')
    base = 'http://127.0.0.1:%d' % PORT
    if prog == 'claude':
        put('ENVARC:Claude/claude.json', json.dumps({'projects': {'Ram Disk:': {'hasTrustDialogAccepted': True}}}))
        put('RAM:.claude/settings.json', json.dumps({'permissions': {'allow': ['Read', 'Glob', 'Bash']},
                                                      'promptSuggestionEnabled': False, 'awaySummaryEnabled': False}))
    else:
        put('ENVARC:amiga-pi/models.json', json.dumps({'providers': {'fixo': {
            'baseUrl': base + '/v1', 'api': 'openai-completions', 'apiKey': '!Echo rig-test-key',
            'models': [{'id': 'm2', 'contextWindow': 32000}]}}}))
        put('ENVARC:amiga-pi/settings.json', json.dumps({'enabledModels': ['fixo/m2']}))


def agent_session(prog, kind):
    session = '%s-%s' % (prog, kind)
    dev, label = CONSOLES[kind]
    title = TITLE + session
    dump_screens = []
    seen_req = []
    res = {}
    shown = False
    typed_ok = False

    def fail_rest(why, from_check):
        names = CHECKS[prog]
        for ck in names[names.index(from_check):]:
            res.setdefault(ck, ('fail', why))

    fx = Fixture(delay=0.05 if prog == 'claude' else 0.25)
    try:
        agent_prepare(prog)
        run('Delete >NIL: ENVARC:Claude/history ENVARC:Claude/projects ALL QUIET') if prog == 'claude' else None
        open_window(dev, title, prelude_for('none', 'unset'))
        launch = ('VTC:sa/Claude URL=%s/v1/messages ROOT=RAM:' % ('http://127.0.0.1:%d' % PORT)) if prog == 'claude' \
            else 'VTC:sa/amiga-pi --model fixo/m2'
        banner = 'Claude' if prog == 'claude' else 'amiga-pi'
        procname = 'VTC:sa/Claude' if prog == 'claude' else 'VTC:sa/amiga-pi'
        typeline(launch, wait=1)
        text, unknown, ok = wait_text(title, dev, r'Type .*(help|exit)|^> ', 25)
        seen_req += [dismiss(w) for w in foreign(title)]
        text, unknown = screen(title, dev, session + '-start')
        up = alive(procname)
        res['start'] = ('pass' if ok and up and not seen_req else 'fail',
                        'alive=%s banner=%s requesters=%s screen: %s' % (up, ok, seen_req, ' | '.join(text.splitlines()[-4:])))
        dump_screens.append((text, unknown))
        if res['start'][0] == 'fail':
            fail_rest('did not start', 'conversation')
            raise StopIteration

        # conversation with a tool call; the prompt typed with a typo and corrected by Backspace
        n0 = fx.count()
        prompt = 'show me the startup' + (' tool' if prog == 'pi' else '')
        tw.typ('show mx')
        tw.key(BS, wait=0.5)
        tw.typ('e the startup' + (' tool' if prog == 'pi' else ''))
        typed, _ = screen(title, dev)
        tw.key(RET, wait=1)
        got = fx.wait_count(n0 + 2, 90)
        bodies = fx.bodies()[n0:]
        first = bodies[0] if bodies else ''
        tr = tool_result_text(bodies[1]) if len(bodies) > 1 else ''
        text, unknown, shown = wait_text(title, dev, r'SetPatch first', 40)
        text, unknown = screen(title, dev, session + '-conversation')
        dump_screens.append((text, unknown))
        has_tool = got and len(bodies) > 1 and bool(tr)
        res['conversation'] = ('pass' if has_tool and shown else 'fail',
                               'requests=%d tool_result=%r answer_on_screen=%s' % (len(bodies), tr[:120], shown))
        typed_ok = ('"%s"' % prompt in first or prompt in first) and 'show mx' not in first
        # the system prompt names the AmigaShell (bash fallback) -- checked once here
        sysmsg = system_text(first)
        res['_amigashell'] = 'AmigaShell' in sysmsg or 'AmigaShell' in first

        # Up recalls the last prompt
        n1 = fx.count()
        tw.key(UP, wait=1)
        recalled, _ = screen(title, dev, session + '-history')
        tw.key(RET, wait=1)
        fx.wait_count(n1 + 1, 40)
        again = fx.bodies()[n1:]
        up_ok = any(prompt in b for b in again[:1]) and not JUNK.search(recalled)
        last_rows = ' | '.join(recalled.splitlines()[-2:])
        res['history'] = ('pass' if up_ok else 'fail',
                          'requests after Up+Return=%d, carries "%s"=%s; screen: %s' % (len(again), prompt, up_ok, last_rows))
        fx.wait_count(n1 + 2, 40)
        time.sleep(6)

        # the bash tool: vsh absent, the AmigaShell runs it
        n2 = fx.count()
        if prog == 'claude':
            typeline('time limit', wait=1)
            fx.wait_count(n2 + 2, 90)
        else:
            typeline('shell', wait=1)
            fx.wait_count(n2 + 2, 90)
        screen(title, dev, session + '-bash')
        bb = fx.bodies()[n2:]
        btr = tool_result_text(bb[1]) if len(bb) > 1 else ''
        res['bash'] = ('pass' if len(bb) > 1 and btr and res.get('_amigashell') else 'fail',
                       'requests=%d tool_result=%r system prompt names the AmigaShell=%s' % (len(bb), btr[:160], res.get('_amigashell')))
        time.sleep(8)

        # Ctrl+C ends a running answer or tool
        n3 = fx.count()
        if prog == 'claude':
            typeline('slow', wait=1)
            fx.wait_count(n3 + 1, 60)
            time.sleep(4)
        else:
            typeline('say hello', wait=1)
            time.sleep(3)
        t0 = time.time()
        tw.key(KEY_C, CTRL, wait=1)
        if prog == 'claude':
            ended = fx.wait_count(n3 + 2, 40)
            dt = time.time() - t0
            cr = tool_result_text(fx.bodies()[n3 + 1]) if ended and fx.count() > n3 + 1 else ''
            cc_ok = ended and dt < 20
            cc_detail = 'Ctrl+C at 4 s into Wait 30: next request after %.0f s, tool_result=%r' % (dt, cr[:120])
        else:
            time.sleep(4)
            after, _ = screen(title, dev)
            cc_ok = alive(procname) and bool(re.search(r'Stopped|Interrupted|Aborted', after))
            cc_detail = 'alive after Ctrl+C=%s requests=%d; screen: %s' % (alive(procname), fx.count() - n3,
                                                                           ' | '.join(after.splitlines()[-3:]))
        stays = alive(procname)
        res['keys'] = ('pass' if typed_ok and cc_ok else 'fail',
                       'typed with Backspace reached the fixture as "%s"=%s; %s; program alive after Ctrl+C=%s'
                       % (prompt, typed_ok, cc_detail, stays))
        time.sleep(4)

        # leave, then the data
        if alive(procname):
            typeline('/exit' if prog == 'claude' else '/exit', wait=3)
            if alive(procname):
                tw.key(0x22, CTRL, wait=1)
                tw.key(0x22, CTRL, wait=2)
        listing = run('List ENVARC:%s ALL FILES' % ('Claude' if prog == 'claude' else 'amiga-pi'))[1]
        seen_req += [dismiss(w) for w in foreign(title)]
        sessions = [l for l in listing.splitlines() if '.jsonl' in l]
        hist = 'history' in listing if prog == 'claude' else True
        res['data'] = ('pass' if sessions and not seen_req else 'fail',
                       'session files in ENVARC:%s: %d, history file=%s, requesters over the whole run: %s, UP-Term: asked for: %s'
                       % ('Claude' if prog == 'claude' else 'amiga-pi', len(sessions), hist, seen_req,
                          'UP-Term:' in ' '.join(seen_req)))
        # the screen verdict over every text read
        bad = [garbled(t, u) for t, u in dump_screens]
        bad = [b for b in bad if b]
        res['screen'] = ('pass' if not bad and shown else 'fail',
                         'line mode (no escape sequences shown, banner and answer legible) ' if not bad and shown else
                         ('; '.join(bad) or 'the answer never appeared'))
    except StopIteration:
        pass
    finally:
        fx.stop()
        close_window(title)
        res.pop('_amigashell', None)
    for ck in CHECKS[prog]:
        v = res.get(ck, ('fail', 'not reached'))
        record(session, ck, v[0], v[1])


# ---------------------------------------------------------------- nvim and python

def nvim_session(kind, libs, term):
    session = 'nvim-%s-%s-%s' % (kind, libs, term)
    dev, _ = CONSOLES[kind]
    title = TITLE + 'nvim'
    res = {}
    run('Delete >NIL: RAM:t.txt QUIET')
    seen = []
    open_window(dev, title, prelude_for(libs, term))
    try:
        typeline('VTC:sa/nvim/bin/nvim -u NONE RAM:t.txt', wait=1)
        time.sleep(1)
        text, unknown, ok = wait_text(title, dev, r't\.txt', 20)
        seen += [dismiss(w) for w in foreign(title)]
        up = alive('nvim')
        text, unknown = screen(title, dev, session + '-start')
        errs = re.findall(r'E\d+: [^\n]*', text)
        res['start'] = ('pass' if up and ok and not seen else 'fail',
                        'alive=%s file name on screen=%s requesters=%s startup messages: %s; screen: %s' %
                        (up, ok, seen, errs, ' | '.join(text.splitlines()[-3:]) if text else '(blank)'))
        if res['start'][0] != 'pass':
            raise StopIteration
        tw.typ('ihello')
        time.sleep(1)
        text, unknown = screen(title, dev, session + '-screen')
        rows = text.splitlines()
        tildes = sum(1 for r in rows if r.startswith('~'))
        bad = garbled(text, unknown)
        basic_repl = "can't use pyrepl" in text.replace('\n', ' ')
        res['screen'] = ('pass' if rows and 'hello' in rows[0] and tildes >= 3 and not bad else 'fail',
                         'first row %r, %d rows of ~, %s' % (rows[0] if rows else '', tildes, bad or 'no escape sequences shown'))
        tw.key(ESC, wait=0.7)
        typeline(':wq', wait=3)
        for _ in range(10):
            if not alive('nvim'):
                break
            time.sleep(1.5)
        got = amiga_text('RAM:t.txt')
        after, _ = screen(title, dev, session + '-edit')
        res['edit'] = ('pass' if got == 'hello' and not alive('nvim') else 'fail',
                       'RAM:t.txt=%r after ihello<Esc>:wq, nvim still running=%s; screen: %s'
                       % (got, alive('nvim'), ' | '.join(after.splitlines()[-4:])))
        # N1: nvim's default 'shell' is sh, which AmigaOS has not (E475 ... 'sh' is not executable); the kit's
        # share/nvim/sysinit.vim names the base's vsh. Started without -u NONE, as a user does.
        step('nvim :terminal')
        # the base installs vsh in C: and UP-Term:bin; this rig has no base, so C: gets a copy for this check.
        # Evidence is files (no screen read): the 'shell' nvim ended with, and the exit status of the job.
        run('Copy VTC:vsh C:vsh CLONE')
        run('Delete >NIL: RAM:n1.txt RAM:n1.sh RAM:n1.st QUIET')
        put('VTC:sa/n1.vim', "call writefile([&shell], 'RAM:n1.sh')\n"
                             "autocmd TermClose * call writefile([string(v:event.status)], 'RAM:n1.st')\n")
        typeline('VTC:sa/nvim/bin/nvim -S VTC:sa/n1.vim', wait=10)
        typeline(':terminal', wait=6)
        tw.typ('i')
        typeline('echo hi >RAM:n1.txt', wait=4)
        typeline('exit', wait=6)
        sh, hi, st = amiga_text('RAM:n1.sh'), amiga_text('RAM:n1.txt'), amiga_text('RAM:n1.st')
        run('Delete >NIL: C:vsh QUIET')
        res['terminal'] = ('pass' if sh and 'vsh' in sh and (hi or '').strip() == 'hi' and (st or '').strip() == '0' else 'fail',
                           ":terminal: 'shell'=%r, echo hi wrote %r, job exit status %r" % (sh, hi, st))
    except StopIteration:
        for ck in ('edit', 'screen', 'terminal'):
            res[ck] = ('fail', 'did not start')
    finally:
        if alive('nvim'):
            tw.key(ESC, wait=0.5)
            typeline(':q!', wait=2)
        if alive('nvim'):
            kill('python3' if 'python' in session else 'nvim')
        close_window(title)
    for ck in CHECKS['nvim']:
        v = res.get(ck, ('fail', 'not reached'))
        record(session, ck, v[0], v[1])


def python_session(kind, libs, term):
    session = 'python-%s-%s-%s' % (kind, libs, term)
    dev, _ = CONSOLES[kind]
    title = TITLE + 'python'
    res = {}
    py = 'VTC:sa/python3/bin/python3'
    run('Delete >NIL: RAM:p.txt RAM:q.txt RAM:c.txt RAM:p.rc QUIET')
    seen = []
    open_window(dev, title, prelude_for(libs, term))
    try:
        step('python -c')
        typeline('%s -c "print(6*7)" >RAM:p.txt' % py, wait=1)
        for _ in range(30):
            if amiga_text('RAM:p.txt') is not None and not alive('python3'):
                break
            seen += [dismiss(w) for w in foreign(title)] if foreign(title) else []
            if seen:
                break
            time.sleep(1.5)
        out = amiga_text('RAM:p.txt')
        text, unknown = screen(title, dev, session + '-start')
        res['start'] = ('pass' if out == '42' and not seen else 'fail',
                        'python3 -c print(6*7) wrote %r, requesters=%s, screen: %s' % (out, seen, ' | '.join(text.splitlines()[-3:])))
        if res['start'][0] != 'pass':
            if alive('python3'):
                kill('python3' if 'python' in session else 'nvim')
            raise StopIteration
        step('python repl')
        typeline(py, wait=1)
        text, unknown, ok = wait_text(title, dev, r'>>>', 25)
        seen += [dismiss(w) for w in foreign(title)]
        text, unknown = screen(title, dev, session + '-screen')
        bad = garbled(text, unknown)
        basic_repl = "can't use pyrepl" in text.replace('\n', ' ')
        res['screen'] = ('pass' if ok and not bad else 'fail',
                         'prompt on screen=%s, %s; screen: %s' % (ok, bad or 'no escape sequences shown', ' | '.join(text.splitlines()[-3:])))
        step('python typed line')
        typeline("open('RAM:q.txt','w').write('x')", wait=3)
        first = amiga_text('RAM:q.txt')
        res['repl'] = ('pass' if first == 'x' else 'fail', 'typed line ran: RAM:q.txt=%r' % first)
        run('Delete >NIL: RAM:q.txt QUIET')
        step('python ctrl-c')
        # Python installs its SIGINT handler with SA_ONSTACK: an ixemul that then used the signal stack
        # nobody set (ixemul-vtcon sendsig, Y1) reset the machine at the first key after the Ctrl-C
        # A ROM CON: or KingCON Read cannot be interrupted (no vtcon handler): the KeyboardInterrupt comes
        # when the next line is read and takes that line, so the line may need typing twice
        tw.key(KEY_C, CTRL, wait=2)
        tries = 0
        for tries in (1, 2):
            typeline("open('RAM:c.txt','w').write('c')", wait=3)
            got = amiga_text('RAM:c.txt')
            if got == 'c':
                break
        res['ctrlc'] = ('pass' if got == 'c' else 'fail',
                        'Ctrl-C at the prompt, then a typed line: RAM:c.txt=%r after %d line(s)%s' % (
                            got, tries, ' (the first went to the KeyboardInterrupt)' if tries == 2 and got == 'c' else ''))
        step('python up')
        # Up recalls the LAST line typed (the Ctrl-C step's RAM:c.txt line, not the q.txt one of the repl step):
        # delete its file so Return after Up proves the recalled line ran. The console sends Up as the 8-bit
        # CSI 0x9B 'A' (ROM CON: and KingCON); PyREPL must take that as the Up key (cpython-amiga patch 0015)
        run('Delete >NIL: RAM:c.txt QUIET')
        tw.key(UP, wait=1)
        recalled, _ = screen(title, dev)
        tw.key(RET, wait=3)
        again = amiga_text('RAM:c.txt')
        detail = 'after Up+Return RAM:c.txt=%r; screen: %s' % (again, ' | '.join(recalled.splitlines()[-2:]))
        if basic_repl:
            # no terminal type: PyREPL refuses ("can't use pyrepl", no clear capability) and Python reads a
            # cooked line; Up is then the console's own business (ROM CON: has no history for a Read, KingCON has)
            res['history'] = ('observed', 'basic REPL (TERM unset), Up is the console\'s: ' + detail)
        else:
            res['history'] = ('pass' if again == 'c' else 'fail', detail)
        # a line typed after the Up must still run (Y2-TERM b: the undecodable 0x9B once stayed in the buffer)
        typeline("open('RAM:u.txt','w').write('u')", wait=3)
        after = amiga_text('RAM:u.txt')
        res['afterup'] = ('pass' if after == 'u' else 'fail', 'a typed line after Up+Return: RAM:u.txt=%r' % after)
        run('Delete >NIL: RAM:u.txt QUIET')
    except StopIteration:
        for ck in ('screen', 'repl', 'ctrlc', 'history', 'afterup'):
            res[ck] = ('fail', 'did not start')
    finally:
        step('python leaving')
        if alive('python3'):
            typeline('exit()', wait=2)
        if alive('python3'):
            kill('python3' if 'python' in session else 'nvim')
        close_window(title)
    for ck in CHECKS['python']:
        v = res.get(ck, ('fail', 'not reached'))
        record(session, ck, v[0], v[1])


def pi_mathlib():
    ok = run('Version LIBS:mathieeedoubbas.library')
    record('pi-any', 'mathlib', 'pass' if ok[0] == 0 else 'fail', 'Version LIBS:mathieeedoubbas.library: %s' % ok[1].strip())


# ---------------------------------------------------------------- main

def run_session(name):
    parts = name.split('-')
    prog, kind = parts[0], parts[1]
    if kind == 'king':
        ok, note = kingcon_ready()
        if not ok:
            for ck in CHECKS[prog]:
                record(name, ck, 'fail', 'KingCON not available: ' + note)
            return
    close_strays()
    say('[INFO] session %s' % name)
    if prog in ('claude', 'pi'):
        agent_session(prog, kind)
    elif prog == 'nvim':
        nvim_session(kind, parts[2], parts[3])
    else:
        python_session(kind, parts[2], parts[3])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--only', action='append', default=[], help='sessions whose name starts with this')
    ap.add_argument('--failed', action='store_true', help='only the sessions with a failed verdict')
    ap.add_argument('--force', action='store_true', help='all selected sessions, recorded or not')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--cleanup', action='store_true')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if a.list:
        for s in SESSIONS:
            ok, failed = session_state(s)
            print('%-34s %s' % (s, 'FAILED' if failed else 'recorded' if ok else 'not run'))
        return 0
    if a.cleanup:
        cleanup()
        return 0
    todo = [s for s in SESSIONS if not a.only or any(s.startswith(o) for o in a.only)]
    if not a.force:
        todo = [s for s in todo if not session_state(s)[0] or (a.failed and session_state(s)[1])]
    if a.failed:
        todo = [s for s in todo if session_state(s)[1]]
    if not todo:
        say('every selected session is recorded with this build (--force to run again)')
        return 0
    stage()
    versions = check_stock()
    say('[INFO] ixemul stock / kit: %s' % versions.replace('\n', ' '))
    try:
        need = {CONSOLES[s.split('-')[1]][0] for s in todo}
        for dev in sorted(need):
            if dev == 'KCON':
                ok, note = kingcon_ready()
                say('[INFO] ' + note)
                if not ok:
                    continue
            READER.calibrate(dev)
        for s in todo:
            started = time.time()
            try:
                run_session(s)
            except (SystemExit, OSError) as e:
                if not isinstance(e, OSError) and 'connection closed' not in str(e) and 'refused' not in str(e):
                    raise
                alert = emulator_text()
                note = 'the machine stopped answering during this session (%s), last step: %s; the emulator window shows: %s' % (
                    str(e)[:60], LAST_STEP[0], ' | '.join(alert[:6]) or '(nothing readable)')
                say('[CRASH] %s: %s' % (s, note))
                (OUT / 'crashes.log').open('a').write('%s %s %s\n' % (time.strftime('%F %T'), s, note))
                for ck in CHECKS[s.split('-')[0]]:
                    v = load(s, ck)
                    if not v or vpath(s, ck).stat().st_mtime < started:
                        record(s, ck, 'fail', note)
                if not wait_rig():
                    say('[ERROR] the rig did not come back; start it again (rig.py start --os 32 --max) and run --failed')
                    return 1
        if any(s.startswith('pi-') for s in todo):
            pi_mathlib()
    finally:
        close_strays()
        cleanup()
    bad = [s for s in todo if session_state(s)[1]]
    say('standalone_rig: %d sessions run, %d with a failed verdict' % (len(todo), len(bad)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
