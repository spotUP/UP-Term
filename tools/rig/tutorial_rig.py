#!/usr/bin/env python3
"""tutorial_rig.py -- the shell tutorial on the rig: L-C1 (every lesson's --selftest through vsh) and L-R (Help >
Shell tutorial, /tutorial, lesson 1 typed to its end, :quit then `tutorial next`, the progress file, the
vshrc first-run hint).

Cases, in this order (a later one may need an earlier one's state, so --only runs just the named case):
  selftest-01 .. selftest-11   UP-Term:bin/vsh UP-Term:bin/tutorial --selftest <lesson>: "ok N" for every step
  hint-before   a fresh vsh window with no progress prints the vshrc hint
  typed         `tutorial` typed at the vsh prompt lists all 11 lessons (the hint's advice)
  menu          Help > Shell tutorial opens a tab that stays up at a tutorial prompt
  slash         /tutorial does the same
  lesson1       (run as `vsh UP-Term:bin/tutorial`, not the typed command: see typed) lesson 1 typed to its end: "Lesson finished."
  progress      $HOME/.tutorial (System:.tutorial) holds the line 1
  resume        :quit in lesson 2, then `tutorial next` starts lesson 2 again
  hint-after    a fresh vsh window after lesson 1 prints no hint

Verdicts go to build/<rig>/tutorial.verdicts.json when a case ends; a passed case with the same files is skipped.
  tutorial_rig.py                 every case not yet passed
  tutorial_rig.py --only menu     one case (repeat for more), even when it passed
  tutorial_rig.py --force         every case, from zero
Helpers: typewin.py (open_xcon/typ/key/screen_text, the ones amigapi step 13 uses) and vshwin.py (a vsh window).
Usage: UPTERM_RIG=<n> python3 -I tools/rig/tutorial_rig.py
Rig up with the kit installed (install_rig.prepare + runinstall) and rebooted; `make amiga build/amiga/snipprobe`."""
import glob, hashlib, json, os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths, ami, typewin, vshwin
import condev_rig as c

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VERDICTS = paths.RIG / "tutorial.verdicts.json"
TUT = "UP-Term:bin/vsh UP-Term:bin/tutorial"
PROGRESS = "System:.tutorial"
HINT = "New to a Unix shell?"
LESSONS = sorted(os.path.basename(f) for f in glob.glob(ROOT + "/dist/tutorial-lessons/*.lesson"))
# the Amiga's file names are 30 characters: the kit's copy of a longer name is cut
AMIGA_NAME = lambda n: n[:30]
CASES = ["selftest-%s" % n[:2] for n in LESSONS] + ["hint-before", "typed", "menu", "slash", "lesson1", "progress", "resume", "hint-after"]
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
    name = AMIGA_NAME(next(l for l in LESSONS if l.startswith(n)))
    rc, out = c.run("%s --selftest UP-Term:share/tutorial/%s" % (TUT, name), 180)
    steps = [l for l in out.splitlines() if l.startswith(("ok ", "FAIL"))]
    ok = rc == 0 and steps and all(l.startswith("ok ") for l in steps)
    return ok, "rc %s: %s" % (rc, out)


def case_hint(expect):
    c.run("Delete %s QUIET" % PROGRESS) if expect else None
    fresh_vsh("hintwin")
    text = typewin.screen_text()
    close_vsh()
    return (HINT in text) == expect, text[-300:]


def case_tab(how):
    c.run("Delete T:UP-Term-tutorial QUIET")
    fresh_vsh("tabwin")
    if how == "menu":
        with ami._conn(60) as conn:
            ami._m().ui_menu(conn, "Help/Shell tutorial", "tabwin")
    else:
        typewin.typ("/tutorial")
        typewin.key(typewin.RET, wait=1)
    wrote = until(lambda: c.run("List >NIL: T:UP-Term-tutorial")[0] == 0, 15)
    time.sleep(4)
    alive = tut_running()
    text = typewin.screen_text() if ami.window("tabwin") else ""
    ok = bool(wrote) and (alive or "tutorial>" in text)
    detail = "script written %s; tutorial process running %s; screen tail %r" % (bool(wrote), alive, text[-160:])
    if ami.window("tabwin"):
        typewin.click_in("tabwin")
        close_vsh()
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
    rc, out = c.run("Type %s" % PROGRESS)
    return rc == 0 and out.split() == ["1"], "rc %s: %r" % (rc, out)


def case_resume():
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
    ok = "Looking at files" in first and "Looking at files" in second.split("tutorial next")[-1]
    return ok, "after :quit and tutorial next: %r" % second[-300:]


def run_case(name):
    if name.startswith("selftest-"):
        return case_selftest(name[-2:])
    return {"hint-before": lambda: case_hint(True), "typed": case_typed, "menu": lambda: case_tab("menu"), "slash": lambda: case_tab("slash"),
            "lesson1": case_lesson1, "progress": case_progress, "resume": case_resume,
            "hint-after": lambda: case_hint(False)}[name]()


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
