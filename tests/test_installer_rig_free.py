"""installer_rig.sys_free_bytes reads SYS: free space whether Info prints the device as DH0: (3.1) or DH0 (3.2)."""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as rig

HEAD = "Mounted disks:\nUnit      Size    Used    Free Full Errs   Status   Name\n"


def free_with(device):
    out = HEAD + "DH1:     100M   10000   90000  10%   0  Read/Write  Work\n" + \
          "%-9s 200M   20000   40000  33%%   0  Read/Write  System\n" % device
    with mock.patch.object(rig.ir, "run", lambda cmd: (0, out)):
        return rig.sys_free_bytes()


class SysFree(unittest.TestCase):
    def test_info_line_with_dh0_and_no_colon_gives_sys_free_bytes(self):
        self.assertEqual(free_with("DH0"), 40000 * 512)

    def test_info_line_with_dh0_colon_gives_sys_free_bytes(self):
        self.assertEqual(free_with("DH0:"), 40000 * 512)

    def test_unrelated_volume_is_ignored(self):
        with mock.patch.object(rig.ir, "run", lambda cmd: (0, HEAD + "DH1  100M 1 90000 10% 0 Read/Write Work\n")):
            with self.assertRaises(SystemExit):
                rig.sys_free_bytes()


if __name__ == "__main__":
    unittest.main()
