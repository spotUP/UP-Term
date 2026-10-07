trap 'echo "D: $BASH_COMMAND"' DEBUG
f() { echo in-f; }
f
set -T
f
trap - DEBUG
f
