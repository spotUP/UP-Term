"""rig.py dopus: the Installer script runs the 3.2 tree's SYS:Install/Installer by its bare name (Status COM sees it)."""
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import dopus_rig


def test_script_adds_install_drawer_then_bare_installer():
    t = dopus_rig.script_text().splitlines()
    assert t[1] == "CD VTCX:" + dopus_rig.TREE
    assert "Path SYS:Install ADD" in t[3]
    # the Installer's startup page edits a scratch file the script creates, never the real S:Startup-Sequence
    assert t[0].startswith("Echo >" + dopus_rig.STARTUP_SEQUENCE)
    assert dopus_rig.STARTUP_SEQUENCE.startswith("RAM:")
    assert t[-1].startswith('Installer "Install DOpus5"')


if __name__ == "__main__":
    test_script_adds_install_drawer_then_bare_installer()
    print("rig_dopus_script_os32 ok")
