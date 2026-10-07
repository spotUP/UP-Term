# posix mode: a command substitution inherits set -e
r() { $BASH --posix -c "$1" 2>/dev/null; echo "st=$?"; }
r 'set -e; x=$(false; echo hi); echo "[$x]"'
r 'set -e; echo $(false; echo hi)'
r 'set -e; x=$(true; echo hi); echo "[$x]"'
$BASH -c 'set -e; x=$(false; echo hi); echo "[$x]"'; echo "st=$?"
