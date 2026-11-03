"""KIT-WASABI-DOUBLE: the rig starts its own wasabid from a rig-only S:User-Startup block before the kit's blocks.

Install's `Status COM C:wasabid` misses the rig's BOOTX:wasabid, and an Amiga-side reboot (C:Reboot) never passes the
host-side strip of the kit's wasabi block, so two daemons could run (wasabid.c: two daemons patching the same exec
vectors guru the machine). The rig daemon now starts from a block placed before the kit's blocks, so it wins the race;
wasabid itself refuses a second non-trial instance, which makes the go-script start and the kit's start harmless.
"""
import pathlib, shutil, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig
import user_startup

XDF = shutil.which("xdftool") or str(pathlib.Path.home() / ".local/bin/xdftool")
KIT = (b"; user lines\n"
       b";BEGIN UP-Term assign\nAssign UP-Term: Work:UP-Term\n;END UP-Term assign\n"
       b";BEGIN UP-Term wasabi\nRun >NIL: C:wasabid\n;END UP-Term wasabi\n"
       b";BEGIN UP-Term Amarchy\nRun >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\n")


class WasabidBlock(unittest.TestCase):
    def test_block_sits_before_the_first_kit_block_once(self):
        out = rig.user_startup_with_rig_blocks(KIT).decode("latin-1")
        self.assertIn(";BEGIN rig-wasabid\nExecute BOOTX:wasabid-up\n;END rig-wasabid\n", out)
        self.assertLess(out.index(";BEGIN rig-wasabid"), out.index(";BEGIN UP-Term assign"))
        self.assertLess(out.index(";BEGIN rig-wasabid"), out.index("Run >NIL: C:wasabid"))
        self.assertEqual(rig.user_startup_with_rig_blocks(out.encode("latin-1")).decode("latin-1"), out)   # once

    def test_no_kit_block_the_block_is_appended(self):
        out = rig.user_startup_with_rig_blocks(b"; plain")
        self.assertIn(b";END rig-wasabid\n;BEGIN rig-solidwin", out)   # the SolidWin block follows it (RIG-SOLIDWIN)
        self.assertIn(b"; plain\n;BEGIN rig-wasabid", out)

    def test_the_install_check_filter_ignores_both_rig_blocks(self):
        self.assertEqual(user_startup.without_rig_blocks(rig.user_startup_with_rig_blocks(KIT)).decode("latin-1"),
                         rig.user_startup_with_shadows_block(KIT).decode("latin-1").replace(
                             ";BEGIN rig-upterm-shadows\nExecute BOOTX:shadows-up\n;END rig-upterm-shadows\n", ""))
        self.assertEqual(user_startup.without_rig_blocks(rig.user_startup_with_rig_blocks(KIT)), KIT)

    def test_the_early_script_and_go_start_the_same_daemon(self):
        self.assertIn(rig.WASABID_START, rig.WASABID_UP_SCRIPT)
        self.assertIn(rig.WASABID_START, rig.GO)       # the fallback: it exits "already running" when one runs
        self.assertLess(rig.WASABID_START.index("SetEnv wasabi.key"), rig.WASABID_START.index("Run >NIL:"))

    def test_start_writes_the_script(self):
        body = pathlib.Path(rig.__file__).read_text()
        self.assertIn('(RIG / "boot/wasabid-up").write_text(WASABID_UP_SCRIPT)', body)

    @unittest.skipUnless(pathlib.Path(XDF).exists(), "needs amitools' xdftool")
    def test_the_clone_gets_the_block_after_the_kit_strip(self):
        saved = rig.RIG
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            image = d / "sys.hdf"
            subprocess.run([XDF, str(image), "create", "size=1M", "+", "format", "System", "ffs"], check=True, capture_output=True)
            subprocess.run([XDF, str(image), "makedir", "S"], check=True, capture_output=True)
            (d / "us").write_bytes(KIT)
            subprocess.run([XDF, str(image), "write", str(d / "us"), "S/User-Startup"], check=True, capture_output=True)
            try:
                rig.RIG = d
                rig.sys_hdf_no_wasabid(image)
                rig.sys_hdf_shadows_block(image)
            finally:
                rig.RIG = saved
            subprocess.run([XDF, str(image), "read", "S/User-Startup", str(d / "out")], check=True, capture_output=True)
            out = (d / "out").read_text("latin-1")
        self.assertIn("Execute BOOTX:wasabid-up", out)
        self.assertNotIn("C:wasabid", out)                 # the kit's own start is stripped on the host path
        self.assertLess(out.index("Execute BOOTX:wasabid-up"), out.index(";BEGIN UP-Term assign"))


if __name__ == "__main__":
    unittest.main()
