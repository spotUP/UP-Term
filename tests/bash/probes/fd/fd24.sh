exec 3<>rw.txt; echo data >&3; exec 3>&-; cat rw.txt
