set -euo pipefail; for f in src/*.c; do [[ -f "$f" ]] && wc -l < "$f"; done | sort -n | tail -3
