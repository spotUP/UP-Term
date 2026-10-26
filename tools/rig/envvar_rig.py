#!/usr/bin/env python3
"""envvar_rig.py -- vsh's $name reads ENV: variables (shell/sh_exec.c env_refresh, os.getenv).

A script file run by vsh (not vsh -c through ami.py: the outer AmigaShell would expand $name first and prove
nothing). A variable set with SetEnv before vsh starts, a system variable nobody set by hand (Kickstart), and a
SetEnv done during the run must all show in `echo "[$name]"`; a plain assignment afterwards wins.

Needs the rig up (UPTERM_RIG=n tools/rig/rig.py start --max --purpose ...) and build/amiga/vsh built.
  envvar_rig.py        copies build/amiga/vsh to VTC:vsh, runs the script, prints the output lines, exit 1 on a mismatch
Not run by `make test`: it needs the emulator."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import install_rig as ir

SCRIPT = """echo "[$pre1]"
echo "[$Kickstart]"
SetEnv mid1 x
echo "[$mid1]"
pre1=local
echo "[$pre1]"
"""
WANT = ["[fromshell]", "[x]", "[local]"]


def main():
    src = os.environ.get("ENVVAR_VSH")   # another vsh build to compare (the one before a fix)
    if src:
        import shutil, rig
        shutil.copyfile(src, ir.VTC / "vsh")
        rig.refresh_kit_shadows(ir.VTC)
    else:
        ir.put_vsh()
    (ir.VTC / "envvar.sh").write_text(SCRIPT)
    (ir.VTC / "envvar-run").write_text('SetEnv pre1 fromshell\nVTC:vsh VTC:envvar.sh >RAM:envvar.out\nType RAM:envvar.out\n')
    rc, out = ir.run('Execute VTC:envvar-run', 120)
    print(out)
    lines = out.split()
    ks = lines[1] if len(lines) > 1 else ""
    ok = lines[0:1] == [WANT[0]] and ks not in ("[]", "") and lines[2:3] == [WANT[1]] and lines[3:4] == [WANT[2]]
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
