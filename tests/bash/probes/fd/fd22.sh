exec 3>f.txt; { echo a; echo b; } >&3; exec 3>&-; cat f.txt
