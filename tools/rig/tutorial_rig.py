#!/usr/bin/env python3
"""tutorial_rig.py -- the shell tutorial on the rig: L-C1 (every lesson's --selftest through vsh) and L-R (Help >
Shell tutorial, /tutorial, lesson 1 typed to its end, :quit then `tutorial next`, the progress file, the
vshrc first-run hint).

Cases, in this order (each sets the progress file it needs, so --only runs any case alone):
  selftest-01 .. selftest-13   UP-Term:bin/vsh UP-Term:bin/tutorial --selftest <lesson>: "ok N" for every step
  hint-before   a fresh vsh window with no progress prints the vshrc hint
  typed         `tutorial` typed at the vsh prompt lists all 13 lessons (the hint's advice)
  menu          Help > Shell tutorial opens a tab that stays up at a tutorial prompt
  slash         /tutorial does the same
  lesson1       (run as `vsh UP-Term:bin/tutorial`, not the typed command: see typed) lesson 1 typed to its end: "Lesson finished."
  progress      lesson 1 again from no progress, then $HOME/.tutorial (System:.tutorial) holds the line 1
  resume        progress 1: :quit in lesson 2, then `tutorial next` starts lesson 2 again
  hint-after    progress 1: a fresh vsh window prints no hint

Verdicts go to build/<rig>/tutorial.verdicts.json when a case ends; a passed case with the same files is skipped.
  tutorial_rig.py                 every case not yet passed
  tutorial_rig.py --only menu     one case (repeat for more), even when it passed
  tutorial_rig.py --force         every case, from zero
Helpers: typewin.py (open_xcon/typ/key/screen_text, the ones amigapi step 13 uses) and vshwin.py (a vsh window).
Usage: UPTERM_RIG=<n> python3 -I tools/rig/tutorial_rig.py
Rig up with the kit installed (install_rig.prepare + runinstall) and rebooted; `make amiga build/amiga/snipprobe`."""
import glob, hashlib, json, os, re, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, typewin, vshwin
import condev_rig as c

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VERDICTS = paths.RESULTS / "tutorial.verdicts.json"
TUT = "UP-Term:bin/vsh UP-Term:bin/tutorial"
PROGRESS = "System:.tutorial"
HINT = "New to a Unix shell?"
LESSONS = sorted(os.path.basename(f) for f in glob.glob(ROOT + "/dist/tutorial-lessons/*.lesson"))
# the Amiga's file names are 30 characters: the kit's copy of a longer name is cut
AMIGA_NAME = lambda n: n[:30]
CASES = ["selftest-%s" % n[:2] for n in LESSONS] + ["hint-before", "typed", "menu", "slash", "lesson12", "lesson1", "progress", "resume", "hint-after", "check-a", "check-b", "check-c", "check-d"]
ANSWERS1 = ["ls", "pwd", "cd docs", "cd .."]     # lesson 1's four steps


def fingerprint():
    h = hashlib.sha256()
    for f in ["dist/tutorial", "dist/vshrc", "build/amiga/vtcon-handler"] + ["dist/tutorial-lessons/" + n for n in LESSONS]:
        p = os.path.join(ROOT, f)
        h.update(open(p, "rb").read() if os.path.exists(p) else b"missing " + f.encode())
    return h.hexdigest()[:16]


def load():
    return json.loads(VERDICTS.read_text()) if VERDICTS.exists() else {}


def record(name, ok, detail):
    d = load()
    d[name] = {"verdict": "pass" if ok else "fail", "fp": FP, "detail": detail[-600:], "time": time.time()}
    VERDICTS.parent.mkdir(parents=True, exist_ok=True)
    VERDICTS.write_text(json.dumps(d, indent=1))
    print("[%s] %s %s" % ("PASS" if ok else "FAIL", name, "" if ok else detail[-300:].replace("\n", " | ")), flush=True)


def until(cond, secs):
    end = time.time() + secs
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(1.5)
    return cond()


def tut_running():
    return "tutorial" in c.run("Status FULL")[1].lower()


def fresh_vsh(title):
    vshwin.open_vsh(title, box="0/12/760/440", wait=6)
    typewin._last_title = title
    typewin.click_in(title)
    time.sleep(1)


def close_vsh():
    typewin.typ("exit")
    typewin.key(typewin.RET, wait=2)


def case_selftest(n):
    """the lesson's own answers on Amiga vsh. The answers of the window lesson act on the window
    (UPTERM_STATE act ...), which --selftest cannot do: the host test's fake state stands in, as
    in tests/test_tutorial_lessons.py"""
    name = AMIGA_NAME(next(l for l in LESSONS if l.startswith(n)))
    ami.put(os.path.join(ROOT, "tests", "fake-upterm-state"), "RAM:fake-upterm-state")
    c.run("Delete RAM:fakestate ALL QUIET >NIL:", 30)
    c.run("MakeDir RAM:fakestate", 30)
    rc, out = c.run("UP-Term:bin/vsh -c \"UPTERM_STATE='UP-Term:bin/vsh RAM:fake-upterm-state' "
                    "UPTERM_FAKE_DIR=RAM:fakestate %s --selftest UP-Term:share/tutorial/%s\""
                    % (TUT, name), 180)
    steps = [l for l in out.splitlines() if l.startswith(("ok ", "FAIL"))]
    ok = rc == 0 and steps and all(l.startswith("ok ") for l in steps)
    return ok, "rc %s: %s" % (rc, out)


def case_hint(expect):
    set_progress([] if expect else [1])
    fresh_vsh("hintwin")
    text = typewin.screen_text()
    close_vsh()
    return (HINT in text) == expect, text[-300:]


def vsh_pids():
    """The tutorial tab's process is a vsh loaded from UP-Term:bin/vsh (the host's window runs VTC:vsh).
    screen_text() reads the host's session whatever tab is active (a plain UP-Term > New tab reads the same),
    so the tab is judged by its process: alive after the script ran means the tutorial did not end at once."""
    return [l.split(":")[0].split()[-1] for l in c.run("Status FULL")[1].splitlines() if "Loaded as command: UP-Term:bin/vsh" in l]


RCMD = 0x0080
SANDBOX = "T:tutorial"


def active_title():
    """The title of the active window (UITREE W line): with tabs it is the active tab's title, so it names the
    tab that has the keys (tabs_rig.ours() reads the same line)."""
    for l in ami.req(0x0D).decode("latin-1").splitlines():
        f = l.split()
        if l.startswith("W ") and len(f) > 5 and f[5] == "active":
            return l.split('"')[-2]
    return None


def tab_type(s, wait=2.0):
    """Type a line into the active tab (tabs_rig.t(): the window's own keys, then Return)."""
    ami.req(0x08, bytes([4]) + s.encode("latin-1"))
    time.sleep(0.5)
    ami.key(typewin.RET)
    time.sleep(wait)


def set_progress(nums):
    """System:.tutorial holds the numbers, one per line (written on the host through the VTC: assign: an Echo
    argument cannot carry a newline)."""
    (paths.RIG / "vtc" / "tut-progress").write_text("".join("%d\n" % n for n in nums))
    c.run("Delete %s QUIET" % PROGRESS)
    if nums:
        c.run("Copy VTC:tut-progress %s QUIET" % PROGRESS)


def saved_progress():
    """The progress file's numbers, to put back with set_progress; none when there is no file (Type's error text
    carries a fault number, which is not progress)."""
    rc, out = c.run("Type %s" % PROGRESS)
    return [int(n) for n in out.split() if n.isdigit()] if rc == 0 else []


def sandbox_there():
    return c.run("List >NIL: %s" % SANDBOX)[0] == 0


def open_tab(how):
    """Host window "tabwin", then the real entry: Help > Shell tutorial or the typed /tutorial. Returns the host's
    title; the new tab is active when active_title() differs."""
    c.run("Delete T:UP-Term-tutorial %s QUIET ALL" % SANDBOX)
    fresh_vsh("tabwin")
    if how == "menu":
        with ami._conn(60) as conn:
            ami._m().ui_menu(conn, "Help/Shell tutorial", "tabwin")
    else:
        typewin.typ("/tutorial")
        typewin.key(typewin.RET, wait=1)


def close_host():
    if ami.window("tabwin"):
        typewin.click_in("tabwin")
        close_vsh()


def case_tab(how):
    """The tutorial tab judged by the window title (the new tab is active, not "tabwin") and the sentinel
    enter_sandbox leaves: T:tutorial exists while the lesson waits at its prompt (dist/tutorial:247-254).
    screen_text() reads the host's session, so the tab's own text is never read."""
    open_tab(how)
    try:
        up = until(lambda: sandbox_there(), 20)
        time.sleep(6)
        title = active_title()
        still = sandbox_there()
        ok = bool(up) and still and title not in (None, "tabwin")
        detail = "sandbox appeared %s, still there after 6 s %s, active title %r" % (bool(up), still, title)
        if ok:
            # :quit ends the lesson and the tab closes: the host's title comes back
            tab_type(":quit", 3)
            back = bool(until(lambda: active_title() == "tabwin", 10))
            ok = back and not sandbox_there()
            detail += "; after :quit the host is active %s, sandbox gone %s" % (back, not sandbox_there())
    finally:
        if active_title() not in (None, "tabwin"):
            ami.key(typewin.RET)
            tab_type("exit", 2)
        close_host()
    return ok, detail


def counters():
    """`UPTerm state` typed into the active tab (the host's): the handler's counters, as {key: int}."""
    c.run("Delete RAM:cnt.out QUIET")
    tab_type("UPTerm state >RAM:cnt.out", 2)
    out = c.run("Type RAM:cnt.out")[1]
    d = {}
    for l in out.splitlines():
        f = l.replace("=", " ").replace(":", " ").split()
        if len(f) >= 2 and f[-1].lstrip("-").isdigit():
            d[f[0]] = int(f[-1])
    return d, out


LINE = 16                                # pixel rows per text line: the tab is drawn at twice topaz 8's height


class Screen:
    """The tab's text, found by pixels (no screen reader): the cells are topaz 8, 8x8 (mouse_rig.cell_xy), the
    text area starts 4 px in. ink = a pixel unlike the background in the window's last column."""

    def __init__(self):
        for l in ami.req(0x0D).decode("latin-1").splitlines():
            f = l.split()
            if l.startswith("W ") and len(f) > 5 and f[5] == "active":
                self.x, self.y = int(f[2]), int(f[3])
                self.w, self.h = map(int, f[4].split("x"))
        b = ami.req(0x07)
        self.b, self.sw = b, struct.unpack(">H", b[2:4])[0]
        self.x0 = self.x + 4
        self.cols = (self.w - 24) // 8          # text columns; the last one is always background
        self.y0 = self.y + 22
        self.bg = self.px(self.x0 + (self.cols - 1) * 8 + 3, self.y + self.h // 2)

    def px(self, x, y):
        o = 8 + (y * self.sw + x) * 3
        return bytes(self.b[o:o + 3])

    def ink(self, y, c0, c1):
        o = 8 + (y * self.sw + self.x0 + (c0 - 1) * 8) * 3
        row = self.b[o:o + (c1 - c0 + 1) * 8 * 3]
        return row != self.bg * ((c1 - c0 + 1) * 8)

    def rows(self, c0, c1):
        return [y for y in range(self.y0, self.y + self.h - 12) if self.ink(y, c0, c1)]

    def line_sig(self, bottom, k, c0, c1):
        """The pixels of text line k above the bottom (prompt) line, columns c0..c1: the 12 pixel rows (6 glyph rows, each drawn
        twice) above the baseline (descenders of the line above never leak in). bottom = the prompt's last ink row."""
        out = b""
        for y in range(bottom - LINE * k - 11, bottom - LINE * k + 1):
            o = 8 + (y * self.sw + self.x0 + (c0 - 1) * 8) * 3
            out += bytes(self.b[o:o + (c1 - c0 + 1) * 8 * 3])
        return out

    def task_rows(self):
        """[(y_center, last_col)] of the text lines above the last prompt group, nearest first: lines whose
        first 9 columns equal the bottom prompt ("tutorial> ", also with a typed command after it) and blank
        lines are skipped; every other line is a candidate for the task line's end."""
        last = self.rows(1, self.cols)
        if not last:
            return []
        bottom = last[-1]
        prompt = self.line_sig(bottom, 0, 1, 9)
        out = []
        for k in range(1, (bottom - self.y0) // LINE):
            if self.line_sig(bottom, k, 1, 9) == prompt:
                continue
            sig = self.line_sig(bottom, k, 1, self.cols)
            if sig == self.bg * (len(sig) // 3):
                continue
            cols = [cc for cc in range(self.cols, 0, -1)
                    if self.line_sig(bottom, k, cc, cc) != self.bg * (12 * 8)]
            out.append((bottom - LINE * k - 6, cols[0]))
        return out

    def task_row(self, skip=0):
        """(y_center, last_col) of the skip-th candidate task line (see task_rows), or None."""
        rows = self.task_rows()
        return rows[skip] if skip < len(rows) else None


    def reprompted(self):
        """True when the bottom row (the "tutorial> " prompt) is pixel-identical, over the prompt's 9 columns, to
        the row above it: an empty line only re-prompted and the step did not advance. After a pass the row above
        the prompt is the next task (or its wrapped tail), never a prompt."""
        last = self.rows(1, self.cols)
        if not last:
            return True
        y = last[-1]
        # the prompt's glyph rows: compare the 8 pixel rows ending at the bottom ink row with the 8 above
        return all(self.b[8 + ((y - d) * self.sw + self.x0) * 3:8 + ((y - d) * self.sw + self.x0 + 72) * 3] ==
                   self.b[8 + ((y - d - LINE) * self.sw + self.x0) * 3:8 + ((y - d - LINE) * self.sw + self.x0 + 72) * 3]
                   for d in range(12))

    def word_span(self, skip=0):
        """(y_center, first_px, last_px) of the word at the end of the task row right above the prompt: the last
        ink column of that row and the contiguous ink columns before it (a space ends the word)."""
        t = self.task_row(skip)
        if not t:
            return None
        y, last = t
        rows = [yy for yy in range(y - 4, y + 5)]
        first = last
        while first > 1 and any(self.ink(yy, first - 1, first - 1) for yy in rows):
            first -= 1
        return y, self.x0 + (first - 1) * 8, self.x0 + last * 8


def settled(secs=20):
    """The Screen once two snapshots 1.5 s apart find the same text rows: the mirror is still drawing the last
    Return's prompt and scroll before that (a snapshot taken then put the task line 2 rows too high and the drag
    selected nothing: rig M, 2026-10-19, 3 of 4 runs)."""
    prev, end = None, time.time() + secs
    while True:
        sc = Screen()
        rows = sc.task_rows()
        if rows and rows == prev or time.time() > end:
            return sc
        prev = rows
        time.sleep(1.5)


def drag(x0, x1, y):
    kx, ky = ami.pointer_scale()
    steps = [("move", int(x0 * kx), int(y * ky)), ("wait", 2), ("button", 0, 1), ("wait", 2)]
    n = 6
    for i in range(1, n + 1):
        steps += [("move", int((x0 + (x1 - x0) * i / n) * kx), int(y * ky)), ("wait", 1)]
    steps += [("wait", 2), ("button", 0, 0), ("wait", 5)]
    ami.script(*steps)
    time.sleep(1.5)


def case_lesson12():
    """Lesson 12 for real, progress 1-11: every step done with the keys, the menu and the mouse in the tab. The
    empty Return of an action step cannot be rescued by the lesson's answer (dist/tutorial:149-160: a failing empty
    line only re-prompts), so "12" in the progress file proves each action step was seen by the handler's counters.
    The Return is repeated up to 3 times when the tab still shows the same prompt (the mirror lagging); retries
    are listed in the detail."""
    saved = saved_progress()
    set_progress(range(1, 12))
    log, retries, ok, host_title = [], [], False, "tabwin"
    try:
        open_tab("menu")
        # the baseline is read from the host before the tab exists: open it first, read from tab 1 after
        if not until(sandbox_there, 20):
            return False, "tutorial tab did not start (no %s)" % SANDBOX
        time.sleep(6)
        tut = active_title()
        ami.key(0x01, RCMD)
        until(lambda: active_title() == host_title, 8)
        base, out0 = counters()
        ami.key(0x02, RCMD)
        until(lambda: active_title() == tut, 8)
        time.sleep(2)

        def ret(name):
            """Return in the tutorial tab; repeated only while the prompt row is still the same prompt (the step
            did not advance: the mirror lagging). Never pressed again once the next task shows."""
            for n in range(3):
                ami.key(typewin.RET)
                time.sleep(3)
                if not Screen().reprompted():
                    if n:
                        retries.append("%s x%d" % (name, n))
                    return True
            retries.append("%s gave up" % name)
            return False

        ami.key(0x14, RCMD)                      # 1. Right Amiga T
        time.sleep(4)
        ami.key(0x02, RCMD)                      # back to the tutorial tab (tab 2)
        until(lambda: active_title() == tut, 8)
        log.append(("tabs-opened", ret("step1")))
        ami.key(0x01, RCMD); time.sleep(2)       # 2. switch away and back
        ami.key(0x02, RCMD); time.sleep(2)
        log.append(("tab-switches", ret("step2")))
        with ami._conn(60) as conn:              # 3. Help > Slash commands
            ami._m().ui_menu(conn, "Help/Slash commands", active_title())
        time.sleep(2)
        log.append(("menu-picks", ret("step3")))
        tab_type("/help", 2)                     # 4. a slash command; the shell sees an empty line
        ret("step4")                             # Return even when /help output sits above the prompt
        log.append(("slash", True))
        sc = settled()                           # 5. drag over the word at the end of the task line
        clip = None
        for skip in range(4):                    # a try again on a settled screen when the clipboard is not "up<pid>"
            w = sc.word_span(0)
            if not w:
                break
            y, px0, px1 = w
            drag(px0 + 1, px1 - 1, y)
            ami.key(0x33, RCMD)                  # copy-on-select is off by default: Right Amiga C copies the selection
            time.sleep(1)
            clip = typewin._clip("RAM:rig.clip").strip()
            if re.fullmatch(r"up\d+", clip):
                break
            kx, ky = ami.pointer_scale()         # one click clears the selection
            ami.script(("move", int(px0 * kx), int(y * ky)), ("wait", 2), ("button", 0, 1), ("wait", 2), ("button", 0, 0), ("wait", 3))
            sc = settled()
        log.append(("copies", ret("step5")))
        ami.req(0x08, bytes([4]) + b"echo ")     # 6. echo + paste
        time.sleep(0.5)
        ami.key(0x34, RCMD)
        time.sleep(1)
        ami.key(typewin.RET)
        time.sleep(3)
        said = c.run("Type %s.out" % SANDBOX)[1].strip()
        log.append(("paste", bool(re.fullmatch(r"up\d+", said))))
        ami.key(0x27, RCMD)                      # 7. Right Amiga K
        time.sleep(1)
        log.append(("clear", ret("step7")))
        time.sleep(3)
        prog = c.run("Type %s" % PROGRESS)[1].split()
        done = "12" in prog
        ami.key(0x01, RCMD)
        until(lambda: active_title() == host_title, 8)
        after, out1 = counters()
        moved = {k: (base.get(k), after.get(k)) for k in ("tabs-opened", "tab-switches", "menu-picks", "slash-count", "copies")}
        grew = all((v[1] or 0) > (v[0] or 0) for v in moved.values())
        # per-step proof is the progress file: "12" is written only after all seven checks passed (dist/tutorial),
        # and every counter grew. The pixel test of ret() can miss an advance, so it is detail, not a verdict.
        ok = done and grew and dict(log)["paste"]
        detail = "progress %s; steps %s; counters %s; retries %s; echo said %r" % (prog, log, moved, retries or "none", said)
        if not (base or after):
            detail += "; state output %r" % out1[:80]
    finally:
        if sandbox_there():                      # the lesson still waits: end it in the tutorial tab only
            ami.key(0x02, RCMD)
            if until(lambda: active_title() not in (None, "", host_title), 8):
                tab_type(":quit", 1)
                until(lambda: active_title() == host_title, 10)
        if active_title() == host_title:
            close_host()
        set_progress(saved)
    return ok, detail


def case_lesson1():
    c.run("Delete %s QUIET" % PROGRESS)
    fresh_vsh("lesswin")
    typewin.typ(TUT + " 1")
    typewin.key(typewin.RET, wait=4)
    for a in ANSWERS1:
        typewin.typ(a)
        typewin.key(typewin.RET, wait=2.5)
    text = typewin.screen_text()
    close_vsh()
    return "Lesson finished." in text, text[-400:]


def case_typed():
    """What the hint tells a newcomer to do: `tutorial` typed at the vsh prompt lists the lessons (all 11)."""
    fresh_vsh("typedwin")
    typewin.typ("tutorial")
    typewin.key(typewin.RET, wait=4)
    text = typewin.screen_text()
    close_vsh()
    n = sum(1 for l in text.splitlines() if l.strip().startswith("[") and "]" in l[:8])
    return n == len(LESSONS), "%d of %d lessons listed: %r" % (n, len(LESSONS), text[-300:])


def case_progress():
    case_lesson1()
    rc, out = c.run("Type %s" % PROGRESS)
    return rc == 0 and out.split() == ["1"], "rc %s: %r" % (rc, out)


def case_resume():
    set_progress([1])
    fresh_vsh("reswin")
    typewin.typ(TUT + " next")
    typewin.key(typewin.RET, wait=4)
    first = typewin.screen_text()
    typewin.typ(":quit")
    typewin.key(typewin.RET, wait=2)
    typewin.typ(TUT + " next")
    typewin.key(typewin.RET, wait=4)
    second = typewin.screen_text()
    typewin.typ(":quit")
    typewin.key(typewin.RET, wait=2)
    close_vsh()
    if "No such file" in first and ".tutorial" in first.split("tutorial next")[-1]:
        return False, "blocked-ixemul: grep cannot open System:.tutorial (Vol:/x), so next_lesson sees no progress"
    ok = "Looking at files" in first and "Looking at files" in second.split("tutorial next")[-1]
    return ok, "after :quit and tutorial next: %r" % second[-300:]


def case_check_a():
    """tutorial and upkg-ask run at the vsh prompt: arguments and exit status pass through, no S bit."""
    flags = [c.run("List UP-Term:bin/%s" % n)[1].split("\n")[1].split()[2] for n in ("tutorial", "upkg-ask")]
    rc, out = c.run("UP-Term:bin/vsh UP-Term:bin/tutorial foo")
    rc2, out2 = c.run("UP-Term:bin/vsh UP-Term:bin/upkg-ask")
    ok = all("s" not in f for f in flags) and rc == 2 and "usage" in out and "not executable" not in out + out2
    return ok, "flags %s; tutorial foo rc %s %r; upkg-ask rc %s %r" % (flags, rc, out[:60], rc2, out2[:80])


def case_check_b():
    rc, out = c.run("%s" % TUT)
    names = [open(os.path.join(ROOT, "dist", "tutorial-lessons", n)).readline()[2:].strip() for n in LESSONS]
    lines = [l for l in out.splitlines() if l.strip().startswith("[")]
    miss = [n for n in names if n.lower() not in out.lower()]
    return len(lines) == len(LESSONS) == 13 and not miss, "%d lessons listed; missing %s" % (len(lines), miss)


def case_check_c():
    """Every lesson finished: Help > Shell tutorial opens a tab that stays open on the lesson prompt. Judged by
    the title and the sentinel: no sandbox at the prompt (dist/tutorial:289-295), then `1` makes T:tutorial."""
    saved = saved_progress()
    set_progress(range(1, len(LESSONS) + 1))
    try:
        open_tab("menu")
        time.sleep(8)
        title = active_title()
        idle = not sandbox_there()
        stays = title not in (None, "tabwin")
        tab_type("1", 3)
        up = bool(until(sandbox_there, 15))
        tab_type(":quit", 3)
        back = bool(until(lambda: active_title() == "tabwin", 10))
        ok = idle and stays and up and back
        detail = "tab open on the prompt %s (title %r), no sandbox before the number %s, 1 starts lesson 1 %s, :quit closes %s" % (stays, title, idle, up, back)
    finally:
        if active_title() not in (None, "tabwin"):
            tab_type("", 1)
            tab_type("exit", 2)
        close_host()
        set_progress(saved)
    return ok, detail


def case_check_d():
    """A 2-line AmigaDOS script with the S bit still runs."""
    c.run("Echo >T:chk-d.script \"Echo chk-d-ran\"")
    c.run("Protect T:chk-d.script +s")
    rc, out = c.run("T:chk-d.script")
    c.run("Delete T:chk-d.script QUIET")
    return rc == 0 and "chk-d-ran" in out, "rc %s: %r" % (rc, out)


def run_case(name):
    if name.startswith("selftest-"):
        return case_selftest(name[-2:])
    return {"hint-before": lambda: case_hint(True), "typed": case_typed, "menu": lambda: case_tab("menu"), "slash": lambda: case_tab("slash"), "lesson12": case_lesson12,
            "lesson1": case_lesson1, "progress": case_progress, "resume": case_resume,
            "hint-after": lambda: case_hint(False),
            "check-a": case_check_a, "check-b": case_check_b, "check-c": case_check_c, "check-d": case_check_d}[name]()


def main(argv):
    only = [argv[i + 1] for i, a in enumerate(argv) if a == "--only"]
    force = "--force" in argv
    for o in only:
        if o not in CASES:
            sys.exit("unknown case %s; cases: %s" % (o, " ".join(CASES)))
    have = load()
    todo = only or [n for n in CASES if force or have.get(n, {}).get("fp") != FP or have[n]["verdict"] != "pass"]
    bad = 0
    for name in todo:
        try:
            ok, detail = run_case(name)
        except SystemExit as e:
            ok, detail = False, "script stop: %s" % e
        record(name, bool(ok), detail)
        bad += not ok
    print("tutorial_rig: %d case(s) run, %d failed" % (len(todo), bad))
    sys.exit(1 if bad else 0)


FP = fingerprint()
if __name__ == "__main__":
    main(sys.argv[1:])
