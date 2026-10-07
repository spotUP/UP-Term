# posix mode: export -p and readonly -p print the commands that recreate the variables
r() { $BASH --posix -c "$1" 2>/dev/null; echo "st=$?"; }
r 'export X=1; export -p | grep " X="'
r 'readonly Y=1; readonly -p | grep " Y="'
r 'export E=; export -p | grep " E="'
r 'export Q="a b"; export -p | grep " Q="; readonly Q; readonly -p | grep " Q="'
r 'X=1; declare -p X'
$BASH -c 'export X=1; export -p | grep " X="; readonly Y=1; readonly -p | grep " Y="'
