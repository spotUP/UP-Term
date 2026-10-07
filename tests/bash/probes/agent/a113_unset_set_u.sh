set -u; x=1; unset x; (echo "$x") 2>/dev/null; echo rc=$?
