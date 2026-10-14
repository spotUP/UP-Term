"""tools/rig/amimcp.py picks the amiagent target from the environment: rig 1
(127.0.0.1:7846) by default, UPTERM_RIG=2 -> 7847, AMI_HOST/AMI_PORT override.
No network."""
import os
import pathlib
import subprocess
import sys

WRAP = pathlib.Path(__file__).resolve().parent / "rig" / "amimcp.py"
fails = 0


def run(**env):
    base = {k: v for k, v in os.environ.items()
            if k not in ("UPTERM_RIG", "AMI_HOST", "AMI_PORT", "AMI_TOKEN")}
    r = subprocess.run([sys.executable, "-I", str(WRAP), "--print"],
                       env={**base, **env}, capture_output=True, text=True)
    return r.stdout.strip()


def check(got, want, what):
    global fails
    if got != want:
        fails += 1
        print("FAIL: %s: got %r want %r" % (what, got, want))


check(run(), "127.0.0.1 7846", "default is rig 1")
check(run(UPTERM_RIG="2"), "127.0.0.1 7847", "UPTERM_RIG=2 is port 7847")
check(run(UPTERM_RIG="3"), "127.0.0.1 7848", "UPTERM_RIG=3 is port 7848")
check(run(AMI_HOST="10.0.0.5", AMI_PORT="9000"), "10.0.0.5 9000", "AMI_* override")
check(run(UPTERM_RIG="2", AMI_HOST="10.0.0.5"), "10.0.0.5 7847", "host alone keeps the rig port")
sys.exit(1 if fails else 0)
