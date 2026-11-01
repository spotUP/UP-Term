"""ensure_current: the one gate that brings a rig's VTC: to the build before any command goes to the rig (owner
2026-10-29: "we have lost a lot of time testing with stale binaries"). Before: only start/install refreshed, so a command
to a running rig ran against whatever VTC: held. Now rig.py's commands and every ami.req pass the gate."""
import io, pathlib, shutil, sys, tempfile, unittest
from contextlib import redirect_stdout
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import paths
import rig
import ami


class Base(unittest.TestCase):
    def setUp(self):
        self.d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        self.addCleanup(setattr, rig, "ROOT", rig.ROOT)
        rig.ROOT = self.d
        rig.GATE_STATE.update(busy=False, sig=None)
        (self.d / "build/amiga").mkdir(parents=True)
        self.vtc = self.d / "rig/vtc"
        self.bin = self.vtc / paths.UPTERM_DRAWER / "bin"
        self.bin.mkdir(parents=True)
        self.names = ("vsh", "hl", "ixnet.library")
        for n in self.names:
            (self.d / "build/amiga" / n).write_bytes(b"new " + n.encode())
            (self.vtc / n).write_bytes(b"new " + n.encode())
            (self.bin / n).write_bytes(b"new " + n.encode())
        for p in ("userland_names", "mine"):
            self.addCleanup(setattr, rig, p, getattr(rig, p))
        rig.userland_names = lambda: []
        rig.mine = lambda: []

    def gate(self):
        out = io.StringIO()
        with redirect_stdout(out):
            done = rig.ensure_current(self.vtc)
        return done, out.getvalue()

    def stale(self, name):
        (self.vtc / name).write_bytes(b"old")


class Gate(Base):

    def test_stale_file_is_refreshed_before_the_command_runs(self):
        self.stale("hl")
        done, out = self.gate()
        self.assertEqual(done, ["hl"])
        self.assertEqual(out, "[REFRESH] rig %d: 1 file(s): hl\n" % paths.RIG_N)
        self.assertEqual(rig.build_drift(self.vtc), [])

    def test_stale_gg_sh_is_refreshed_with_vsh(self):
        # GG:bin/sh is vsh: the gate saw it drift but copied only VTC:vsh and Apps/bin/vsh (rig 10, s22)
        (self.vtc / "gg/bin").mkdir(parents=True)
        (self.vtc / "gg/bin/sh").write_bytes(b"old")
        done, out = self.gate()
        self.assertEqual(done, ["gg/bin/sh"])
        self.assertEqual((self.vtc / "gg/bin/sh").read_bytes(), b"new vsh")
        self.assertEqual(rig.build_drift(self.vtc), [])

    def test_no_drift_means_no_refresh_and_no_output(self):
        with mock.patch.object(rig, "refresh_programs") as refresh:
            done, out = self.gate()
        self.assertEqual((done, out), ([], ""))
        refresh.assert_not_called()

    def test_held_library_on_a_quiet_running_rig_asks_for_a_reboot(self):
        self.stale("ixnet.library")
        rig.mine = lambda: ["fs-uae"]
        with mock.patch.object(rig, "reboot") as reboot, mock.patch.object(paths, "long_run_active", return_value=False), \
                mock.patch.object(paths, "last_command", return_value=None):
            done, out = self.gate()
        reboot.assert_called_once_with([])
        self.assertIn("[REFRESH] rig %d: LIBS: changed, rebooting" % paths.RIG_N, out)

    def test_held_library_is_never_rebooted_under_a_running_command(self):
        self.stale("ixnet.library")
        rig.mine = lambda: ["fs-uae"]
        with mock.patch.object(rig, "reboot") as reboot, mock.patch.object(paths, "long_run_active", return_value=True):
            done, out = self.gate()
        reboot.assert_not_called()
        self.assertIn("[WARN]", out)

    def test_plain_program_does_not_reboot(self):
        self.stale("hl")
        rig.mine = lambda: ["fs-uae"]
        with mock.patch.object(rig, "reboot") as reboot:
            self.gate()
        reboot.assert_not_called()


class Reachable(Base):
    """ONE reachability test: the real entry points reach ensure_current, before the command."""
    def test_rig_main_and_ami_req_run_the_gate_before_the_command(self):
        order = []
        real = rig.ensure_current
        def gate_then_note(vtc=None):
            order.append("gate")
            return real(self.vtc)
        self.stale("hl")
        with mock.patch.object(rig, "ensure_current", gate_then_note), \
                mock.patch.object(rig, "dbg", lambda: order.append(("command", (self.vtc / "hl").read_bytes()))), \
                redirect_stdout(io.StringIO()):
            self.assertEqual(rig.main(["rig.py", "dbg"]), 0)
        self.assertEqual(order, ["gate", ("command", b"new hl")])
        # ami.req: every script, ami.py and wasabi.run pass it; a ping does not
        order.clear()
        self.stale("hl")
        def conn(*a, **k):
            order.append(("command", (self.vtc / "hl").read_bytes()))
            raise SystemExit("stub transport")
        with mock.patch.object(rig, "ensure_current", gate_then_note), mock.patch.object(ami, "_conn", conn), \
                mock.patch.object(ami, "_m"), mock.patch.object(paths, "cap_fsuae_log"), \
                mock.patch.dict("os.environ", {}, clear=False), redirect_stdout(io.StringIO()):
            sys.modules["rig"] = rig
            with self.assertRaises(SystemExit):
                ami.req(0x02, b"\0\x0aEcho x")
            self.assertEqual(order, ["gate", ("command", b"new hl")])
            order.clear()
            with self.assertRaises(SystemExit):
                ami.req(0x01)
            self.assertEqual(order[0][0], "command")   # no gate for a ping


if __name__ == "__main__":
    unittest.main()
