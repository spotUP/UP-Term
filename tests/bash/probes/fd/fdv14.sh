exec 3>o.txt; ( echo s >&3 ) 3>p.txt; echo t >&3; exec 3>&-; cat o.txt p.txt
