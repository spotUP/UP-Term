#!/usr/bin/env python3
"""Files written into a rig's DH0: image survive its removal (RIG-GC-SAVE-USER-FILES part 2, RULES.md rule 7). A rig's
default DH0: is build/rig<n>/sys.hdf, an APFS clone of the master image; part 1 saved only the drive directories, so a
file the user made on DH0: (an ENVARC: prefs file, an edited S:Startup-Sequence) went with the folder. Now the image is
unpacked with xdftool, compared by content against the master's cached digests, and every new or changed file is copied
to build/rig-saved/<n>/<date-time>/sys/; an image that cannot be unpacked keeps the folder. Small RDB images made here
with rdbtool and xdftool (the real sys.hdf is an RDB disk with one partition, DH0); temp dir, no real rig touched."""
import contextlib, io, pathlib, shutil, subprocess, sys, tempfile, unittest, unittest.mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

NOW = 1_000_000_000.0
HOUR = 3600


def tool(name):
    return shutil.which(name) or str(pathlib.Path.home() / ".local/bin" / name)


def make_image(path, files):
    """A 4 MiB RDB hard disk file with one FFS partition DH0 (volume System) holding files {amiga path: bytes}."""
    path = pathlib.Path(path)
    subprocess.run([tool("rdbtool"), str(path), "create", "size=4Mi", "+", "init", "+", "add", "size=100%",
                    "name=DH0", "dostype=ffs"], check=True, capture_output=True)
    subprocess.run([tool("xdftool"), str(path), "open", "part=DH0", "+", "format", "System", "ffs"], check=True,
                   capture_output=True)
    write_into(path, files)


def write_into(image, files):
    """Write {amiga path: bytes} into the image's DH0: (drawers made, an existing file replaced), as the Amiga would."""
    image = pathlib.Path(image)
    with tempfile.TemporaryDirectory() as tmp:
        have = subprocess.run([tool("xdftool"), "-r", str(image), "list", "/", "all"], capture_output=True, text=True).stdout
        names = {l.split()[0] for l in have.splitlines()[1:] if l.split()}
        for i, (amiga, data) in enumerate(sorted(files.items())):
            host = pathlib.Path(tmp) / str(i)
            host.write_bytes(data)
            cmd = [tool("xdftool"), str(image), "open", "part=DH0"]
            parts = amiga.split("/")
            for k in range(1, len(parts)):
                if parts[k - 1] not in names:
                    cmd += ["+", "makedir", "/".join(parts[:k])]
                    names.add(parts[k - 1])
            if parts[-1] in names:
                cmd += ["+", "delete", amiga]
            cmd += ["+", "write", str(host), amiga]
            subprocess.run(cmd, check=True, capture_output=True)
            names.add(parts[-1])


MASTER_FILES = {"S/Startup-Sequence": b"kit startup\n", "C/Tool": b"kit tool " * 50,
                "Prefs/Env-Archive/Sys/kit.prefs": b"kit prefs"}


class SysHdfFilesSurvive(unittest.TestCase):
    def setUp(self):
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        self.out, self.err = io.StringIO(), io.StringIO()
        st = contextlib.ExitStack()
        self.addCleanup(st.close)
        st.enter_context(contextlib.redirect_stdout(self.out))
        st.enter_context(contextlib.redirect_stderr(self.err))
        self.master = self.home / "master.hdf"
        make_image(self.master, MASTER_FILES)
        self.unpacks = 0
        real = getattr(rig, "xdf_unpack", None)   # create=True: on a rig.py without it the save assertions fail, not setUp
        def counted(image, tmp):
            self.unpacks += 1
            return real(image, tmp)
        st.enter_context(unittest.mock.patch.object(rig, "xdf_unpack", counted, create=True))

    def make_rig(self, n):
        d = self.home / ("rig%d" % n)
        d.mkdir()
        shutil.copyfile(self.master, d / "sys.hdf")   # fresh_sys_hdf's clone
        (d / "last-command").write_text("%f\n" % (NOW - 5 * HOUR))
        # written on the rig: a new file in a new drawer, a kit file the user edited; C/Tool stays as cloned
        write_into(d / "sys.hdf", {"Prefs/Env-Archive/DCTelnet/DCTelnet.prefs": b"user dctelnet prefs\x00\x01",
                                   "S/Startup-Sequence": b"user startup\n"})
        return d

    def sweep(self):
        return rig.gc(home=self.home, own_n=9, lines=[], now=NOW, sources={"sys.hdf": self.master})

    def saved_files(self, n):
        base = self.home / "rig-saved" / str(n)
        saves = sorted(base.iterdir()) if base.is_dir() else []
        self.assertEqual(len(saves), 1, "no save folder under rig-saved/%d" % n)
        return {str(p.relative_to(saves[0])): p.read_bytes() for p in saves[0].rglob("*") if p.is_file()}

    def test_changed_and_new_dh0_files_saved_under_sys(self):
        self.make_rig(3)
        self.assertEqual(self.sweep(), ["rig3"])
        self.assertFalse((self.home / "rig3").exists())
        self.assertEqual(self.saved_files(3), {"sys/Prefs/Env-Archive/DCTelnet/DCTelnet.prefs": b"user dctelnet prefs\x00\x01",
                                               "sys/S/Startup-Sequence": b"user startup\n"})
        self.assertIn("[INFO] rig gc: rig 3: saved 2 files", self.out.getvalue())
        self.assertEqual(sorted(p.name for p in (self.home / "rig-saved").iterdir()), ["3", "sys-master.json"],
                         "unpacked scratch left behind")

    def test_master_digests_cached_once(self):
        self.make_rig(3)
        self.make_rig(4)
        self.sweep()
        self.assertEqual(self.unpacks, 3, "master once, then each rig image once")
        self.make_rig(5)
        self.sweep()
        self.assertEqual(self.unpacks, 4, "the cached master digests were not reused")

    def test_image_that_cannot_be_unpacked_keeps_the_folder(self):
        d = self.make_rig(3)
        (d / "sys.hdf").write_bytes(b"not an amiga disk" * 100)
        self.assertEqual(self.sweep(), [])
        self.assertTrue((d / "sys.hdf").is_file())
        self.assertFalse((d / "sys.hdf.unpack").exists())
        self.assertIn("[ERROR] rig gc: could not save", self.err.getvalue())
        self.assertIn("xdftool could not unpack", self.err.getvalue())

    def test_default_sources_name_the_master_image(self):
        self.assertEqual(rig.rig_save_sources(self.home)["sys.hdf"], rig.SRC_HDF)


if __name__ == "__main__":
    unittest.main()
