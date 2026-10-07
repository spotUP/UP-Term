exec 3>f.txt; echo a >&3; exec 3>&-; echo b >&3; echo "st=$?"; cat f.txt
