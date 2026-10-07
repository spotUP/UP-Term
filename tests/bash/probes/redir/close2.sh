echo hi >&-; echo "rc=$?"
{ echo to-err >&2; } 2>&-; echo rc=$?
printf x >&-; echo rc=$?
{ echo a; echo b; } >&-; echo rc=$?
echo ok >&-; echo rc=$?; echo after
