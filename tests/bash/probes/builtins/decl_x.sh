declare -x DX=1; env | grep -c ^DX=; declare +x DX; env | grep -c ^DX=; export EX=2; env | grep -c ^EX=; export -n EX; env | grep -c ^EX=
