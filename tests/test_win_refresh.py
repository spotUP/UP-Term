#!/usr/bin/env python3
"""PATINA-WIN-REFRESH-UAF: compiles patina/win_refresh.c with the fake Intuition of tests/win_refresh_host against
tests/test_win_refresh.c and runs it. Fails when win_refresh_frames() hands RefreshWindowFrame a window another task closed in
the gap after the IBase lock was dropped, or when own / idle-Workbench windows are not refreshed."""
import os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def boot_frames_after_pens():
    """FLAT-BOOT-FRAMES: patina-flat must refresh the pre-hook frames only after pens_run obtained the Workbench pens; a refresh
    from install_all drew the DOpus boot lister's zoom/depth with the own pens still 0 (all black)."""
    src = open(os.path.join(ROOT, 'patina', 'flat.c')).read()
    body = src[src.index('static int install_all('):src.index('int main(void)')]
    main_src = src[src.index('int main(void)'):]
    if 'win_refresh_all_frames' in body or main_src.find('win_refresh_all_frames') < main_src.find('dim_refresh(r);'):
        print('[ERROR] flat.c: win_refresh_all_frames runs before the pens are obtained')
        return 1
    return 0


def main():
    if boot_frames_after_pens():
        return 1
    cc = os.environ.get('CC', 'cc')
    with tempfile.TemporaryDirectory() as d:
        exe = os.path.join(d, 'twr')
        r = subprocess.run([cc, '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined',
                            '-I', os.path.join(HERE, 'win_refresh_host'), '-o', exe,
                            os.path.join(HERE, 'test_win_refresh.c'), os.path.join(ROOT, 'patina', 'win_refresh.c')],
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
