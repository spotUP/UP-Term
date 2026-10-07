sleep 0.1 & wait $!; echo rc=$?; (exit 3) & wait $!; echo rc=$?
