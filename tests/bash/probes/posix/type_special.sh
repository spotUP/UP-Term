# posix mode: type and command -V call the special builtins special
r() { $BASH --posix -c "$1" 2>/dev/null; echo "st=$?"; }
r 'type : eval export cd echo'
r 'command -V set shift pwd'
r 'command -v export'
$BASH -c 'type : eval; command -V export'
