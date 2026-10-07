ls >&-; echo "rc=$?"
cat < a.txt <&-; echo rc=$?
