"""The Amiberry backend: amiberry.config_lines spells the rig's machine in Amiberry keys, and rig.py's own machine
description (rig.amiberry_machine) feeds it the same choices the FS-UAE config gets (060, MMU, 020, A500, stock,
--os tree, Work drive). One end-to-end check drives rig.setup() and reads the written .uae file."""
import os, pathlib, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import amiberry

BASE = dict(model="A1200", cpu="68060", chip_kb=2048, fast_mb=8, z3_mb=1024, rtg=True, fpu="68060", mmu=None,
            speed="max", compatible=False, cycle_exact=False, jit=True, no_unimplemented=True, rom="/r/k.rom",
            bsdsocket=True, volume=100, clipboard=True, window=(800, 600), title="t",
            drives=[dict(path="/d/os-32", hdf=False, label="Empty", ro=False, pri=10),
                    dict(path="/d/vtc", label="VTCX"), dict(path="/d/boot", label="BOOTX", pri=0),
                    dict(path="/d/work", label="Work", pri=-128)])


def cfg(**kw):
    return dict(l.split("=", 1) for l in amiberry.config_lines(dict(BASE, **kw)))


class Config(unittest.TestCase):
    def test_060_max(self):
        c = cfg()
        self.assertEqual((c["cpu_model"], c["fpu_model"], c["cpu_speed"], c["z3mem_size"], c["chipmem_size"]),
                         ("68060", "68060", "max", "1024", "4"))
        self.assertNotIn("mmu_model", c)
        self.assertEqual((c["cachesize"], c["bsdsocket_emu"], c["gfxcard_type"]), ("8192", "true", "ZorroIII"))

    def test_mmu_turns_jit_off(self):
        c = cfg(mmu="68060", jit=False)
        self.assertEqual((c["mmu_model"], c["cachesize"]), ("68060", "0"))

    def test_a500(self):
        c = cfg(model="A500", cpu="68000", chip_kb=512, fpu=None, rtg=False, z3_mb=0, speed="real", cycle_exact=True,
                jit=False, no_unimplemented=False)
        self.assertEqual((c["chipset"], c["cpu_model"], c["chipmem_size"], c["cpu_cycle_exact"], c["cpu_24bit_addressing"]),
                         ("ocs", "68000", "1", "true", "true"))
        self.assertNotIn("fpu_model", c)
        self.assertNotIn("gfxcard_type", c)

    def test_drives(self):
        c = cfg()
        self.assertEqual(c["uaehf0"], "dir,rw,DH0:Empty:/d/os-32,10")
        self.assertEqual(c["uaehf1"], "dir,rw,DH1:VTCX:/d/vtc,0")
        self.assertEqual(c["uaehf3"], "dir,rw,DH3:Work:/d/work,-128")
        self.assertTrue(amiberry.drive_line(0, dict(path="/s.hdf", hdf=True)).startswith("uaehf0=hdf,rw,DH0:/s.hdf"))

    def test_mute_and_title(self):
        c = cfg(volume=0, title="vtcon rig 20 - Amarchy s29: x")
        self.assertEqual((c["sound_output"], c["sound_volume"]), ("none", "100"))
        self.assertEqual(c["config_description"], "vtcon rig 20 - Amarchy s29: x")

    def test_command(self):
        self.assertEqual(amiberry.command("/c.uae")[1:], ["-G", "--log", "-f", "/c.uae"])


class MachineFromFlags(unittest.TestCase):
    def machine(self, *flags):
        """rig.amiberry_machine() in a fresh interpreter started with these flags (rig.py reads sys.argv at import);
        rig 29 is only named, nothing is written under build/."""
        code = ("import sys, json; sys.path.insert(0, %r); sys.argv = ['rig.py', 'status'] + %r; import rig;"
                "print(json.dumps(rig.amiberry_machine()))" % (str(ROOT / "tools/rig"), list(flags)))
        out = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True,
                             env=dict(os.environ, UPTERM_RIG="29"))
        self.assertEqual(out.returncode, 0, out.stderr[-400:])
        import json
        return json.loads(out.stdout.strip().splitlines()[-1])

    def test_max_mmu_purpose(self):
        m = self.machine("--max", "--mmu", "--emu", "amiberry", "--purpose", "Amarchy s29: probe")
        self.assertEqual((m["cpu"], m["mmu"], m["jit"], m["z3_mb"]), ("68060", "68060", False, 1024))
        self.assertIn("Amarchy s29: probe", m["title"])
        c = dict(l.split("=", 1) for l in amiberry.config_lines(dict(m, window=tuple(m["window"]))))
        self.assertIn("Amarchy s29: probe", c["config_description"])

    def test_a1200_is_020(self):
        m = self.machine("--a1200", "--emu", "amiberry")
        self.assertEqual((m["cpu"], m["fpu"], m["speed"]), ("68020", "68882", "real"))

    def test_a500(self):
        m = self.machine("--a500", "--emu", "amiberry")
        self.assertEqual((m["model"], m["cpu"], m["cycle_exact"]), ("A500", "68000", True))


if __name__ == "__main__":
    unittest.main()
