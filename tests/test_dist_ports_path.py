"""Every upterm-ports path in the Makefile follows UPTERM_PORTS.

2026-10-27: `UPTERM_PORTS=<worktree> make dist` stopped with "No rule to make target
.../upterm-ports/build/sysroot/UP-Term/lib/libupkgui.a, needed by build/amiga/amarchy-menu":
UPKGUI_SYS and the amarchy-menu compiler were spelled $(UPTERM_ROOT)/upterm-ports, so a ports
build in a worktree was ignored for them while the kit itself came from the worktree. Checked
through make itself: with UPTERM_PORTS pointing at a temporary tree that has the libraries and
the compiler, a dry run of amarchy-menu names that tree, and the main checkout never.
Since the libupkgui pin (tools/upkgui.rev) the library is archived out of that tree at the
pinned commit and linked from build/upkgui/<rev>.
"""
import os, pathlib, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class PortsPath(unittest.TestCase):
    def test_amarchy_menu_uses_upterm_ports(self):
        with tempfile.TemporaryDirectory() as tmp:
            ports = pathlib.Path(tmp) / 'ports-worktree'
            lib = ports / 'build/sysroot/UP-Term/lib'
            lib.mkdir(parents=True)
            for name in ('libupkgui.a', 'libncursesw.a', 'libixcompat.a'):
                (lib / name).write_bytes(b'')
            (ports / 'tools').mkdir()
            (ports / 'tools/m68k-cc').write_text('#!/bin/sh\n')
            env = dict(os.environ)
            env.pop('UPKGUI_SYS', None)
            r = subprocess.run(['make', '-n', '-B', '-C', str(ROOT), 'build/amiga/amarchy-menu',
                                'UPTERM_PORTS=%s' % ports], capture_output=True, text=True, env=env)
            out = r.stdout + r.stderr
            self.assertNotIn('No rule to make target', out)
            self.assertIn('%s/tools/m68k-cc' % ports, out)
            # libupkgui is built from the pinned commit (tools/upkgui.rev), archived out of
            # UPTERM_PORTS, so the archive must name the worktree and the link the pinned build.
            self.assertIn('git -C %s archive' % ports, out)
            self.assertIn('/build/upkgui/', out)
            main_ports = str(ROOT.parent / 'upterm-ports') + '/'
            for line in out.splitlines():
                if 'm68k-cc' in line and 'amarchy-menu' in line:
                    self.assertNotIn(main_ports, line)


if __name__ == '__main__':
    unittest.main()
