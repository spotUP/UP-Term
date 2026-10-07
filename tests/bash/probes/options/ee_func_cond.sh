set -e; f() { false; echo in-f; }; if f; then echo yes; fi; f || echo alt; echo done
