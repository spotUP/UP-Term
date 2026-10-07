exec 3>f.txt; ( echo sub >&3 ); echo main >&3; exec 3>&-; cat f.txt
