read x <&-; echo "rc=$?"
echo hi 2>&-; echo rc=$?
