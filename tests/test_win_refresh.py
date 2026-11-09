#!/usr/bin/env python3
"""AMARCHY-WIN-REFRESH-UAF: compiles amarchy/win_refresh.c with the fake Intuition of tests/win_refresh_host against
tests/test_win_refresh.c and runs it. Fails when win_refresh_frames() hands RefreshWindowFrame a window another task closed in
the gap after the IBase lock was dropped, or when own / idle-Workbench windows are not refreshed."""
import os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def main():
    cc = os.environ.get('CC', 'cc')
    with tempfile.TemporaryDirectory() as d:
        exe = os.path.join(d, 'twr')
        r = subprocess.run([cc, '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined',
                            '-I', os.path.join(HERE, 'win_refresh_host'), '-o', exe,
                            os.path.join(HERE, 'test_win_refresh.c'), os.path.join(ROOT, 'amarchy', 'win_refresh.c')],
                           capture_output=True, text=True)
        if r.returncode:
            print(r.stderr)
            return 1
        r = subprocess.run([exe], capture_output=True, text=True)
        print(r.stdout, end='')
        if r.returncode:
            print(r.stderr[-2000:])
        return r.returncode


if __name__ == '__main__':
    sys.exit(main())
