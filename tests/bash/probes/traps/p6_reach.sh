trap 'echo e' ERR
false
trap - ERR
trap 'echo d' DEBUG
:
trap - DEBUG
f() { :; }
trap 'echo r' RETURN
set -T
f
