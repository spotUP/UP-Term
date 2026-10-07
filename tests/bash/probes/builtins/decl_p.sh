declare -i -x PV=7; declare -p PV; declare -- PW=a; declare -p PW; declare -p NOPE 2>/dev/null; echo rc=$?; PQ="a b\"c\$d"; declare -p PQ
