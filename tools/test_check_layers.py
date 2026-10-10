#!/usr/bin/env python3
"""tools/check_layers.py exits 0 on clean trees and 1 on a planted include
(llm/ in the vtcon tree, amiga-pi/ in the amiga-pi tree), and fails under
--strict when claude/http.c names a provider."""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CHK = os.path.join(HERE, "check_layers.py")


def run(vt, pi, *extra):
    return subprocess.run([sys.executable, "-I", CHK, "--vtcon", vt, "--pi", pi] + list(extra),
                          capture_output=True, text=True)


def put(path, text, mode="w"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, mode) as f:
        f.write(text)


def main():
    with tempfile.TemporaryDirectory() as t:
        vt, pi = os.path.join(t, "vtcon"), os.path.join(t, "amiga-pi")
        put(os.path.join(vt, "llm", "be_x.c"), '#include "llm.h"\n#include <string.h>\n')
        put(os.path.join(vt, "claude", "http.c"), '/* no provider here */\n')
        put(os.path.join(pi, "amiga-pi", "ui_x.c"), '#include "agent.h"\n')
        r = run(vt, pi, "--strict")
        assert r.returncode == 0, "clean trees must pass: " + r.stdout
        put(os.path.join(vt, "llm", "be_x.c"), '#include <proto/dos.h>\n', "a")
        r = run(vt, pi)
        assert r.returncode == 1 and "llm/be_x.c:3" in r.stdout, "planted AmigaDOS include: " + r.stdout
        put(os.path.join(pi, "amiga-pi", "ui_x.c"), '#include "../claude/net.h"\n', "a")
        r = run(vt, pi)
        assert "amiga-pi/ui_x.c:2" in r.stdout, "planted net.h include: " + r.stdout
        put(os.path.join(vt, "llm", "be_x.c"), '#include "llm.h"\n')
        put(os.path.join(pi, "amiga-pi", "ui_x.c"), '#include "agent.h"\n')
        put(os.path.join(vt, "claude", "http.c"), 'put(&o, "x-api-key: ");\n')
        r = run(vt, pi)
        assert r.returncode == 0 and "PENDING(B5)" in r.stdout, "provider name, not strict: " + r.stdout
        r = run(vt, pi, "--strict")
        assert r.returncode == 1 and "claude/http.c:1" in r.stdout, "provider name, strict: " + r.stdout
        r = run(vt, os.path.join(t, "absent"))
        assert r.returncode == 0, "no amiga-pi checkout: its rule is skipped: " + r.stdout
    print("test_check_layers: ok")


main()
