#!/usr/bin/env python3
"""tools/rig/amimcp.py: start the amimcp MCP server (build/amimcp, `make amimcp`)
against a rig's amiagent, or a real Amiga.

Target rule is ami.py's: AMI_HOST (default 127.0.0.1), AMI_PORT (default the
rig's port from paths.py: 7846 + UPTERM_RIG - 1), AMI_TOKEN (default rigtoken).
amimcp itself reads AMIGA_HOST / AMIGA_PORT / AMIGA_TOKEN; this maps them.

  amimcp.py            exec the server on stdio
  amimcp.py --print    print "host port" and exit (the test)
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import paths  # the one reader of UPTERM_RIG

SERVER = paths.VTCON / "build" / "amimcp" / "server" / "amimcp.py"


def target(env):
    return (env.get("AMI_HOST") or "127.0.0.1",
            int(env.get("AMI_PORT") or paths.AGENT_PORT),
            env.get("AMI_TOKEN") or "rigtoken")


def main():
    host, port, token = target(os.environ)
    if "--print" in sys.argv:
        print(host, port)
        return 0
    if not SERVER.exists():
        sys.stderr.write("[ERROR] amimcp is not built: run `make amimcp` in %s\n" % paths.VTCON)
        return 1
    os.environ.update(AMIGA_HOST=host, AMIGA_PORT=str(port), AMIGA_TOKEN=token)
    os.execv(sys.executable, [sys.executable, str(SERVER)])


if __name__ == "__main__":
    sys.exit(main())
