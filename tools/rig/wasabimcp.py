#!/usr/bin/env python3
"""tools/rig/wasabimcp.py: start wasabi's MCP server (wasabi_mcp.py, `make wasabi` pins it into build/wasabi)
against a rig's wasabid, or a real Amiga. Replaces amimcp.py.

Target rule: rig N listens on 1234 + N - 1 with the rig key (wasabi.port(), wasabi.KEY: the one place that
knows them); UPTERM_RIG picks N (paths.py), rig 1 by default. Overrides: AMI_HOST (default 127.0.0.1), AMI_PORT,
AMI_TOKEN (the wasabid key); WASABI_HOST / WASABI_PORT / WASABI_KEY win over those, as they do for wasabi itself.
The server reads WASABI_HOST / WASABI_PORT / WASABI_KEY; this sets them.

  wasabimcp.py            exec the server on stdio
  wasabimcp.py --print    print "host port" and exit (the test)
"""
import os
import pathlib
import sys

ENV = dict(os.environ)     # before paths.py, which sets AMI_PORT to the amiagent port
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import paths    # the one reader of UPTERM_RIG
import wasabi   # port() and KEY of a rig's wasabid


def target(env):
    return (env.get("WASABI_HOST") or env.get("AMI_HOST") or "127.0.0.1",
            int(env.get("WASABI_PORT") or env.get("AMI_PORT") or wasabi.port()),
            env.get("WASABI_KEY") or env.get("AMI_TOKEN") or wasabi.KEY)


def server():
    for d in (paths.VTCON / "build" / "wasabi", paths.RIG_HOME / "wasabi"):
        for s in (d / "wasabi_mcp.py", d / "src" / "wasabi_mcp.py"):
            if s.is_file() and (s.parent / "wasabi").is_file():
                return s
    return None


def main():
    host, port, key = target(ENV)
    if "--print" in sys.argv:
        print(host, port)
        return 0
    srv = server()
    if srv is None:
        sys.stderr.write("[ERROR] wasabi is not built: run `make wasabi` in %s\n" % paths.VTCON)
        return 1
    if not (ENV.get("WASABI_HOST") or ENV.get("AMI_HOST")):   # a local rig: VTC: current before the session starts
        import rig
        rig.ensure_current()
    os.environ.update(WASABI_HOST=host, WASABI_PORT=str(port), WASABI_KEY=key)
    os.execv(sys.executable, [sys.executable, str(srv)])


if __name__ == "__main__":
    sys.exit(main())
