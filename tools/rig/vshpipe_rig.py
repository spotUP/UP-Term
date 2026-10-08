#!/usr/bin/env python3
"""vshpipe_rig.py -- vsh pipelines whose first stage is a builtin end on the rig.

A pipeline stage that is not an external command runs in a subshell, a
second vsh process whose shell is malloc memory. On 2026-10-07 `echo hello |
wc -c` hung there for ever (sh_shell_init left nclosed and wfail as garbage,
and the subshell's echo walked a garbage count of closed streams), while
the same line with an external first stage worked.

Each case is a vsh script run detached (`Run`), so a hang does not hold
amiagent; a case that has not ended after 20 s is a FAIL. The binary goes to
RAM:vshpipe/ (VTC: is not touched). The rig must be up; `make
build/amiga/vsh` first (VSH= names another binary). A hung case leaves its
processes running: restart the rig after a FAIL."""
import os, pathlib, shutil, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import paths

ROOT = pathlib.Path(__file__).resolve().parents[2]
DIR = "RAM:vshpipe"
# wc and cat are the kit's coreutils (dist/gg), staged into this rig's VTC:userland/bin
# (the same drawer userland_rig puts on PATH): a rig's system disk has neither on PATH,
# and a stage that is "not found" ends the Execute script at once (rc 127 > FailAt)
COREUTILS = ROOT / "dist/gg/coreutils-5.2.1/bin"
STAGED = ("wc", "cat")
PATH_LINE = "export PATH=/VTC/userland/bin:$PATH\n"

# (name, vsh script, the output it must give). The redirect_* cases are a builtin or compound
# LAST stage: it keeps its own stdout redirect and the pipeline's stdout (os_spawn once opened
# the console "*" for it, as for a background job).
CASES = (
    ("echo_wc", "echo hello | wc -c\n", "6\n"),
    ("printf_wc", "printf 'a\\nb\\n' | wc -l\n", "2\n"),
    ("func_stage", "f() { echo one; echo two; }\nf | wc -l\n", "2\n"),
    ("three_stages", "echo abc | cat | wc -c\n", "4\n"),
    ("redirect_last_group", "echo a | { read x; echo got $x; } >RAM:vshpipe/rl.txt\ncat RAM:vshpipe/rl.txt\n",
     "gota\n"),
    ("last_group_plain", "echo a | { read x; echo got $x; }\n", "gota\n"),
    ("redirect_last_inner", "{ echo c | { read x; echo got $x; }; } >RAM:vshpipe/rl2.txt\ncat RAM:vshpipe/rl2.txt\n",
     "gotc\n"),
)


def put_text(text, path):
    with tempfile.NamedTemporaryFile("w", delete=False) as t:
        t.write(text)
    try:
        ami.put(t.name, path)
    finally:
        os.unlink(t.name)


def ex(cmd, secs=20):
    return ami.req(0x02, (secs).to_bytes(2, "big") + cmd.encode("latin-1"))


def run_case(name, script, want):
    base = "%s/%s" % (DIR, name)
    ex('Delete >NIL: "%s.out" "%s.done" QUIET' % (base, base))
    put_text(PATH_LINE + script, base + ".sh")
    put_text("FailAt 21\n%s/vsh %s.sh >%s.out\nEcho >%s.done $RC\n" % (DIR, base, base, base), base + ".run")
    ex("Run >NIL: <NIL: Execute %s.run" % base)
    end = time.time() + 20
    while time.time() < end:
        if ami.read_file(base + ".done") is not None:
            out = (ami.read_file(base + ".out") or b"").decode("latin-1").replace(" ", "")
            return out == want, out
        time.sleep(1)
    return False, "hung (no end after 20 s)"


def main():
    vsh = pathlib.Path(os.environ.get("VSH") or ROOT / "build/amiga/vsh")
    ex("MakeDir >NIL: %s" % DIR)
    bindir = paths.RIG / "vtc/userland/bin"
    bindir.mkdir(parents=True, exist_ok=True)
    for tool in STAGED:
        shutil.copyfile(COREUTILS / tool, bindir / tool)
    ami.put(str(vsh), DIR + "/vsh")
    ok = True
    for name, script, want in CASES:
        good, out = run_case(name, script, want)
        ok = ok and good
        print("[%s] %s: %r" % ("OK" if good else "FAIL", name, out))
    print("vshpipe_rig: %s" % ("OK" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
