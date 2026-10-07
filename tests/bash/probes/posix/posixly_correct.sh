# POSIXLY_CORRECT and set -o posix are one switch: entering sets it to y, leaving unsets it
r() { $BASH -c "$1" 2>/dev/null; echo "st=$?"; }
r 'set -o posix; echo "[$POSIXLY_CORRECT]"; set +o posix; echo "[${POSIXLY_CORRECT-unset}]"'
r 'POSIXLY_CORRECT=; set -o | grep posix; unset POSIXLY_CORRECT; set -o | grep posix'
r 'POSIXLY_CORRECT=z; echo $POSIXLY_CORRECT; set -o | grep posix'
r 'set -o posix; unset -v POSIXLY_CORRECT; set -o | grep posix'
POSIXLY_CORRECT=1 $BASH -c 'set -o | grep posix; echo $POSIXLY_CORRECT; set +o posix; echo "[${POSIXLY_CORRECT-unset}]"'
$BASH --posix -c 'echo "[$POSIXLY_CORRECT]"; set -o | grep posix'
$BASH -c 'set -o posix; FOO=1 :; echo "[$FOO]"; shopt -s nullglob; echo done'
