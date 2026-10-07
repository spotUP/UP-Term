echo "[$(< a.txt)]" "[$(<b.txt)]"
f=nums.txt; echo "[$(< "$f")]"; wc=$(< $f); echo "${#wc}"
echo "[$(< nothere 2>/dev/null)]"; echo rc=$?
echo "[$(< empty.txt)]"
