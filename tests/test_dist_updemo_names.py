#!/usr/bin/env python3
"""DIST-UPDEMO-COLLISION: no two kit entries in one drawer may differ only by case. AmigaDOS (FFS, PFS) and APFS
ignore case, so the program Files/UPDemo and a media drawer Files/updemo are one name: the drawer replaced the
program and Install's `Copy UPDemo C:UPDemo` found nothing. The media sit in Files/share/updemo (the installed
UP-Term:share/updemo). DIST_ROOT (default: the repo) names the tree to check, to run the test against another checkout."""
import collections, os, pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(os.environ.get("DIST_ROOT", pathlib.Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import mkmanifest

KIT = ROOT / "build/dist/up-term"
MEDIA = ("earth.anim", "kanim-stream.bin", "photo.png")


def case_clashes(paths):
    """[(drawer, [names])] where a drawer holds names equal when lower-cased but not equal: every path
    contributes its own name and each drawer on the way."""
    seen = collections.defaultdict(set)
    for p in paths:
        parts = p.split("/")
        for i, name in enumerate(parts):
            seen[("/".join(parts[:i]).lower(), name.lower())].add((("/".join(parts[:i])), name))
    return sorted((sorted(d for d, _ in v)[0], sorted(n for _, n in v)) for v in seen.values() if len({n for _, n in v}) > 1)


class UpdemoNames(unittest.TestCase):
    def test_parts_txt_has_no_case_insensitive_duplicates(self):
        parts = mkmanifest.read_parts((ROOT / "dist/parts.txt").read_text())
        self.assertEqual(case_clashes([p[1] for p in parts if p[1] != "-"]), [])

    def test_parts_txt_places_the_media_under_share(self):
        paths = [p[1] for p in mkmanifest.read_parts((ROOT / "dist/parts.txt").read_text())]
        self.assertIn("Files/UPDemo", paths)
        self.assertIn("Files/share/updemo", paths)

    def test_clash_finder_sees_the_old_layout(self):
        self.assertEqual(case_clashes(["Files/UPDemo", "Files/updemo"]), [("Files", ["UPDemo", "updemo"])])

    def test_absent_is_exact_case(self):
        with tempfile.TemporaryDirectory() as d:
            kit = pathlib.Path(d)
            (kit / "Files/updemo").mkdir(parents=True)
            parts = [("tools", "Files/UPDemo", None)]
            self.assertEqual(mkmanifest.absent(kit, parts), ["Files/UPDemo"])

    @unittest.skipUnless((KIT / "Files").is_dir(), "no built kit at %s: run make dist" % KIT)
    def test_built_kit_has_program_and_media(self):
        files = os.listdir(KIT / "Files")
        self.assertIn("UPDemo", files, "the program UPDemo is missing from Files")
        self.assertTrue((KIT / "Files/UPDemo").is_file())
        self.assertNotIn("updemo", files, "a Files/updemo drawer would replace the program")
        self.assertEqual(sorted(os.listdir(KIT / "Files/share/updemo")), sorted(MEDIA))
        rels = [str(p.relative_to(KIT)) for p in KIT.rglob("*")]
        self.assertEqual(case_clashes(rels), [])


if __name__ == "__main__":
    unittest.main()
