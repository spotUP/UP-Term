# posix mode: a function cannot have the name of a special builtin
r() { $BASH --posix -c "$1" 2>/dev/null; echo "st=$?"; }
r 'eval() { :; }; echo after'
r 'function : { :; }; echo after'
r 'cd() { :; }; echo after'
$BASH -c 'eval() { echo mine; }; eval x; echo after'
