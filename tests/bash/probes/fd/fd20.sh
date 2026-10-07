exec 3>f.txt; echo "$(echo cap >&3)"; exec 3>&-; cat f.txt
