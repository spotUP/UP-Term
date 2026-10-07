trap 'echo "err-trap"; false; echo err-after' ERR
false
echo next
trap 'echo dbg; true' DEBUG
echo hi
trap - DEBUG
