exec 3>f3.txt; echo hi >&3; exec 3>&-; cat f3.txt
