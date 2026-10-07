set -e; f() { g; echo f-after; }; g() { false; echo g-after; }; if f; then echo y; fi; echo end
