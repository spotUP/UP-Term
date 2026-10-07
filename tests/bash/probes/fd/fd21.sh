f() { echo in-f >&3; }; exec 3>f.txt; f; exec 3>&-; cat f.txt
