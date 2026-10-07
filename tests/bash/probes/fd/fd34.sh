exec 3>f.txt; fd=3; echo viavar >&$fd; exec 3>&-; cat f.txt
