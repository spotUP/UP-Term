sleep 0 & p=$!; wait $p; echo rc=$?; [ -n "$p" ] && echo has-pid
