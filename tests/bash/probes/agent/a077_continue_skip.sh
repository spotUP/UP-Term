for f in a.txt nope.txt b.txt; do [ -f "$f" ] || continue; echo "$f ok"; done
