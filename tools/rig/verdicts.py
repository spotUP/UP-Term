"""Resumable verdicts for the rig scripts (RULES section 6a): one place for the
fingerprint, the verdict files, the check counter and the choice of which cases run.

A rig script owns a `Store` (a drawer of <case>.json files) and a fingerprint
function; `select()` says which cases run, `Store.record()` writes each verdict
the moment its case ends. Used by install_rig.py, installer_rig.py, assign_rig.py.
Pure Python, no emulator: tests/test_rig_verdicts.py runs it on the host."""
import hashlib, json, pathlib, time


def fingerprint(*chunks):
    """16 hex digits over the given bytes; a pathlib.Path contributes its content
    (or the words 'missing <path>' when it is not there, so a missing file changes it)."""
    h = hashlib.sha256()
    for c in chunks:
        if isinstance(c, pathlib.Path):
            h.update(c.read_bytes() if c.exists() else b"missing " + str(c).encode())
        else:
            h.update(c)
    return h.hexdigest()[:16]


class Store:
    """The verdict files of one rig script: <out>/<case>.json."""

    def __init__(self, out):
        self.out = pathlib.Path(out)

    def verdict(self, case):
        f = self.out / (case + ".json")
        return json.loads(f.read_text()) if f.exists() else None

    def record(self, case, ok, detail, fp, width=400):
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / (case + ".json")).write_text(json.dumps(
            {"case": case, "verdict": "pass" if ok else "fail", "fp": fp, "detail": detail, "time": time.time()}))
        print("[%s] %s %s" % ("PASS" if ok else "FAIL", case, ("" if ok else detail[-width:])))

    def failed(self, cases):
        return [c for c in cases if (self.verdict(c) or {}).get("verdict") == "fail"]


def select(store, cases, fp_of, only=(), failed=False, force=False):
    """[(case, why)] for the cases to run, in the order of `cases`, and [(case, why)] skipped.
    fp_of(case) is the fingerprint of what the case installs.
      only    exactly these cases, run even when they passed (an unknown name is a SystemExit)
      failed  of the candidates, only those whose last verdict is a fail
      force   every candidate, from zero
    Without any of them: the cases not yet passed with the current fingerprint."""
    for c in only:
        if c not in cases:
            raise SystemExit("cases: " + " ".join(cases))
    run, skip = [], []
    for case in cases:
        v = store.verdict(case)
        if only:
            if case not in only:
                continue
            why = "named"
        elif failed:
            if not (v and v["verdict"] == "fail"):
                skip.append((case, "did not fail last time"))
                continue
            why = "failed last time"
        elif force:
            why = "forced"
        elif v and v["verdict"] == "pass" and v["fp"] == fp_of(case):
            skip.append((case, "passed with this kit"))
            continue
        else:
            why = "no pass recorded for this kit" if v else "never run"
        if failed and only and not (v and v["verdict"] == "fail"):
            skip.append((case, "did not fail last time"))
            continue
        run.append((case, why))
    return run, skip


class Checks:
    """ok/FAIL lines with a count; `failed` holds the reasons for a verdict's detail."""

    def __init__(self):
        self.failed, self.total = [], 0

    def __call__(self, ok, what, seen=''):
        self.total += 1
        if not ok:
            self.failed.append('%s: %s' % (what, str(seen).strip()[-200:]))
        print('%s %d %s%s' % ('ok' if ok else 'FAIL', self.total, what,
                              (': ' + str(seen).strip()) if seen and not ok else ''))
        return bool(ok)

    @property
    def passed(self):
        return self.total - len(self.failed)
