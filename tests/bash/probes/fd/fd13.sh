exec 3>f.txt; exec 4>&3; echo via4 >&4; exec 3>&-; echo still4 >&4; exec 4>&-; cat f.txt
