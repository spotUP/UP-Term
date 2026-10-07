exec 3>a3.txt; echo one >&3; echo two >&3; exec 3>&-; cat a3.txt
