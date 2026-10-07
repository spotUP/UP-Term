exec 3>o.txt; ( echo s >&3 ) 3>&- 2>/dev/null; echo t >&3; exec 3>&-; cat o.txt
