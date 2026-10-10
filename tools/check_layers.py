#!/usr/bin/env python3
"""amiga-pi's layering rule (plan A4): UI -> agent -> LLM -> HTTP -> TLS -> bsdsocket.

Greps #include lines per directory against the rule in amiga-pi's
ARCHITECTURE.md and prints each offending file:line.  Exit 0 = clean,
1 = violation.  The shared layers live in vtcon (llm/, claude/; decision D9),
the program in the amiga-pi repo (UPTERM_ROOT/amiga-pi, skipped when absent).

  check_layers.py [--vtcon DIR] [--pi DIR] [--strict]

claude/http.c must name no provider.  Until plan item B5 moves the Anthropic
head into llm/be_anthropic.c it does, so those lines are reported as PENDING
and fail the run only with --strict.
"""
import argparse
import os
import re
import sys

INC = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
AMIGA_DIRS = ("proto/", "dos/", "exec/", "clib/", "intuition/", "devices/", "utility/")
VTCON = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# (name, repo ("vtcon" or "pi"), directory, filename regex, forbidden include test, message)
RULES = [
    ("llm-no-ui", "vtcon", "llm", r".*\.[ch]$",
     lambda h: os.path.basename(h) in ("ui.h", "tui.h", "sys.h"),
     "llm/ includes no ui.h/tui.h/sys.h"),
    ("backend-no-amigados", "vtcon", "llm", r"be_.*\.[ch]$",
     lambda h: os.path.basename(h) == "sys.h" or h.startswith(AMIGA_DIRS),
     "llm/be_*.c include no AmigaDOS headers or sys.h"),
    ("ui-no-transport", "pi", "amiga-pi", r"(ui|tui)[_a-z0-9]*\.[ch]$",
     lambda h: os.path.basename(h) in ("net.h", "tls.h"),
     "amiga-pi/ UI files include no net.h/tls.h"),
]
PROVIDER = re.compile(r"anthropic|openai|x-api-key|api\.google|bedrock|azure", re.I)


def scan(roots):
    bad = []
    for name, repo, d, rx, test, msg in RULES:
        root = roots.get(repo)
        base = os.path.join(root, d) if root else ""
        if not root or not os.path.isdir(base):
            continue
        for fn in sorted(os.listdir(base)):
            if not re.match(rx, fn):
                continue
            path = os.path.join(base, fn)
            with open(path, errors="replace") as f:
                for n, line in enumerate(f, 1):
                    m = INC.match(line)
                    if m and test(m.group(1)):
                        bad.append((name, "%s:%d" % (os.path.relpath(path, root), n), msg))
    return bad


def scan_http(vtcon):
    path = os.path.join(vtcon, "claude", "http.c")
    out = []
    if os.path.isfile(path):
        with open(path, errors="replace") as f:
            for n, line in enumerate(f, 1):
                if PROVIDER.search(line):
                    out.append(("http-no-provider", "claude/http.c:%d" % n,
                                "claude/http.c contains no provider name"))
    return out


def main():
    upterm = os.environ.get("UPTERM_ROOT") or os.path.dirname(VTCON)
    ap = argparse.ArgumentParser()
    ap.add_argument("--vtcon", default=VTCON)
    ap.add_argument("--pi", default=os.path.join(upterm, "amiga-pi"))
    ap.add_argument("--strict", action="store_true")
    a = ap.parse_args()
    bad = scan({"vtcon": a.vtcon, "pi": a.pi})
    for name, where, msg in bad:
        print("VIOLATION %s %s: %s" % (name, where, msg))
    pend = scan_http(a.vtcon)
    for name, where, msg in pend:
        print("%s %s %s: %s" % ("VIOLATION" if a.strict else "PENDING(B5)", name, where, msg))
    if bad or (a.strict and pend):
        return 1
    print("check_layers: ok (%d rules, %d pending hit(s) in claude/http.c)" % (len(RULES), len(pend)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
