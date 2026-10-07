trap 'echo "ERR in ${FUNCNAME:-main} status $?"' ERR
f() { false; echo f-continues; }
f
set -E
f
g() { f; }
g
( false )
set +E
f
