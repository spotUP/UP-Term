exec 3<a.txt; while read -u 3 l; do echo "L:$l"; done; exec 3<&-
