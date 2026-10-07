times | sed 's/[0-9]/N/g'
f(){ caller; caller 0; caller 1; echo "c=$?"; g; }
g(){ caller 0; caller 1; caller 2; echo "c2=$?"; }
f
caller; echo "top=$?"
ulimit -t; ulimit -f; ulimit -SH -t; ulimit -z 2>/dev/null; echo "u=$?"
enable -n echo; type echo | sed "s|bin:|/bin/|"; enable echo; type echo
enable -n nosuchbi 2>/dev/null; echo "e=$?"
enable -n test; type test | sed "s|bin:|/bin/|"; enable test; type test | sed "s|bin:|/bin/|"
help nosuch 2>/dev/null; echo "h=$?"
select s in a b; do break; done <<< 2; echo "s=$s"
declare -f f g 
h(){ select x in a b c; do echo $x; done; time -p ls; time echo x; time; }
declare -f h
