"""tools/rig/piscript.py: a rig window script gives amiga-pi <NIL: where it feeds no input.

Regression (R7, from N1): the CON: window scripts of amigapi_rig.py started amiga-pi with no
input redirection; the Shell reads a script through its input stream, so a program that reads
stdin reads the rest of the script and the trailing commands never ran.
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "rig"))
import piscript  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


PI = "VTC:amiga-pi --model m -p \"a < b\" >VTC:pi2.out"
check(piscript.no_stdin(PI).endswith(">VTC:pi2.out <NIL:"), "amiga-pi without input gets <NIL:")
check(piscript.no_stdin("VTC:amiga-pi --mode rpc <VTC:pi14.in >VTC:pi14.out") ==
      "VTC:amiga-pi --mode rpc <VTC:pi14.in >VTC:pi14.out", "an amiga-pi fed from a file keeps its own <")
check(piscript.no_stdin("VTC:amiga-pi -p \"say *\"<*\" x\" >o").endswith("<NIL:"),
      "a < inside a quoted prompt (with *\" escapes) is not a redirection")
check(piscript.no_stdin("VTC:pi-llmprobe http://h/v1 m k hello >VTC:pi1.out").endswith(" <NIL:"),
      "the llm probe is guarded too")
check(piscript.no_stdin("Echo \"x\" >RAM:y") == "Echo \"x\" >RAM:y", "other commands are left alone")
check(piscript.no_stdin("VTC:vsh VTC:pi11.sh <NIL:") == "VTC:vsh VTC:pi11.sh <NIL:", "vsh lines are left alone")

text = piscript.script_text(["CD RAM:", "VTC:amiga-pi -p hello >VTC:pi2.out", "Echo \"rc=$RC\" >VTC:pi2.rc"], "pi2-done")
lines = text.splitlines()
check(lines[1] == "VTC:amiga-pi -p hello >VTC:pi2.out <NIL:", "script_text guards the amiga-pi line: %r" % lines[1])
check(lines[-3:] == ['Echo >RAM:pi2-done "done"', "Wait 3", "EndCLI"], "script_text closes the script")

# reachability: the rig module writes its window scripts through script_text
src = (HERE / "rig" / "amigapi_rig.py").read_text()
check("import piscript" in src and src.count("piscript.script_text(") >= 3,
      "amigapi_rig.py writes run_window, run_watched and step 1 scripts through piscript.script_text")
check("'Echo >RAM:%s \"done\"" not in src, "no window script of amigapi_rig.py is closed by hand")

if fails:
    sys.exit(1)
print("rig_stdin: ok")
