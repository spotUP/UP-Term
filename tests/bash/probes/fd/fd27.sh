exec 3>f.txt; echo a 1>&3 2>&3; exec 3>&-; cat f.txt
