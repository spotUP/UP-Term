"""rig.py dopus: the Installer script runs the 3.2 tree's SYS:Install/Installer by its bare name (Status COM sees it)."""
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import dopus_rig


def test_script_adds_install_drawer_then_bare_installer():
    t = dopus_rig.script_text().splitlines()
    assert t[0] == "CD VTCX:" + dopus_rig.TREE
    assert "Path SYS:Install ADD" in t[2]
    assert t[-1].startswith('Installer "Install DOpus5"')
