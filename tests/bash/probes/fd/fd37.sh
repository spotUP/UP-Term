exec 3>f.txt; echo a >&3; exec 3>>f.txt; echo b >&3; exec 3>&-; cat f.txt
