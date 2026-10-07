f() { local -r x=1; echo $x; }; f; echo "[$x]"; readonly z=3; f2() { local z=4 2>/dev/null; echo rc=$?; }; f2
