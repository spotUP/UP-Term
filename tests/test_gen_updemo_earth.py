#!/usr/bin/env python3
"""S56-UD2-EARTH-OVER-PHOTO: the earth.anim frames are RGBA with the globe disc opaque and everything outside it
alpha 0, so the fjord photo shows through the corners. Runs the real generator on the real GIF and reads the PNGs back."""
import io, pathlib, struct, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None


@unittest.skipIf(Image is None, "Pillow missing")
class EarthFrames(unittest.TestCase):
    def test_corners_are_transparent_and_centre_opaque(self):
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d) / "earth.anim"
            subprocess.run([sys.executable, "-I", str(ROOT / "tools/gen_updemo_earth.py"),
                            str(ROOT / "demo/media/earth.gif"), str(out), "160"], check=True, capture_output=True)
            data = out.read_bytes()
        self.assertEqual(data[:8], b"UPANIM1\n")
        count = struct.unpack(">I", data[8:12])[0]
        self.assertEqual(count, 44)
        pos = 12
        for _ in range(count):
            _gap, n = struct.unpack(">II", data[pos:pos + 8])
            im = Image.open(io.BytesIO(data[pos + 8:pos + 8 + n]))
            self.assertEqual(im.mode, "RGBA")
            w, h = im.size
            for xy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1), (6, 6), (w - 7, h - 7)):
                self.assertEqual(im.getpixel(xy)[3], 0, xy)
            self.assertEqual(im.getpixel((w // 2, h // 2))[3], 255)
            pos += 8 + n


if __name__ == "__main__":
    unittest.main()
