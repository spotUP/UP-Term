#!/usr/bin/env python3
"""One-off generator of dist/amarchy-themes/NAME.theme: each file is Omarchy's themes/NAME/colors.toml, verbatim
(plan thoughts/shared/plans/2026-10-26-amarchy-gui-themes.md, section 2). The output is committed (the install is
offline); run this only to refresh it. Source: https://raw.githubusercontent.com/omacom/omarchy/quattro/themes/NAME/colors.toml"""
import os
import sys
import urllib.request

NAMES = [
    "catppuccin-latte", "catppuccin", "ethereal", "everforest", "flexoki-light", "gruvbox", "hackerman",
    "jade-horse", "kanagawa", "last-horizon", "lumon", "lupine", "matte-black", "miasma", "nord", "retro-82",
    "ristretto", "rose-pine", "solitude", "starship", "tokyo-night", "vantablack", "white",
]
URL = "https://raw.githubusercontent.com/omacom/omarchy/quattro/themes/%s/colors.toml"


def main():
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "dist", "amarchy-themes")
    os.makedirs(out, exist_ok=True)
    for n in NAMES:
        with urllib.request.urlopen(URL % n, timeout=30) as r:
            data = r.read()
        with open(os.path.join(out, n + ".theme"), "wb") as f:
            f.write(data)
        print("[OK] %s (%d bytes)" % (n, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
