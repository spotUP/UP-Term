#!/usr/bin/env python3
"""ami.py on the command line refuses to run when neither AMI_HOST nor UPTERM_RIG names the machine (it would silently drive
local rig 1): exit 2 and a one-line message naming both variables. With either set it does not refuse (an unknown command
then reaches main() and exits with the usage, no connection made). Importing ami keeps the old default."""
import os, pathlib, subprocess, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
AMI = str(ROOT / "tools/rig/ami.py")


def run(**extra):
    env = {k: v for k, v in os.environ.items() if k not in ('AMI_HOST', 'UPTERM_RIG')}
    env.update(extra)
    return subprocess.run([sys.executable, AMI, 'bogus-no-network-command'], env=env, capture_output=True, text=True, timeout=60)


class AmiTarget(unittest.TestCase):
    def test_refuses_without_target(self):
        r = run()
        self.assertEqual(r.returncode, 2)
        self.assertIn('UPTERM_RIG', r.stderr)
        self.assertIn('AMI_HOST', r.stderr)
        self.assertEqual(len(r.stderr.strip().splitlines()), 1)

    def test_rig_or_host_is_a_choice(self):
        for extra in ({'UPTERM_RIG': '7'}, {'AMI_HOST': '192.168.0.74'}):
            r = run(**extra)
            self.assertNotIn('set UPTERM_RIG or AMI_HOST', r.stderr)
            self.assertNotEqual(r.returncode, 2)   # main() exits through SystemExit(__doc__) = 1


if __name__ == '__main__':
    unittest.main()
