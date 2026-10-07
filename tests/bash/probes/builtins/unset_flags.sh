f() { echo f; }; v=1; unset -f f; type f >/dev/null 2>&1; echo rc=$?; unset -v v; echo "[$v]"; v=2; unset v; echo "[$v]"
