exec 3>o.txt; /bin/sh -c "echo viaext >&3" 2>/dev/null; echo st=$?; exec 3>&-; cat o.txt
