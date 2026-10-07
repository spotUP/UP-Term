f() { declare v=1; echo $v; }; f; echo "[$v]"; g() { declare -g gv=2; }; g; echo $gv
