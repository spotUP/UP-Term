f() { echo f; }; unset -f f; f 2>/dev/null; echo rc=$?
