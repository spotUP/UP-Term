"""TE-10.6c guard: the role-pen enum of amarchy/flat_role_core.h (FLAT_RP_*) shares no enumerator name with the glyph roles of
amarchy/flat_glyph.h (FLAT_ROLE_*), so flat.c can include both; iconpen_dos.h takes FLAT_RP_COUNT, not its own copy."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def enumerators(path):
    text = re.sub(r"/\*.*?\*/", "", (ROOT / path).read_text(), flags=re.S)
    names = set()
    for body in re.findall(r"\benum\s*\w*\s*\{(.*?)\}", text, flags=re.S):
        for item in body.split(","):
            m = re.match(r"\s*([A-Za-z_]\w*)", item)
            if m:
                names.add(m.group(1))
    return names


class FlatRoleEnumNames(unittest.TestCase):
    def test_no_enumerator_in_both_headers(self):
        both = enumerators("amarchy/flat_glyph.h") & enumerators("amarchy/flat_role_core.h")
        self.assertEqual(sorted(both), [])

    def test_no_ipd_role_count(self):
        me = pathlib.Path(__file__).resolve()
        hits = [str(p) for d in ("amarchy", "tests") for p in (ROOT / d).rglob("*")
                if p.is_file() and p.resolve() != me and p.suffix in (".c", ".h", ".py", ".inc")
                and "IPD_ROLE_COUNT" in p.read_text(errors="replace")]
        self.assertEqual(hits, [])

    def test_flat_c_includes_role_core(self):
        self.assertIn('#include "flat_role_core.h"', (ROOT / "amarchy/flat.c").read_text())


if __name__ == "__main__":
    unittest.main()
